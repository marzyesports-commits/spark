cmake_minimum_required(VERSION 3.22)

project(Spark VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "Minimum macOS version")
endif()

# JUCE: use a local checkout if JUCE_DIR is given, otherwise download it.
set(JUCE_DIR "" CACHE PATH "Path to a JUCE checkout (optional)")
if(JUCE_DIR)
    add_subdirectory(${JUCE_DIR} JUCE)
else()
    include(FetchContent)
    FetchContent_Declare(JUCE
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG 8.0.10
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(JUCE)
endif()

option(SPARK_HARDENED_RUNTIME "Enable the macOS hardened runtime (needed for notarisation)" OFF)

if(APPLE)
    set(SPARK_FORMATS VST3 AU Standalone)
elseif(WIN32)
    set(SPARK_FORMATS VST3 Standalone)
else()
    set(SPARK_FORMATS VST3 Standalone)
endif()

juce_add_binary_data(SparkAssets SOURCES
    Resources/Fonts/Syne-ExtraBold.ttf
    Resources/Fonts/Manrope-Medium.ttf
    Resources/Fonts/Manrope-Bold.ttf
    Resources/Fonts/JetBrainsMono-Regular.ttf)
set_target_properties(SparkAssets PROPERTIES POSITION_INDEPENDENT_CODE ON)

set(SPARK_COMMON_SOURCES
    Source/Common/SparkLookAndFeel.cpp
    Source/Common/Lineage.cpp
    Source/Common/SparkProcessorBase.cpp
    Source/Common/Components.cpp
    Source/Common/Wavetable.cpp)

function(spark_configure target)
    target_sources(${target} PRIVATE ${SPARK_COMMON_SOURCES})
    target_include_directories(${target} PRIVATE Source)
    target_compile_definitions(${target} PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0)
    target_link_libraries(${target}
        PRIVATE
            SparkAssets
            juce::juce_audio_utils
            juce::juce_audio_formats
            juce::juce_dsp
        PUBLIC
            juce::juce_recommended_config_flags
            juce::juce_recommended_lto_flags
            juce::juce_recommended_warning_flags)
endfunction()

# ---------- Spark (instrument) ----------
juce_add_plugin(Spark
    COMPANY_NAME "Spark Audio"
    COMPANY_WEBSITE "https://github.com/marzyesports-commits/spark"
    BUNDLE_ID com.sparkaudio.spark
    PLUGIN_MANUFACTURER_CODE Sprk
    PLUGIN_CODE Spk1
    FORMATS ${SPARK_FORMATS}
    PRODUCT_NAME "Spark"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Sampler Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    ICON_BIG Resources/Icon.png
    HARDENED_RUNTIME_ENABLED ${SPARK_HARDENED_RUNTIME}
    MICROPHONE_PERMISSION_ENABLED FALSE)

target_sources(Spark PRIVATE
    Source/Instrument/InstrumentProcessor.cpp
    Source/Instrument/InstrumentEditor.cpp
    Source/Instrument/SparkVoice.cpp)
spark_configure(Spark)

# ---------- Spark FX (effect) ----------
juce_add_plugin(SparkFX
    COMPANY_NAME "Spark Audio"
    COMPANY_WEBSITE "https://github.com/marzyesports-commits/spark"
    BUNDLE_ID com.sparkaudio.sparkfx
    PLUGIN_MANUFACTURER_CODE Sprk
    PLUGIN_CODE Spk2
    FORMATS ${SPARK_FORMATS}
    PRODUCT_NAME "Spark FX"
    IS_SYNTH FALSE
    NEEDS_MIDI_INPUT FALSE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Fx
    AU_MAIN_TYPE kAudioUnitType_Effect
    ICON_BIG Resources/Icon.png
    HARDENED_RUNTIME_ENABLED ${SPARK_HARDENED_RUNTIME}
    HARDENED_RUNTIME_OPTIONS com.apple.security.device.audio-input
    MICROPHONE_PERMISSION_ENABLED TRUE
    MICROPHONE_PERMISSION_TEXT "Spark FX processes live audio input in standalone mode.")

target_sources(SparkFX PRIVATE
    Source/FX/FxProcessor.cpp
    Source/FX/FxEditor.cpp)
spark_configure(SparkFX)

# ---------- Offline tests (not shipped) ----------
option(SPARK_BUILD_TESTS "Build the offline render tests" OFF)
if(SPARK_BUILD_TESTS)
    juce_add_console_app(SparkTests PRODUCT_NAME "SparkTests")
    target_sources(SparkTests PRIVATE
        Tests/SparkTests.cpp
        Source/Instrument/InstrumentProcessor.cpp
        Source/Instrument/InstrumentEditor.cpp
        Source/Instrument/SparkVoice.cpp
        Source/FX/FxProcessor.cpp
        Source/FX/FxEditor.cpp
        ${SPARK_COMMON_SOURCES})
    target_include_directories(SparkTests PRIVATE Source)
    target_compile_definitions(SparkTests PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 SPARK_TESTS=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(SparkTests PRIVATE SparkAssets
        juce::juce_audio_utils juce::juce_audio_formats juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
