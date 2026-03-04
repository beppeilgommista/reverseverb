#include "PluginEditor.h"

/**
 * Constructor: Initialize the plugin editor with UI components
 * 
 * Steps:
 * 1. Call AudioProcessorEditor constructor with processor reference
 * 2. Add and link the Load IR button
 * 3. Add smoothing combobox with 4 frequency resolution options
 * 4. Create attachment to sync combobox with processor parameter tree
 * 5. Add status label showing current IR file
 * 6. Set window size to 400x160 pixels
 */
DynamicConvolutionReverbAudioProcessorEditor::DynamicConvolutionReverbAudioProcessorEditor (DynamicConvolutionReverbAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    // Add the "Load IR…" button and listen for clicks
    addAndMakeVisible (loadButton);
    loadButton.addListener (this);

    // Add smoothing selector combobox with 4 options
    smoothingBox.addItemList ({ "1/2 octave", "1/4 octave", "1/8 octave", "1/16 octave" }, 1);
    addAndMakeVisible (smoothingBox);
    
    // Create attachment: automatically maps combobox selection to processor.parameters["smoothing"]
    // This ensures the parameter value is updated whenever the combobox is moved
    smoothingAttachment.reset (new juce::AudioProcessorValueTreeState::ComboBoxAttachment (
                                    processor.parameters, "smoothing", smoothingBox));

    // Add status label showing the loaded IR filename
    addAndMakeVisible (statusLabel);
    statusLabel.setText (processor.isIrLoaded() ? processor.getIrFileName() : "<no IR loaded>",
                         juce::dontSendNotification);
    statusLabel.setJustificationType (juce::Justification::centred);

    setSize (400, 160);
}

DynamicConvolutionReverbAudioProcessorEditor::~DynamicConvolutionReverbAudioProcessorEditor() {}

/**
 * Paint callback: Draw the UI background
 * 
 * Keep it simple - focus on audio processing, not fancy graphics.
 */
void DynamicConvolutionReverbAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkgrey);
}

/**
 * Resize callback: Position all UI components
 * 
 * Layout (top to bottom):
 * 1. Load IR button (30px height)
 * 2. Smoothing combobox (30px height)
 * 3. Status label (25px height)
 * 
 * All components have 10px margin from edges
 */
void DynamicConvolutionReverbAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    loadButton.setBounds (area.removeFromTop (30));
    smoothingBox.setBounds (area.removeFromTop (30));
    statusLabel.setBounds (area.removeFromTop (25));
}

/**
 * Button click handler: Triggered when user clicks "Load IR…"
 * 
 * Opens a file chooser dialog (async) to select audio files.
 * On selection:
 * 1. Get the selected file
 * 2. Check it exists
 * 3. Call processor.loadImpulseResponseFile() to load it
 * 4. Update statusLabel with the filename
 */
void DynamicConvolutionReverbAudioProcessorEditor::buttonClicked (juce::Button* b)
{
    if (b == &loadButton)
    {
        // Create file chooser filtered to audio formats
        juce::FileChooser chooser ("Select an impulse response file",
                                   juce::File(),
                                   "*.wav;*.aiff;*.flac");
        
        // Open chooser asynchronously (non-blocking) with callback
        chooser.launchAsync (juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectFiles,
                            [this] (const juce::FileChooser& fc)
                            {
                                auto file = fc.getResult();
                                if (file.existsAsFile())
                                {
                                    // Pass file to processor for loading
                                    processor.loadImpulseResponseFile (file);
                                    // Update UI to show new filename
                                    statusLabel.setText (file.getFileName(), juce::dontSendNotification);
                                }
                            });
    }
}

