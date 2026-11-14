cmake_minimum_required(VERSION 3.22)

project(DEADWAVE VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

option(DEADWAVE_BUILD_TESTS "Build the headless engine test runner" ON)
option(DEADWAVE_COPY_AFTER_BUILD "Copy the VST3 into the system plug-in folder after building" OFF)
set(DEADWAVE_JUCE_PATH "" CACHE PATH "Path to a local JUCE 8 checkout. Leave empty to download JUCE 8.0.9 automatically.")

# ---------------------------------------------------------------------------------------------
# JUCE
# ---------------------------------------------------------------------------------------------
if(DEADWAVE_JUCE_PATH)
    add_subdirectory("${DEADWAVE_JUCE_PATH}" "${CMAKE_BINARY_DIR}/JUCE")
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.9
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

# ---------------------------------------------------------------------------------------------
# Plug-in
# ---------------------------------------------------------------------------------------------
juce_add_plugin(DEADWAVE
    PRODUCT_NAME "DEADWAVE"
    COMPANY_NAME "DEADWAVE"
    BUNDLE_ID "com.deadwave.deadwave"
    PLUGIN_MANUFACTURER_CODE Ddwv
    PLUGIN_CODE Dwav
    VERSION ${PROJECT_VERSION}
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS TRUE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    FORMATS VST3 Standalone
    COPY_PLUGIN_AFTER_BUILD ${DEADWAVE_COPY_AFTER_BUILD})

set(DEADWAVE_SOURCES
    Source/PluginProcessor.cpp
    Source/PluginEditor.cpp
    Source/Core/Params.cpp
    Source/Core/Expression.cpp
    Source/DSP/Wavetable.cpp
    Source/DSP/WaveGen.cpp
    Source/DSP/Oscillator.cpp
    Source/DSP/Filter.cpp
    Source/DSP/Voice.cpp
    Source/DSP/SynthEngine.cpp
    Source/DSP/Effects.cpp
    Source/DSP/FxRack.cpp
    Source/MIDI/MidiFx.cpp
    Source/Wave/WaveDocument.cpp
    Source/Presets/StateIO.cpp
    Source/Presets/PresetManager.cpp
    Source/Presets/FactoryPresets.cpp
    Source/Presets/SoundGenerator.cpp
    Source/Wave/SampleImport.cpp
    Source/GUI/LookAndFeel.cpp
    Source/GUI/Widgets.cpp
    Source/GUI/OscPanel.cpp
    Source/GUI/ModulationViews.cpp
    Source/GUI/WaveEditor.cpp
    Source/GUI/FxView.cpp
    Source/GUI/ArpSeqView.cpp
    Source/GUI/BrowserView.cpp
    Source/GUI/GeneratorView.cpp
    Source/GUI/MainView.cpp)

# Shared code consumed by the plug-in and by the test runner.
add_library(DeadwaveCore INTERFACE)
target_sources(DeadwaveCore INTERFACE ${DEADWAVE_SOURCES})
target_include_directories(DeadwaveCore INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/Source")
target_compile_definitions(DeadwaveCore INTERFACE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=0
    JUCE_SILENCE_XCODE_15_LINKER_WARNING=1
    DEADWAVE_VERSION_STRING="${PROJECT_VERSION}")
target_link_libraries(DeadwaveCore INTERFACE
    juce::juce_audio_utils
    juce::juce_audio_formats
    juce::juce_dsp)

target_link_libraries(DEADWAVE
    PRIVATE
        DeadwaveCore
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if(MSVC)
    target_compile_options(DEADWAVE PRIVATE /fp:fast /bigobj)
else()
    target_compile_options(DEADWAVE PRIVATE -ffast-math -fno-finite-math-only)
endif()

# ---------------------------------------------------------------------------------------------
# Headless test runner: renders audio through the real processor, checks invariants.
# ---------------------------------------------------------------------------------------------
if(DEADWAVE_BUILD_TESTS)
    juce_add_console_app(DeadwaveTests PRODUCT_NAME "DeadwaveTests")
    target_sources(DeadwaveTests PRIVATE Tests/EngineTests.cpp)
    target_compile_definitions(DeadwaveTests PRIVATE
        JucePlugin_Name="DEADWAVE"
        JucePlugin_IsSynth=1
        JucePlugin_WantsMidiInput=1
        JucePlugin_ProducesMidiOutput=0
        JucePlugin_IsMidiEffect=0
        DEADWAVE_TESTS=1
        JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(DeadwaveTests PRIVATE
        DeadwaveCore
        juce::juce_recommended_config_flags
        juce::juce_recommended_warning_flags)
    if(MSVC)
        target_compile_options(DeadwaveTests PRIVATE /fp:fast /bigobj)
    else()
        target_compile_options(DeadwaveTests PRIVATE -ffast-math -fno-finite-math-only)
    endif()

    # Loads the built VST3 through JUCE's hosting layer (validates the real plug-in binary).
    juce_add_console_app(DeadwaveHostTest PRODUCT_NAME "DeadwaveHostTest")
    target_sources(DeadwaveHostTest PRIVATE Tests/HostTest.cpp)
    target_compile_definitions(DeadwaveHostTest PRIVATE
        JUCE_PLUGINHOST_VST3=1
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0)
    target_link_libraries(DeadwaveHostTest PRIVATE
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_recommended_config_flags)
    add_dependencies(DeadwaveHostTest DEADWAVE_VST3)
endif()
