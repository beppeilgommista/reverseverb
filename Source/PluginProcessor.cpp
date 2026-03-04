#include "PluginProcessor.h"
#include "PluginEditor.h"

/**
 * ============================================================================
 * Constructor
 * ============================================================================
 * 
 * Initialize DSP objects and parameters
 * 
 * Init list:
 * - AudioProcessor: Stereo I/O bus configuration
 * - forwardFFT/inverseFFT: Real FFT engines (order 11 = 2048 bins)
 * - window: Hann windowing function for FFT (reduces spectral leakage)
 * - parameters: AudioProcessorValueTreeState with smoothing choice parameter
 *   * Options: 1/2 octave, 1/4 octave, 1/8 octave (default), 1/16 octave
 */
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
    // All initialization done in member initializer list
}

DynamicConvolutionReverbAudioProcessor::~DynamicConvolutionReverbAudioProcessor() {}


/**
 * Load impulse response from a file
 * 
 * @param file: Audio file to load (.wav, .aiff, .flac, etc)
 * 
 * Passes to convolution engine with parameters:
 * - Stereo::yes: Use stereo if IR is stereo
 * - Trim::no: Keep full IR without trimming silence
 * - Normalise::yes: Auto-scale IR to prevent clipping
 */
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


/**
 * Serialize plugin state for saving/recall
 * 
 * Saves:
 * 1. Parameter tree as XML (includes smoothing level)
 * 2. IR file path (for auto-reload)
 */
void DynamicConvolutionReverbAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    juce::MemoryOutputStream mo (destData, true);
    
    // Serialize parameter state as XML
    if (auto xml = parameters.copyState().createXml())
        mo.writeString (xml->toString());
    
    // Serialize IR file path
    mo.writeString (irFileName);
}


/**
 * Restore plugin state from saved data
 * 
 * Called by host when loading a preset/session
 * Restores:
 * 1. Parameter state (smoothing level)
 * 2. IR file path (actual IR loading happens in prepareToPlay)
 */
void DynamicConvolutionReverbAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    juce::MemoryInputStream mi (data, static_cast<size_t> (sizeInBytes), false);
    
    // Restore parameter state
    auto xmlString = mi.readString();
    if (xmlString.isNotEmpty())
    {
        if (auto xml = juce::XmlDocument::parse (xmlString))
            parameters.replaceState (juce::ValueTree::fromXml (*xml));
    }

    // Restore IR file path
    auto saved = mi.readString();
    if (saved.isNotEmpty())
    {
        irFileName = saved;
        irLoaded = true;
        // Actual IR loading will happen in prepareToPlay() with proper DSP specs
    }
}


/**
 * ============================================================================
 * prepareToPlay()
 * ============================================================================
 * 
 * Initialize DSP engines when playback starts
 * 
 * Called when:
 * - Host starts playback
 * - Sample rate changes
 * - Buffer size changes
 * 
 * Steps:
 * 1. Create ProcessSpec with host settings
 * 2. Prepare convolution reverb engine
 * 3. Prepare dynamic EQ (FIR filter) engine
 * 4. Initialize EQ with unity filter (no change)
 * 5. Reload IR if one was saved/restored
 */
void DynamicConvolutionReverbAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    // Create DSP spec from host settings
    juce::dsp::ProcessSpec spec;
    spec.sampleRate = sampleRate;
    spec.maximumBlockSize = samplesPerBlock;
    spec.numChannels = getTotalNumOutputChannels();

    // Prepare DSP engines with new spec
    convolutionReverb.prepare (spec);
    dynamicEQ.prepare (spec);

    // Initialize dynamic EQ with unity filter (1.0 = no change)
    // This will be replaced every block by computed inverse magnitude
    static float unity = 1.0f;
    dynamicEQ.state = new juce::dsp::FIR::Coefficients<float> (&unity, 1);

    // Reload IR if it was saved/restored and file path is valid
    if (irLoaded && irFileName.isNotEmpty())
        convolutionReverb.loadImpulseResponse (juce::File (irFileName),
                                              juce::dsp::Convolution::Stereo::yes,
                                              juce::dsp::Convolution::Trim::no,
                                              0,
                                              juce::dsp::Convolution::Normalise::yes);
}


/**
 * Release resources when playback stops
 */
void DynamicConvolutionReverbAudioProcessor::releaseResources()
{
    // Free any temporary audio buffers
}


/**
 * ============================================================================
 * processBlock() - MAIN AUDIO PROCESSING
 * ============================================================================
 * 
 * Called by the host for each audio buffer
 * 
 * 5-STEP AUDIO CHAIN:
 * 
 * 1. CLEANUP: Clear unused output channels
 * 2. ANALYSIS: Update dynamic EQ based on dry signal spectrum
 * 3. REVERB: Apply convolution if IR is loaded
 * 4. EQ: Apply inverse EQ to compensate for dry signal coloration
 * 5. OUTPUT: Send processed audio to host
 * 
 * Result: Clean reverb without "muddy" spectral buildup
 */
void DynamicConvolutionReverbAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    // Prevent CPU slowdown from denormal floating-point numbers
    juce::ScopedNoDenormals noDenormals;
    
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    // Clear output channels not fed by inputs
    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    // === STEP 1: SPECTRUM ANALYSIS ===
    // Copy dry signal for analysis before processing
    juce::AudioBuffer<float> dry (buffer);
    
    // Compute inverse magnitude response and update dynamic EQ coefficients
    // This analyzes the incoming signal and prepares a compensating filter
    updateDynamicEQ (dry);

    // === STEP 2: CONVOLUTION REVERB ===
    // Convert buffer to DSP-friendly AudioBlock format
    juce::dsp::AudioBlock<float> block (buffer);
    juce::dsp::ProcessContextReplacing<float> context (block);

    // Apply reverb if an IR is loaded, otherwise signal passes through
    if (irLoaded)
        convolutionReverb.process (context);

    // === STEP 3: DYNAMIC EQ ===
    // Apply inverse-spectrum FIR filter to balance the output
    // This removes the dry signal's coloration from the reverb,
    // creating a clean, non-muddy effect
    juce::dsp::ProcessContextReplacing<float> eqContext (block);
    dynamicEQ.process (eqContext);
}


/**
 * Create the editor window
 */
juce::AudioProcessorEditor* DynamicConvolutionReverbAudioProcessor::createEditor()
{
    return new DynamicConvolutionReverbAudioProcessorEditor (*this);
}


/**
 * ============================================================================
 * updateDynamicEQ() - CORE ALGORITHM
 * ============================================================================
 * 
 * This is the heart of the plugin!
 * 
 * Analyzes the incoming signal's spectrum and computes an INVERSE filter
 * that will cancel the signal's original coloration when applied to reverb.
 * 
 * STEP-BY-STEP PROCESS:
 * 
 * STEP 1: COPY & WINDOW
 *   - Copy first audio channel to FFT buffer
 *   - Apply Hann window function
 *   - WHY? Reduces spectral leakage at bin edges
 * 
 * STEP 2: FORWARD FFT
 *   - Convert time-domain audio → frequency-domain complex numbers
 *   - Output format: [Re0, Im0, Re1, Im1, ..., Re1024, Im1024]
 * 
 * STEP 3: MAGNITUDE EXTRACTION
 *   - Extract magnitude from each complex bin: mag = sqrt(Re^2 + Im^2)
 *   - Result: Array of positive magnitudes (0 Hz to Nyquist)
 * 
 * STEP 4: FRACTIONAL-OCTAVE SMOOTHING
 *   - Apply moving average with window size based on user's choice:
 *     * 1/2 octave  → 2 bins    (broadest, least aggressive)
 *     * 1/4 octave  → 4 bins
 *     * 1/8 octave  → 8 bins    (default, fine detail)
 *     * 1/16 octave → 16 bins   (finest, most aggressive)
 *   - WHY? Human hearing is logarithmic (octave-based), not linear
 *   - Result: Smooth magnitude curve matching human perception
 * 
 * STEP 5: INVERT SPECTRUM
 *   - Compute inverse: inverted[i] = 1.0 / smoothMag[i]
 *   - This creates a "opposite" EQ that boosts weak frequencies
 *     and reduces strong ones
 *   - Convert to complex form: Re = inverted, Im = 0 (no phase shift)
 * 
 * STEP 6: INVERSE FFT
 *   - Convert frequency-domain inverse back to time domain
 *   - Result: FIR filter coefficients (impulse response)
 * 
 * STEP 7: UPDATE FILTER
 *   - Create new FIR::Coefficients from computed impulse
 *   - Assign to dynamicEQ filter
 *   - Will be applied in next processBlock() call
 * 
 * FINAL RESULT:
 * When this filter is applied to the reverb output, it cancels the dry
 * signal's coloration, creating a clean, balanced reverberant effect
 * without "muddy" spectral buildup.
 */
void DynamicConvolutionReverbAudioProcessor::updateDynamicEQ (const juce::AudioBuffer<float>& directBuffer)
{
    // === STEP 1: COPY & WINDOW ===
    // Clear FFT working buffer
    std::fill (fftData.begin(), fftData.end(), 0.0f);
    
    // Copy first channel (mono) from input
    auto* readPtr = directBuffer.getReadPointer (0);
    auto num = juce::jmin ((int) fftSize, directBuffer.getNumSamples());
    std::memcpy (fftData.data(), readPtr, num * sizeof (float));

    // Apply Hann window (reduces spectral leakage at bin boundaries)
    window.multiplyWithWindowingTable (fftData.data(), fftSize);
    
    // === STEP 2: FORWARD FFT ===
    // Convert time → frequency domain
    forwardFFT.performRealOnlyForwardTransform (fftData.data());

    // === STEP 3: MAGNITUDE EXTRACTION ===
    // Complex bins stored as [Re0, Im0, Re1, Im1, ...]
    int bins = fftSize / 2;
    std::vector<float> mags (bins);
    for (int i = 0; i < bins; ++i)
    {
        float re = fftData[2 * i];
        float im = fftData[2 * i + 1];
        // Magnitude = sqrt(Re^2 + Im^2) + epsilon to prevent div-by-zero
        mags[i] = std::sqrt (re * re + im * im) + 1e-12f;
    }

    // === STEP 4: FRACTIONAL-OCTAVE SMOOTHING ===
    // Get user's smoothing choice (0=1/2, 1=1/4, 2=1/8, 3=1/16 octave)
    int smoothingIndex = (int) *parameters.getRawParameterValue ("smoothing");
    // Convert to bin count: 0→2, 1→4, 2→8, 3→16
    int smoothBins = 1 << (smoothingIndex + 1);

    // Apply moving average
    std::vector<float> smoothMags (bins);
    for (int i = 0; i < bins; ++i)
    {
        // Calculate averaging window around this bin
        int start = juce::jmax (0, i - smoothBins / 2);
        int end   = juce::jmin (bins - 1, i + smoothBins / 2);
        
        // Average magnitudes in window
        float sum = 0;
        for (int j = start; j <= end; ++j)
            sum += mags[j];
        smoothMags[i] = sum / (end - start + 1);
    }

    // === STEP 5: INVERT SPECTRUM ===
    // Clear FFT buffer for inverse operation
    std::fill (fftData.begin(), fftData.end(), 0.0f);
    
    // Set DC and Nyquist (special handling for real-only FFT)
    fftData[0] = 1.0f / smoothMags[0];
    fftData[1] = 1.0f / smoothMags[bins - 1];
    
    // Invert all other bins (1 / magnitude, phase = 0)
    for (int i = 1; i < bins; ++i)
    {
        float val = 1.0f / smoothMags[i];
        fftData[2 * i]     = val;   // Real = inverted magnitude
        fftData[2 * i + 1] = 0.0f;  // Imag = 0 (no phase shift)
    }

    // === STEP 6: INVERSE FFT ===
    // Convert frequency-domain inverse back to time domain
    inverseFFT.performRealOnlyInverseTransform (fftData.data());

    // Copy result to IR storage for coefficient creation
    for (int i = 0; i < fftSize; ++i)
        irData[i] = fftData[i];

    // === STEP 7: UPDATE FILTER ===
    // Create FIR coefficients from computed impulse
    auto coeffs = new juce::dsp::FIR::Coefficients<float> (irData.data(), fftSize);
    
    // Assign to dynamic EQ filter
    // Will be applied to audio in next processBlock() call
    dynamicEQ.state = coeffs;
}


/**
 * Factory function: Plugin host calls this to instantiate the plugin
 */
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new DynamicConvolutionReverbAudioProcessor();
}

