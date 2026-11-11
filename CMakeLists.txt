cmake_minimum_required(VERSION 3.22)
project(RBA6A VERSION 0.1.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS")

# JUCE: use ./JUCE if present, otherwise fetch it.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/JUCE/CMakeLists.txt")
    add_subdirectory(JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.4
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

set(RBA_FORMATS VST3)
if(APPLE)
    list(APPEND RBA_FORMATS AU)
endif()

juce_add_plugin(RBA6A
    COMPANY_NAME "Chris"
    BUNDLE_ID "com.chris.rba6a"
    PLUGIN_MANUFACTURER_CODE Chrs
    PLUGIN_CODE Rb6A
    FORMATS ${RBA_FORMATS}
    PRODUCT_NAME "RBA-6A"
    DESCRIPTION "Vari-mu push-pull limiting amplifier"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES Fx Dynamics
    AU_MAIN_TYPE kAudioUnitType_Effect
    COPY_PLUGIN_AFTER_BUILD FALSE)

target_sources(RBA6A PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(RBA6A PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(RBA6A
    PRIVATE juce::juce_audio_utils juce::juce_audio_processors juce::juce_dsp
    PUBLIC juce::juce_recommended_config_flags juce::juce_recommended_lto_flags juce::juce_recommended_warning_flags)

# Local test (not built on GitHub): cmake -DRBA_BUILD_TESTS=ON
option(RBA_BUILD_TESTS "Build the local plug-in test" OFF)
if(RBA_BUILD_TESTS)
    juce_add_console_app(RbaPluginTest PRODUCT_NAME "RbaPluginTest")
    target_sources(RbaPluginTest PRIVATE tests/plugin_test.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
    target_compile_definitions(RbaPluginTest PRIVATE JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 JucePlugin_Name="RBA-6A"
        JucePlugin_Build_Standalone=0)
    target_link_libraries(RbaPluginTest PRIVATE juce::juce_audio_utils juce::juce_audio_processors juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
