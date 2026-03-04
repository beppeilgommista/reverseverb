#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

/**
 * ============================================================================
 * DynamicConvolutionReverbAudioProcessorEditor
 * ============================================================================
 * 
 * The UI editor for the Dynamic Reverb plugin.
 * 
 * UI Components:
 * 1. loadButton: Opens file chooser to load IR files (.wav, .aiff, .flac)
 * 2. smoothingBox: ComboBox to select frequency smoothing resolution
 *    - 1/2 octave: Broadest smoothing (2 bins)
 *    - 1/4 octave: Medium smoothing (4 bins)
 *    - 1/8 octave: Finer smoothing (8 bins, default)
 *    - 1/16 octave: Finest smoothing (16 bins)
 * 3. statusLabel: Displays the name of the currently loaded IR file
 * 
 * The editor uses ComboBoxAttachment to automatically sync the frequency
 * smoothing choice with the processor's parameter tree.
 * 
 * ============================================================================
 */

class DynamicConvolutionReverbAudioProcessorEditor  : public juce::AudioProcessorEditor,
                                                       private juce::Button::Listener
{
public:
    /** Constructor: Initialize UI components and link to processor */
    DynamicConvolutionReverbAudioProcessorEditor (DynamicConvolutionReverbAudioProcessor&);
    ~DynamicConvolutionReverbAudioProcessorEditor() override;

    /** Called when the plugin window needs to be redrawn */
    void paint (juce::Graphics&) override;
    
    /** Called when the window is resized - positions all UI components */
    void resized() override;

private:
    /** Button listener callback - triggered when user clicks "Load IR…" */
    void buttonClicked (juce::Button* b) override;

    /** Reference to the processor - allows access to DSP objects and parameters */
    DynamicConvolutionReverbAudioProcessor& processor;

    /** Button to load impulse response file */
    juce::TextButton loadButton { "Load IR…" };
    
    /** ComboBox to select smoothing level (1/2, 1/4, 1/8, 1/16 octave) */
    juce::ComboBox smoothingBox;
    
    /** Attachment that automatically syncs smoothingBox with processor.parameters */
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> smoothingAttachment;
    
    /** Label showing current IR filename or status */
    juce::Label statusLabel;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (DynamicConvolutionReverbAudioProcessorEditor)
};

