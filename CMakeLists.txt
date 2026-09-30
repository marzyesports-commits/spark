cmake_minimum_required(VERSION 3.22)

project(Spark VERSION 1.5.0 LANGUAGES C CXX)

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

# The factory sound library (synthesised by tools/factory_sounds/make_sounds.py)
file(GLOB SPARK_SOUND_FILES ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Sounds/*.flac ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Sounds/manifest.json)
juce_add_binary_data(SparkSounds HEADER_NAME SparkSoundData.h NAMESPACE SparkSoundData SOURCES ${SPARK_SOUND_FILES})
set_target_properties(SparkSounds PROPERTIES POSITION_INDEPENDENT_CODE ON)

# OBSDN ships a smaller, lead-focused set of the same sounds (Resources/OBSDN/manifest.json lists them)
file(READ ${CMAKE_CURRENT_SOURCE_DIR}/Resources/OBSDN/manifest.json OBSDN_MANIFEST)
string(REGEX MATCHALL "\"id\": \"[a-z0-9_]+\"" OBSDN_SOUND_IDS "${OBSDN_MANIFEST}")
set(OBSDN_SOUND_FILES ${CMAKE_CURRENT_SOURCE_DIR}/Resources/OBSDN/manifest.json)
foreach(entry ${OBSDN_SOUND_IDS})
    string(REGEX REPLACE "\"id\": \"([a-z0-9_]+)\"" "\\1" sound_id "${entry}")
    list(APPEND OBSDN_SOUND_FILES ${CMAKE_CURRENT_SOURCE_DIR}/Resources/Sounds/${sound_id}.flac)
endforeach()
juce_add_binary_data(OBSDNSounds HEADER_NAME SparkSoundData.h NAMESPACE SparkSoundData SOURCES ${OBSDN_SOUND_FILES})
set_target_properties(OBSDNSounds PROPERTIES POSITION_INDEPENDENT_CODE ON)

# Shared by every Spark plugin
set(SPARK_SHARED_SOURCES
    Source/Common/SparkLookAndFeel.cpp
    Source/Common/Lineage.cpp
    Source/Common/SparkProcessorBase.cpp
    Source/Common/Components.cpp
    Source/Common/Wavetable.cpp
    Source/Common/UndoHistory.cpp
    Source/Common/Cards.cpp
    Source/Instrument/FxRack.cpp
    Source/Instrument/FxHost.cpp
    Source/Instrument/SourceData.cpp
    Source/Instrument/FactorySounds.cpp
    Source/Instrument/Shapeshift.cpp
    Source/Instrument/Modulation.cpp
    Source/Instrument/ModHost.cpp
    Source/Instrument/ModCards.cpp)

# Spark (the sound-design instrument)
set(SPARK_COMMON_SOURCES
    ${SPARK_SHARED_SOURCES}
    Source/Instrument/InstrumentPresets.cpp)

# OBSDN (the lead synth with its riff writer)
set(OBSDN_SOURCES
    ${SPARK_SHARED_SOURCES}
    Source/Lead/Riff.cpp
    Source/Lead/LeadPresets.cpp
    Source/Lead/LeadVoice.cpp
    Source/Lead/LeadProcessor.cpp
    Source/Lead/LeadEditor.cpp
    Source/Instrument/FxPage.cpp)

function(spark_configure target)
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
    Source/Instrument/FxPage.cpp
    Source/Instrument/SynthPage.cpp
    Source/Instrument/SparkVoice.cpp
    ${SPARK_COMMON_SOURCES})
spark_configure(Spark)
target_link_libraries(Spark PRIVATE SparkSounds)

# ---------- OBSDN (lead synth + riff writer, jade theme) ----------
set(OBSDN_VERSION 0.6.0)
juce_add_plugin(OBSDN
    VERSION ${OBSDN_VERSION}
    COMPANY_NAME "Spark Audio"
    BUNDLE_ID com.sparkaudio.obsdn
    PLUGIN_MANUFACTURER_CODE Sprk
    PLUGIN_CODE Obsd
    FORMATS ${SPARK_FORMATS}
    PRODUCT_NAME "OBSDN"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice
    ICON_BIG Resources/OBSDN/Icon.png
    HARDENED_RUNTIME_ENABLED ${SPARK_HARDENED_RUNTIME}
    MICROPHONE_PERMISSION_ENABLED FALSE)
target_sources(OBSDN PRIVATE ${OBSDN_SOURCES})
spark_configure(OBSDN)
target_link_libraries(OBSDN PRIVATE OBSDNSounds)
target_compile_definitions(OBSDN PRIVATE SPARK_THEME_OBSDN=1)

# ---------- Offline tests (not shipped) ----------
option(SPARK_BUILD_TESTS "Build the offline render tests" OFF)
if(SPARK_BUILD_TESTS)
    juce_add_console_app(SparkTests PRODUCT_NAME "SparkTests")
    target_sources(SparkTests PRIVATE
        Tests/SparkTests.cpp
        Source/Instrument/InstrumentProcessor.cpp
        Source/Instrument/InstrumentEditor.cpp
        Source/Instrument/FxPage.cpp
        Source/Instrument/SynthPage.cpp
        Source/Instrument/SparkVoice.cpp
        ${SPARK_COMMON_SOURCES})
    target_include_directories(SparkTests PRIVATE Source)
    target_compile_definitions(SparkTests PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 SPARK_TESTS=1 JUCE_MODAL_LOOPS_PERMITTED=1)
    target_link_libraries(SparkTests PRIVATE SparkAssets SparkSounds
        juce::juce_audio_utils juce::juce_audio_formats juce::juce_dsp
        juce::juce_recommended_config_flags)

    juce_add_console_app(OBSDNTests PRODUCT_NAME "OBSDNTests")
    target_sources(OBSDNTests PRIVATE Tests/OBSDNTests.cpp ${OBSDN_SOURCES})
    target_include_directories(OBSDNTests PRIVATE Source)
    target_compile_definitions(OBSDNTests PRIVATE
        JUCE_WEB_BROWSER=0 JUCE_USE_CURL=0 SPARK_TESTS=1 JUCE_MODAL_LOOPS_PERMITTED=1 SPARK_THEME_OBSDN=1)
    target_link_libraries(OBSDNTests PRIVATE SparkAssets OBSDNSounds
        juce::juce_audio_utils juce::juce_audio_formats juce::juce_dsp
        juce::juce_recommended_config_flags)
endif()
