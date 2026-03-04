#include "PluginProcessor.h"
#include "PluginEditor.h"

DynamicConvolutionReverbAudioProcessor::DynamicConvolutionReverbAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
     : AudioProcessor (BusesProperties()
                     .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                     .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                       ),
       forwardFFT (fftOrder),
       inverseFFT (fftOrder),
       window (fftSize, juce::dsp::WindowingFunction<float>::hann),
       parameters (*this, nullptr, "PARAMETERS",
                   { std::make_unique<juce::AudioParameterChoice> ("smoothing",
                                                                  "Smoothing",
                                                                  juce::StringArray { "1/2 octave", "1/4 octave", "1/8 octave", "1/16 octave" },
                                                                  2) })
#endif
{
    // nothing else yet
}

DynamicConvolutionReverbAudioProcessor::~DynamicConvolutionReverbAudioProcessor() {}


void DynamicConvolutionReverbAudioProcessor::loadImpulseResponseFile (const juce::File& file)
{
    if (file.existsAsFile())
    {
        convolutionReverb.loadImpulseResponse (file,
                                              juce::dsp::Convolution::Stereo::yes,
                                              juce::dsp::Convolution::Trim::no,
                                              0,
                                              juce::dsp::Convolution::Normalise::yes);
        irLoaded = true;
        irFileName = file.getFullPathName();
    }
}


void DynamicConvolutionReverbAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream mo (destData, true);
    // write parameter state as XML
    if (auto xml = parameters.copyState().createXml())
        mo.writeString (xml->toString());
    // then IR filename
    mo.writeString (irFileName);
}


void DynamicConvolutionReverbAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::MemoryInputStream mi (data, static_cast<size_t> (sizeInBytes), false);
    auto xmlString = mi.readString();
    if (xmlString.isNotEmpty())
    {
        if (auto xml = juce::XmlDocument::parse (xmlString))
            parameters.replaceState (juce::ValueTree::fromXml (*xml));
    }

    auto saved = mi.readString();
    if (saved.isNotEmpty())
    {
        irFileName = saved;
        irLoaded = true;
        // actual loading will happen in prepareToPlay when we have a proper spec
    }
}

void DynamicConvolutionReverbAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Initialize DSP specs
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getTotalNumOutputChannels();

    // Initialize the convolution engine
    convolutionReverb.prepare (spec);
    dynamicEQ.prepare (spec);

    // default eq is identity (one-tap filter)
    {
        static float unity = 1.0f;
        dynamicEQ.state = new juce::dsp::FIR::Coefficients<float> (&unity, 1);
    }

    // if an IR was remembered during state restore, reload it now
    if (irLoaded && irFileName.isNotEmpty())
        convolutionReverb.loadImpulseResponse (juce::File (irFileName),
                                              juce::dsp::Convolution::Stereo::yes,
                                              juce::dsp::Convolution::Trim::no,
                                              0,
                                              juce::dsp::Convolution::Normalise::yes);
}

void DynamicConvolutionReverbAudioProcessor::releaseResources()
{
    // Free up resources when playback stops
}

void DynamicConvolutionReverbAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // copy dry signal and analyze it
    juce::AudioBuffer<float> dry (buffer);
    updateDynamicEQ (dry);

    // Pass the audio block into the JUCE dsp::AudioBlock format
    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);

    // Apply the Reverb (only if an IR has been loaded, otherwise let audio pass through)
    if (irLoaded)
        convolutionReverb.process (context);

    // Apply the dynamic equaliser to whatever is in the buffer now
    juce::dsp::ProcessContextReplacing<float> eqContext (block);
    dynamicEQ.process (eqContext);
}

juce::AudioProcessorEditor* DynamicConvolutionReverbAudioProcessor::createEditor()
{
    // custom editor implemented in PluginEditor.*
    return new DynamicConvolutionReverbAudioProcessorEditor (*this);
}


// helper that analyses a block of audio and updates the FIR coefficients
void DynamicConvolutionReverbAudioProcessor::updateDynamicEQ (const juce::AudioBuffer<float>& directBuffer)
{
    // mix to mono into fftData
    std::fill (fftData.begin(), fftData.end(), 0.0f);
    auto* readPtr = directBuffer.getReadPointer (0);
    auto num = juce::jmin ((int) fftSize, directBuffer.getNumSamples());
    std::memcpy (fftData.data(), readPtr, num * sizeof (float));

    window.multiplyWithWindowingTable (fftData.data(), fftSize);
    forwardFFT.performRealOnlyForwardTransform (fftData.data());

    int bins = fftSize / 2;
    std::vector<float> mags (bins);
    for (int i = 0; i < bins; ++i)
    {
        float re = fftData[2 * i];
        float im = fftData[2 * i + 1];
        mags[i] = std::sqrt (re * re + im * im) + 1e-12f;
    }

    int smoothingIndex = (int) *parameters.getRawParameterValue ("smoothing");
    int smoothBins = 1 << (smoothingIndex + 1); // 0->2,1->4,...

    std::vector<float> smoothMags (bins);
    for (int i = 0; i < bins; ++i)
    {
        int start = juce::jmax (0, i - smoothBins / 2);
        int end   = juce::jmin (bins - 1, i + smoothBins / 2);
        float sum = 0;
        for (int j = start; j <= end; ++j)
            sum += mags[j];
        smoothMags[i] = sum / (end - start + 1);
    }

    // build inverse magnitude spectrum
    std::fill (fftData.begin(), fftData.end(), 0.0f);
    fftData[0] = 1.0f / smoothMags[0];
    fftData[1] = 1.0f / smoothMags[bins - 1];
    for (int i = 1; i < bins; ++i)
    {
        float val = 1.0f / smoothMags[i];
        fftData[2 * i]     = val;
        fftData[2 * i + 1] = 0.0f;
    }

    inverseFFT.performRealOnlyInverseTransform (fftData.data());

    for (int i = 0; i < fftSize; ++i)
        irData[i] = fftData[i];

    auto coeffs = new juce::dsp::FIR::Coefficients<float> (irData.data(), fftSize);
    dynamicEQ.state = coeffs;
}

// Required factory function for JUCE plugin instantiation
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DynamicConvolutionReverbAudioProcessor();
}