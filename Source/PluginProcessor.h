#pragma once
#include <JuceHeader.h>

/**
 * ============================================================================
 * DynamicConvolutionReverbAudioProcessor
 * ============================================================================
 * 
 * A VST3 audio plugin that combines impulse response convolution (reverb)
 * with real-time dynamic spectral equalization.
 * 
 * MAIN ALGORITHM:
 * 
 * 1. ANALYZE: FFT analysis of incoming dry signal
 *    - Extract magnitude spectrum
 *    - Apply fractional-octave smoothing
 * 
 * 2. INVERT: Create inverse ("opposite") EQ curve
 *    - Inverted magnitude = 1 / magnitude
 *    - Boost weak frequencies, reduce strong ones
 * 
 * 3. CONVOLVE: Apply reverb using impulse response
 *    - User-loaded .wav/.aiff/.flac files
 *    - Stereo convolution support
 * 
 * 4. EQUALIZE: Apply inverse EQ to reverb output
 *    - FIR filtering with dynamically computed coefficients
 *    - Stereo channel support (ProcessorDuplicator)
 * 
 * RESULT: Reverb effect without spectral buildup/muddy sound
 * 
 * USER CONTROLS:
 * - Load IR button: Select impulse response file
 * - Smoothing selector: Choose frequency resolution
 *   * 1/2 octave (broadest)
 *   * 1/4 octave
 *   * 1/8 octave (default)
 *   * 1/16 octave (finest)
 * 
 * ============================================================================
 */
class DynamicConvolutionReverbAudioProcessor  : public juce::AudioProcessor
{
public:
    //--------------------------------------------------------------------------
    // JUCE Mandatory Methods
    //--------------------------------------------------------------------------
    
    /** Constructor: Initialize DSP objects and parameter tree */
    DynamicConvolutionReverbAudioProcessor();
    ~DynamicConvolutionReverbAudioProcessor() override;

    /**
     * Called by host when setting up playback
     * - Host communication: sample rate, buffer size, channel configuration
     */
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    
    /**
     * Called by host when playback stops
     * - Free temporary resources
     */
    void releaseResources() override;
    
    /**
     * Main audio processing callback
     * - Called for each buffer of audio samples
     * - Performs 5-step chain: analysis → reverb → EQ
     */
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    /** Create editor window */
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    // Required by JUCE (boilerplate)
    const juce::String getName() const override { return "Dynamic Reverb"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int index) override {}
    const juce::String getProgramName (int index) override { return {}; }
    void changeProgramName (int index, const juce::String& newName) override {}
    
    /**
     * Serialize plugin state for host save/recall
     * Saves: parameters (smoothing level) + IR file path
     */
    void getStateInformation (juce::MemoryBlock& destData) override;
    
    /**
     * Restore plugin state from saved data
     * Restores: parameters + IR file path (actual IR loading in prepareToPlay)
     */
    void setStateInformation (const void* data, int sizeInBytes) override;

    //--------------------------------------------------------------------------
    // Public API for Editor
    //--------------------------------------------------------------------------
    
    /**
     * Load impulse response from file
     * @param file: Audio file path (.wav, .aiff, .flac, etc)
     */
    void loadImpulseResponseFile (const juce::File& file);
    
    /** Check if an IR has been loaded */
    bool isIrLoaded() const { return irLoaded; }
    
    /** Get currently loaded IR filename */
    juce::String getIrFileName() const { return irFileName; }

private:
    // Allow editor access to private members
    friend class DynamicConvolutionReverbAudioProcessorEditor;

    //--------------------------------------------------------------------------
    // State & Parameters
    //--------------------------------------------------------------------------
    
    /** True if an impulse response was successfully loaded */
    bool irLoaded = false;
    
    /** Full path to currently loaded IR file */
    juce::String irFileName;

    /**
     * Parameter tree: Stores plugin parameters
     * Currently contains: "smoothing" choice parameter
     *   Values: 0 (1/2 oct), 1 (1/4 oct), 2 (1/8 oct), 3 (1/16 oct)
     */
    juce::AudioProcessorValueTreeState parameters;

    //--------------------------------------------------------------------------
    // DSP Objects - Convolution (Reverb)
    //--------------------------------------------------------------------------
    
    /** 
     * JUCE Convolution engine
     * Applies impulse response to audio signal
     * Creates the reverb/spaciousness effect
     */
    juce::dsp::Convolution convolutionReverb;
    
    //--------------------------------------------------------------------------
    // DSP Objects - FFT & Spectral Analysis
    //--------------------------------------------------------------------------
    
    /**
     * FFT Configuration:
     * - Order 11 = 2048 samples
     * - Frequency resolution @ 48kHz: 23.4 Hz (suitable for octave analysis)
     * - Latency: ~21 ms (acceptable for audio effects)
     * - Balance between detail and CPU cost
     */
    static constexpr auto fftOrder = 11;
    static constexpr auto fftSize  = 1 << fftOrder;  // 2048
    
    /** FFT engine: Time domain → Frequency domain */
    juce::dsp::FFT forwardFFT;
    
    /** FFT engine: Frequency domain → Time domain */
    juce::dsp::FFT inverseFFT;
    
    /** 
     * Window function: Hann window
     * Applied before FFT to reduce spectral leakage
     * WHY? FFT assumes signal repeats infinitely; window fades edges
     */
    juce::dsp::WindowingFunction<float> window;

    /** 
     * FFT working buffer: Complex spectrum storage
     * Size = fftSize * 2 (for interleaved complex numbers)
     * Format: [Re0, Im0, Re1, Im1, ..., Re1024, Im1024]
     */
    std::array<float, fftSize * 2> fftData;
    
    /** 
     * IR coefficient buffer: Inverted spectrum impulse response
     * Size = fftSize (real-valued FIR coefficients)
     * These become the dynamic EQ filter coefficients
     */
    std::array<float, fftSize> irData;

    //--------------------------------------------------------------------------
    // DSP Objects - Dynamic EQ Filter
    //--------------------------------------------------------------------------
    
    /**
     * Stereo FIR filter that applies dynamic inverse EQ
     * 
     * ProcessorDuplicator automatically:
     * - Duplicates the mono FIR filter state
     * - Processes left & right channels independently
     * - Maintains stereo separation
     * 
     * Updated every block with newly computed filter coefficients
     */
    juce::dsp::ProcessorDuplicator<juce::dsp::FIR::Filter<float>, juce::dsp::FIR::Coefficients<float>> dynamicEQ;

    //--------------------------------------------------------------------------
    // Helper Methods
    //--------------------------------------------------------------------------
    
    /**
     * Core algorithm: Compute dynamic EQ coefficients
     * 
     * Analyzes incoming audio and creates an inverse magnitude filter
     * that will cancel the dry signal's spectral coloration when applied
     * to the reverb output.
     * 
     * 7-STEP PROCESS:
     * 1. Copy audio channel + apply Hann window
     * 2. Forward FFT: time → frequency domain
     * 3. Extract magnitude spectrum
     * 4. Fractional-octave smoothing (user-selected)
     * 5. Invert magnitude response (1/mag)
     * 6. Inverse FFT: frequency → time domain
     * 7. Update FIR coefficients
     * 
     * @param directBuffer: The dry audio input signal
     */
    void updateDynamicEQ (const juce::AudioBuffer<float>& directBuffer);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DynamicConvolutionReverbAudioProcessor)
};

