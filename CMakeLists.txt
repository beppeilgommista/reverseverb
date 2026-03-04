cmake_minimum_required(VERSION 3.15)
project(DynamicConvolutionReverb VERSION 1.0.0)

# Find JUCE (Assuming it's installed or added as a submodule)
# If you haven't, add JUCE as a git submodule in a /JUCE folder
add_subdirectory(JUCE) 

juce_add_plugin(DynamicConvolutionReverb
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    FORMATS VST3            # only VST3, standalone and AU removed per user request
    PRODUCT_NAME "DynamicReverb"
    PLUGIN_CODE "Dcrv"
)

# Generate the JUCE header that aggregates module includes; required for <JuceHeader.h>
juce_generate_juce_header(DynamicConvolutionReverb)

target_sources(DynamicConvolutionReverb PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginProcessor.h
    Source/PluginEditor.cpp
    Source/PluginEditor.h
)

target_compile_definitions(DynamicConvolutionReverb PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_IGNORE_VST3_MISMATCHED_PARAMETER_ID_WARNING=1
)

# Crucial: Link the DSP module
target_link_libraries(DynamicConvolutionReverb PRIVATE
    juce::juce_audio_utils
    juce::juce_dsp
    asound
)