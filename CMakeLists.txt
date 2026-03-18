cmake_minimum_required(VERSION 3.22)
project(Silo VERSION 1.0.0)

if(NOT DEFINED JUCE_PATH)
    set(JUCE_PATH "/Users/abhishekshivakumar/spektra-ui/JUCE")
endif()
add_subdirectory(${JUCE_PATH} JUCE)

# Moonbase licensing SDK (must come after JUCE)
add_subdirectory(Libs/moonbase_JUCEClient moonbase_JUCEClient)

juce_add_binary_data(SiloLicenseData
    SOURCES Assets/moonbase_api_config.json
            Assets/quilio_Q.png
)
set_target_properties(SiloLicenseData PROPERTIES POSITION_INDEPENDENT_CODE TRUE)

if(WIN32)
    set(_FORMATS VST3 Standalone)
else()
    set(_FORMATS VST3 AU Standalone)
endif()

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
    FORMATS ${_FORMATS}
    PRODUCT_NAME "Silo"
    ICON_BIG                "${CMAKE_CURRENT_SOURCE_DIR}/Assets/icon.jpg"
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
    juce::juce_product_unlocking
    moonbase_JUCEClient
    SiloLicenseData
    PUBLIC
    juce::juce_recommended_config_flags
    juce::juce_recommended_lto_flags
    juce::juce_recommended_warning_flags
)
