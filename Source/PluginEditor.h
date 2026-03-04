#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

// A very simple editor: a button to load an impulse response and a label
// to show the current file name.  The generic editor previously used
// showed nothing because our processor had no parameters, so the UI was
// a plain grey rectangle.

class DynamicConvolutionReverbAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                                       private juce::Button::Listener
{
public:
    DynamicConvolutionReverbAudioProcessorEditor (DynamicConvolutionReverbAudioProcessor&);
    ~DynamicConvolutionReverbAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    // Button listener callback
    void buttonClicked (juce::Button* b) override;

    DynamicConvolutionReverbAudioProcessor& processor;

    juce::TextButton loadButton { "Load IR…" };
    juce::ComboBox smoothingBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> smoothingAttachment;
    juce::Label statusLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DynamicConvolutionReverbAudioProcessorEditor)
};

