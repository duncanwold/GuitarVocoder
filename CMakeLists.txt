cmake_minimum_required(VERSION 3.22)
project(GuitarVocoder VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(JUCE CONFIG REQUIRED)

juce_add_plugin(GuitarVocoder
    COMPANY_NAME "YachtPockets"
    PLUGIN_MANUFACTURER_CODE Ypkt
    PLUGIN_CODE Gvoc
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Guitar Vocoder"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
)

juce_generate_juce_header(GuitarVocoder)

target_sources(GuitarVocoder PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/PresetManager.cpp
    Source/DSP/Vocoder.cpp
)

target_include_directories(GuitarVocoder PRIVATE Source)

target_compile_definitions(GuitarVocoder PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
)

target_link_libraries(GuitarVocoder
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)
