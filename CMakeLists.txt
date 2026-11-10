cmake_minimum_required(VERSION 3.25)

# Identity lives here and is generated into PluginIdentity.h.
# Hosts store these codes in the session, so they must not change.
set(GROKFX_PRODUCT_NAME "Grok FX")
set(GROKFX_MANUFACTURER "Rando Audio")
set(GROKFX_BUNDLE_ID "com.randoaudio.grokfx")
set(GROKFX_PLUGIN_CODE "Grok")
set(GROKFX_MANUFACTURER_CODE "Raud")
set(GROKFX_VERSION "1.0.0")

if(APPLE)
    set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE STRING "Universal macOS build" FORCE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "11.0" CACHE STRING "" FORCE)
endif()

project(GrokFX VERSION ${GROKFX_VERSION} LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()

include(FetchContent)
Set(FETCHCONTENT_QUIET OFF)

FetchContent_Declare(JUCE
    GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
    GIT_TAG 8.0.15
    GIT_SHALLOW TRUE)
FetchContent_Declare(Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.8.0
    GIT_SHALLOW TRUE)

set(JUCE_BUILD_EXTRAS OFF CACHE BOOL "" FORCE)
set(JUCE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(JUCE Catch2)

if(APPLE)
    set(GROKFX_FORMATS VST3 AU Standalone)
else()
    set(GROKFX_FORMATS VST3 Standalone)
endif()

juce_add_plugin(GrokFX
    COMPANY_NAME "${GROKFX_MANUFACTURER}"
    BUNDLE_ID "${GROKFX_BUNDLE_ID}"
    IS_SYNTH TRUE
    NEEDS_MIDI_INPUT TRUE
    NEEDS_MIDI_OUTPUT FALSE
    IS_MIDI_EFFECT FALSE
    EDITOR_WANTS_KEYBOARD_FOCUS FALSE
    COPY_PLUGIN_AFTER_BUILD FALSE
    PLUGIN_MANUFACTURER_CODE "${GROKFX_MANUFACTURER_CODE}"
    PLUGIN_CODE "${GROKFX_PLUGIN_CODE}"
    FORMATS ${GROKFX_FORMATS}
    PRODUCT_NAME "${GROKFX_PRODUCT_NAME}"
    VST3_CATEGORIES Instrument Synth
    AU_MAIN_TYPE kAudioUnitType_MusicDevice)

juce_add_binary_data(GrokFXAssets
    SOURCES
        fonts/InterRegular.ttf
        fonts/InterMedium.ttf
        fonts/InterSemiBold.ttf)

configure_file(
    ${CMAKE_CURRENT_SOURCE_DIR}/source/PluginIdentity.h.in
    ${CMAKE_CURRENT_BINARY_DIR}/generated/PluginIdentity.h
    @ONLY)

add_library(GrokShared INTERFACE)
file(GLOB_RECURSE GROKFX_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/source/*.h")
target_sources(GrokShared INTERFACE ${GROKFX_SOURCES})
target_include_directories(GrokShared INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/source
    ${CMAKE_CURRENT_BINARY_DIR}/generated)
target_compile_definitions(GrokShared INTERFACE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_VST3_CAN_REPLACE_VST2=0
    JUCE_DISPLAY_SPLASH_SCREEN=1
    JUCE_MODAL_LOOPS_PERMITTED=1
    JUCE_STRICT_REFCOUNTEDPOINTER=1)
target_link_libraries(GrokShared INTERFACE
    GrokFXAssets
    juce::juce_audio_utils
    juce::juce_audio_processors
    juce::juce_audio_formats
    juce::juce_dsp
    juce::juce_gui_basics
    juce::juce_recommended_config_flags
    juce::juce_recommended_warning_flags)

target_link_libraries(GrokFX PRIVATE GrokShared)

juce_add_console_app(GrokFXTests PRODUCT_NAME "GrokFXTests")
file(GLOB GROKFX_TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")
target_sources(GrokFXTests PRIVATE ${GROKFX_TEST_SOURCES})
target_link_libraries(GrokFXTests PRIVATE
    GrokShared
    Catch2::Catch2WithMain)
target_compile_definitions(GrokFXTests PRIVATE
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0)

juce_add_console_app(GrokFXSnapshot PRODUCT_NAME "GrokFXSnapshot")
target_sources(GrokFXSnapshot PRIVATE tools/Snapshot.cpp)
target_link_libraries(GrokFXSnapshot PRIVATE GrokShared)
target_compile_definitions(GrokFXSnapshot PRIVATE
    JUCE_USE_CURL=0
    JUCE_WEB_BROWSER=0)

include(CTest)
include(Catch)
catch_discover_tests(GrokFXTests)

file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/grokfx.env"
"PRODUCT_NAME=\"${GROKFX_PRODUCT_NAME}\"
MANUFACTURER=\"${GROKFX_MANUFACTURER}\"
BUNDLE_ID=\"${GROKFX_BUNDLE_ID}\"
PLUGIN_CODE=\"${GROKFX_PLUGIN_CODE}\"
MANUFACTURER_CODE=\"${GROKFX_MANUFACTURER_CODE}\"
VERSION=\"${GROKFX_VERSION}\"
")
