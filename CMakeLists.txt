cmake_minimum_required(VERSION 3.22)
project(Orbit3D VERSION 1.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.17-security
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(JUCE)

juce_add_plugin(Orbit3D
    COMPANY_NAME "Claude"
    PLUGIN_MANUFACTURER_CODE Cldx
    PLUGIN_CODE Orb3
    FORMATS VST3
    PRODUCT_NAME "Orbit 3D"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx Spatial
    NEEDS_WEB_BROWSER FALSE
    NEEDS_CURL FALSE)

target_sources(Orbit3D PRIVATE
    Source/PluginProcessor.cpp
    Source/SceneView.cpp
    Source/Panels.cpp)

target_compile_definitions(Orbit3D PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(Orbit3D
    PRIVATE
        juce::juce_audio_processors
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)
