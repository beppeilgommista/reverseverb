#pragma once
#include <JuceHeader.h>

class DynamicConvolutionReverbAudioProcessor  : public juce::AudioProcessor
{
public:
    DynamicConvolutionReverbAudioProcessor();
    ~DynamicConvolutionReverbAudioProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Dynamic Reverb"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int index) override {}
    const juce::String getProgramName (int index) override { return {}; }
    void changeProgramName (int index, const juce::String& newName) override {}
    void getStateInformation (juce::MemoryBlock& destData) override {}
    void setStateInformation (const void* data, int sizeInBytes) override {}

private:
    // DSP Objects
    juce::dsp::Convolution convolutionReverb;
    
    // FFT size of 2048 (11th order) is a good starting point for resolution vs CPU
    static constexpr auto fftOrder = 11;
    static constexpr auto fftSize  = 1 << fftOrder;
    
    juce::dsp::FFT forwardFFT;
    juce::dsp::FFT inverseFFT;
    juce::dsp::WindowingFunction<float> window;

    // Buffers for FFT processing
    std::array<float, fftSize * 2> fftData;
    std::array<float, fftSize> irData;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DynamicConvolutionReverbAudioProcessor)
};