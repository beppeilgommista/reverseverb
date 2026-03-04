#include "PluginEditor.h"

DynamicConvolutionReverbAudioProcessorEditor::DynamicConvolutionReverbAudioProcessorEditor (DynamicConvolutionReverbAudioProcessor& p)
    : AudioProcessorEditor (&p), processor (p)
{
    addAndMakeVisible (loadButton);
    loadButton.addListener (this);

    smoothingBox.addItemList ({ "1/2 octave", "1/4 octave", "1/8 octave", "1/16 octave" }, 1);
    addAndMakeVisible (smoothingBox);
    smoothingAttachment.reset (new juce::AudioProcessorValueTreeState::ComboBoxAttachment (
                                    processor.parameters, "smoothing", smoothingBox));

    addAndMakeVisible (statusLabel);
    statusLabel.setText (processor.isIrLoaded() ? processor.getIrFileName() : "<no IR loaded>",
                         juce::dontSendNotification);
    statusLabel.setJustificationType (juce::Justification::centred);

    setSize (400, 160);
}

DynamicConvolutionReverbAudioProcessorEditor::~DynamicConvolutionReverbAudioProcessorEditor() {}

void DynamicConvolutionReverbAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colours::darkgrey);
}

void DynamicConvolutionReverbAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (10);
    loadButton.setBounds (area.removeFromTop (30));
    smoothingBox.setBounds (area.removeFromTop (30));
    statusLabel.setBounds (area.removeFromTop (25));
}

void DynamicConvolutionReverbAudioProcessorEditor::buttonClicked (juce::Button* b)
{
    if (b == &loadButton)
    {
        juce::FileChooser chooser ("Select an impulse response file",
                                   juce::File(),
                                   "*.wav;*.aiff;*.flac");
        chooser.launchAsync (juce::FileBrowserComponent::openMode
                                | juce::FileBrowserComponent::canSelectFiles,
                            [this] (const juce::FileChooser& fc)
                            {
                                auto file = fc.getResult();
                                if (file.existsAsFile())
                                {
                                    processor.loadImpulseResponseFile (file);
                                    statusLabel.setText (file.getFileName(), juce::dontSendNotification);
                                }
                            });
    }
}

