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
       window (fftSize, juce::dsp::WindowingFunction<float>::hann)
#endif
{
}

DynamicConvolutionReverbAudioProcessor::~DynamicConvolutionReverbAudioProcessor() {}

void DynamicConvolutionReverbAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Initialize DSP specs
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getTotalNumOutputChannels();

    // Initialize the convolution engine
    convolutionReverb.prepare (spec);
    
    // TODO: Load default Impulse Response here using convolutionReverb.loadImpulseResponse()
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

    // --- STEP 1: FFT Analysis ---
    // (Here you will fill the fftData array with incoming audio and apply the window function)
    
    // --- STEP 2: Fractional Octave Smoothing & Inverse Curve ---
    // (Here you will analyze the fftData, group by octaves, and invert the curve)
    
    // --- STEP 3: Inverse FFT to generate new FIR Filter ---
    // (Here you will run inverseFFT.performRealOnlyInverseTransform on your new curve)
    
    // --- STEP 4: Convolution Processing ---
    // Pass the audio block into the JUCE dsp::AudioBlock format
    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);
    
    // Apply the Reverb
    convolutionReverb.process (context);

    // --- STEP 5: Apply Dynamic FIR Filter ---
    // (Apply your dynamically generated FIR filter to the output here)
}

juce::AudioProcessorEditor* DynamicConvolutionReverbAudioProcessor::createEditor()
{
    // Returns a generic editor for now. We will build the actual GUI later.
    return new juce::GenericAudioProcessorEditor (*this);
}