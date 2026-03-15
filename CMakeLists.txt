cmake_minimum_required(VERSION 3.22)
project(Silo VERSION 1.0.0)

add_subdirectory(/Users/abhishekshivakumar/spektra-ui/JUCE JUCE)

juce_add_plugin(Silo
    COMPANY_NAME "Quilio"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Quil
    PLUGIN_CODE Silo
    FORMATS VST3 AU
    PRODUCT_NAME "Silo"
)

juce_generate_juce_header(Silo)

target_sources(Silo PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
)

target_compile_definitions(Silo PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
)

target_link_libraries(Silo PRIVATE
    juce::juce_audio_utils
    juce::juce_audio_plugin_client
    juce::juce_dsp
    juce::juce_opengl
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
