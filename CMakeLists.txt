cmake_minimum_required(VERSION 3.22)
project(CanopyPads VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Build a universal Mac binary")

# JUCE: use a local checkout if present (./JUCE), otherwise fetch it.
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

# On macOS the AU/VST3 are copied straight into ~/Library/Audio/Plug-Ins. On Windows copying into
# Program Files needs admin rights, so the build script does that step instead.
if(APPLE)
    set(CANOPY_COPY_AFTER_BUILD TRUE)
else()
    set(CANOPY_COPY_AFTER_BUILD FALSE)
endif()

juce_add_plugin(CanopyPads
    COMPANY_NAME "Tagbor Audio"
    BUNDLE_ID "com.tagboraudio.canopypads"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    COPY_PLUGIN_AFTER_BUILD ${CANOPY_COPY_AFTER_BUILD}
    PLUGIN_MANUFACTURER_CODE Tgbr
    PLUGIN_CODE Cnpy
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    VST3_CATEGORIES Instrument Synth
    FORMATS AU VST3 Standalone
    PRODUCT_NAME "Canopy Pads")

juce_generate_juce_header(CanopyPads)

target_sources(CanopyPads PRIVATE
    Source/DroneEngine.cpp
    Source/SampleLayer.cpp
    Source/Exporter.cpp
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_compile_definitions(CanopyPads PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_USE_MP3AUDIOFORMAT=1
    JUCE_REPORT_APP_USAGE=0)

target_link_libraries(CanopyPads
    PRIVATE
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_audio_formats
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# Optional headless render tool used for testing the engine without a host.
juce_add_console_app(canopy_render PRODUCT_NAME "canopy_render")
target_sources(canopy_render PRIVATE Source/RenderTool.cpp Source/DroneEngine.cpp Source/SampleLayer.cpp Source/Exporter.cpp)
target_compile_definitions(canopy_render PRIVATE JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 JUCE_USE_MP3AUDIOFORMAT=1)
target_link_libraries(canopy_render PRIVATE juce::juce_audio_formats juce::juce_dsp juce::juce_recommended_config_flags)
juce_generate_juce_header(canopy_render)
set_target_properties(canopy_render PROPERTIES EXCLUDE_FROM_ALL TRUE)

# Headless logic tests for the processor (MIDI handling, latch, chord mode, learn, setlist, state).
juce_add_console_app(canopy_test PRODUCT_NAME "canopy_test")
target_sources(canopy_test PRIVATE Source/TestHarness.cpp Source/DroneEngine.cpp Source/SampleLayer.cpp Source/Exporter.cpp Source/PluginProcessor.cpp Source/PluginEditor.cpp)
target_compile_definitions(canopy_test PRIVATE JUCE_MODAL_LOOPS_PERMITTED=1 JUCE_USE_CURL=0 JUCE_WEB_BROWSER=0 JUCE_USE_MP3AUDIOFORMAT=1 "JucePlugin_Name=\"Canopy Pads\"")
target_link_libraries(canopy_test PRIVATE juce::juce_audio_utils juce::juce_dsp juce::juce_audio_formats juce::juce_recommended_config_flags)
juce_generate_juce_header(canopy_test)
set_target_properties(canopy_test PROPERTIES EXCLUDE_FROM_ALL TRUE)
