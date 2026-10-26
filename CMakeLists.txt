cmake_minimum_required(VERSION 3.22)

project(EPNTrinity VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---------------------------------------------------------------------------
# JUCE: use a local checkout if JUCE_DIR is given, otherwise fetch it
# ---------------------------------------------------------------------------
set(JUCE_DIR "" CACHE PATH "Path to a local JUCE checkout (optional)")
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG        8.0.15
        GIT_SHALLOW    ON)
    FetchContent_MakeAvailable(JUCE)
endif()

if(MSVC)
    # static runtime: the .vst3 does not need the VC++ redistributable
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

option(TRINITY_COPY_PLUGIN "Copy the VST3 to the system plug-in folder after build" OFF)
option(TRINITY_BUILD_TOOLS "Build the GUI snapshot tool (dev only)" OFF)

juce_add_plugin(EPNTrinity
    COMPANY_NAME            "EPN Audio"
    COMPANY_WEBSITE         ""
    BUNDLE_ID               com.epnaudio.trinity
    PLUGIN_MANUFACTURER_CODE Epna
    PLUGIN_CODE             Trn3
    PRODUCT_NAME            "EPN Trinity"
    VERSION                 ${PROJECT_VERSION}
    FORMATS                 VST3 Standalone
    IS_SYNTH                FALSE
    NEEDS_MIDI_INPUT        FALSE
    NEEDS_MIDI_OUTPUT       FALSE
    IS_MIDI_EFFECT          FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    VST3_CATEGORIES         Fx Modulation
    COPY_PLUGIN_AFTER_BUILD ${TRINITY_COPY_PLUGIN})

target_sources(EPNTrinity PRIVATE
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp)

target_include_directories(EPNTrinity PRIVATE Source)

target_compile_definitions(EPNTrinity PUBLIC
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0)

target_link_libraries(EPNTrinity
    PRIVATE
        juce::juce_audio_utils
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

# ---------------------------------------------------------------------------
# Offline DSP test (plain C++, no JUCE)
# ---------------------------------------------------------------------------
add_executable(TrinityDspTest tests/render_test.cpp)
target_include_directories(TrinityDspTest PRIVATE Source)

# ---------------------------------------------------------------------------
# Dev tool: renders the editor to a PNG (used to check the GUI headless)
# ---------------------------------------------------------------------------
if(TRINITY_BUILD_TOOLS)
    juce_add_console_app(TrinitySnapshot PRODUCT_NAME "TrinitySnapshot")
    target_sources(TrinitySnapshot PRIVATE
        tools/snapshot.cpp
        Source/PluginProcessor.cpp
        Source/PluginEditor.cpp)
    target_include_directories(TrinitySnapshot PRIVATE Source)
    target_compile_definitions(TrinitySnapshot PRIVATE
        JucePlugin_Name="EPN Trinity"
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(TrinitySnapshot PRIVATE juce::juce_audio_utils juce::juce_gui_extra)

    juce_add_console_app(TrinityHostTest PRODUCT_NAME "TrinityHostTest")
    target_sources(TrinityHostTest PRIVATE tools/hosttest.cpp)
    target_compile_definitions(TrinityHostTest PRIVATE
        JUCE_PLUGINHOST_VST3=1 JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0)
    target_link_libraries(TrinityHostTest PRIVATE juce::juce_audio_processors juce::juce_gui_extra)
endif()
