cmake_minimum_required(VERSION 3.15)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_OSX_DEPLOYMENT_TARGET 10.9 CACHE INTERNAL "")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (PRODUCTION_BUILD)
    if (WIN32)
        set(CMAKE_SYSTEM_VERSION 7 CACHE INTERNAL "")
    elseif(APPLE)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE INTERNAL "")
    endif()
    set(CMAKE_BUILD_TYPE Release CACHE INTERNAL "")
else()
    set(PRODUCTION_BUILD OFF)
endif()
message("PRODUCTION_BUILD: ${PRODUCTION_BUILD}")

project(OmniAmp VERSION 1.0.1)

add_subdirectory(modules/juce)
add_subdirectory(modules/clap-juce-extensions EXCLUDE_FROM_ALL)

set(JUCE_FORMATS AU VST3)

if(BUILD_VST2)
    message("Adding VST2 target")
    juce_set_vst2_sdk_path("vst2_sdk")
    list(APPEND JUCE_FORMATS VST)
endif()

if (UNIX AND NOT APPLE)
    message("Adding LV2 target")
    list(APPEND JUCE_FORMATS LV2)
endif()

juce_add_plugin(OmniAmp
    COMPANY_NAME "Arboreal Audio"
    COMPANY_COPYRIGHT "(c) 2023 Arboreal Audio, LLC"
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Arau
    PLUGIN_CODE Oamp
    BUNDLE_ID "com.ArborealAudio.OmniAmp"
    FORMATS ${JUCE_FORMATS}
    VST2_CATEGORY kPlugCategEffect
    VST3_CATEGORY Fx Distortion
    AU_MAIN_TYPE kAudioUnitType_Effect
    PRODUCT_NAME "OmniAmp"
    NEEDS_CURL TRUE
    HARDENED_RUNTIME_ENABLE TRUE)

clap_juce_extensions_plugin(TARGET OmniAmp
    CLAP_ID "com.ArborealAudio.OmniAmp.clap"
    CLAP_PROCESS_EVENTS_RESOLUTION_SAMPLES 32
    CLAP_FEATURES audio-effect distortion reverb EQ)

if (NOT BETA_BUILD)
    set(BETA_BUILD OFF)
endif()
message("BETA_BUILD: ${BETA_BUILD}")

juce_generate_juce_header(OmniAmp)

target_sources(OmniAmp
    PRIVATE
        Source/PluginEditor.cpp
        Source/PluginProcessor.cpp
        Source/Activation.hpp)

add_subdirectory(Source/Processors)
add_subdirectory(Source/UI)
target_include_directories(OmniAmp PRIVATE
    "modules/chowdsp_wdf/include"
    "modules/Arbor_modules/")

juce_add_modules(
    "modules/gin/modules/gin"
    "modules/gin/modules/gin_graphics"
    "modules/Arbor_modules")

target_compile_definitions(OmniAmp
    PUBLIC
        PRODUCTION_BUILD=${PRODUCTION_BUILD}
        BETA_BUILD=${BETA_BUILD}
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_COREGRAPHICS_RENDER_WITH_MULTIPLE_PAINT_CALLS=1)

juce_add_binary_data(BinaryData SOURCES
    "Resources/2x12.svg"
    "Resources/4x12.svg"
    "Resources/6x10.svg"
    "Resources/mic.svg"
    "Resources/speaker.svg"
    "Resources/Link/link.svg"
    "Resources/Link/link_off.svg"
    "Resources/Link/link_over_off.svg"
    "Resources/Link/link_over_on.svg"
    "Resources/logo.svg"
    "Resources/Sora-Regular.ttf"
    "Resources/Hamburger_icon.svg"
)

# -fPIC flag for Linux
if (UNIX AND NOT APPLE)
    set_target_properties(BinaryData PROPERTIES
        POSITION_INDEPENDENT_CODE TRUE)
endif()

target_link_libraries(OmniAmp
    PRIVATE
        BinaryData
        juce::juce_audio_utils
        juce::juce_dsp
        gin
        gin_graphics
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags)

if (NOT APPLE)
target_link_libraries(OmniAmp
    PRIVATE
    juce::juce_opengl)
endif()
