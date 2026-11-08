cmake_minimum_required(VERSION 3.22)
project(Q45Monitor VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "")

# JUCE is downloaded automatically the first time you configure.
include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.4
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Q45Monitor
    COMPANY_NAME "Personal"
    PLUGIN_MANUFACTURER_CODE Pers
    PLUGIN_CODE Q45m
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Q45 Monitor"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD TRUE)

target_sources(Q45Monitor PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(Q45Monitor PRIVATE Source)

target_compile_definitions(Q45Monitor PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0)

target_link_libraries(Q45Monitor
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Plain C++ unit tests for the parser and the crossfeed maths (no JUCE needed).
add_executable(q45_core_tests tests/test_core.cpp)
target_include_directories(q45_core_tests PRIVATE Source)
