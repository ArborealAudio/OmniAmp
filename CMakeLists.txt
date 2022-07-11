cmake_minimum_required(VERSION 3.15)

project(Gamma VERSION 0.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(JUCE CONFIG REQUIRED)

juce_set_vst2_sdk_path("${VST2_SDK}")

juce_add_plugin(Gamma
    COMPANY_NAME "Arboreal Audio"
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Arau
    PLUGIN_CODE Gamm
    FORMATS AU VST3 VST Standalone
    VST2_CATEGORY kPlugCategEffect
    VST3_CATEGORY Fx
    AU_MAIN_TYPE kAudioUnitType_Effect
    PRODUCT_NAME "Gamma"
    HARDENED_RUNTIME_ENABLE TRUE)

if(WIN32)
    find_library(IPP_CORE ippcore.lib PATHS "C:/Program Files (x86)/Intel/oneAPI/ipp/2021.6.0/lib/intel64")
    find_library(IPP_S ipps.lib PATHS "C:/Program Files (x86)/Intel/oneAPI/ipp/2021.6.0/lib/intel64")
    target_include_directories(Gamma PRIVATE "C:/Program Files (x86)/Intel/oneAPI/ipp/2021.6.0/include")
endif()

juce_generate_juce_header(Gamma)

target_sources(Gamma
    PRIVATE
        Source/PluginEditor.cpp
        Source/PluginProcessor.cpp)

add_subdirectory(Source/Processors)
add_subdirectory(Source/UI)
add_subdirectory(modules/chowdsp_wdf)
add_subdirectory(modules/chowdsp_utils)

juce_add_modules(
    "/modules/Gin/modules/gin"
    "/modules/Gin/modules/gin_gui"
    "/modules/DSPFilters/modules/dsp_filters"
    "/modules/VolumeMeter")

target_compile_definitions(Gamma
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0)
        # JUCE_IPP_AVAILABLE=1)
        # JUCE_COREGRAPHICS_RENDER_WITH_MULTIPLE_PAINT_CALLS=1)

juce_add_binary_data(BinaryData SOURCES
    "Resources/IRs/Fender Bassman SM57.wav"
    "Resources/IRs/grafton bass deluxe.wav")

target_link_libraries(Gamma
    PRIVATE
        BinaryData
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_opengl
        gin
        gin_gui
        dsp_filters
        VolumeMeter
        chowdsp_wdf
        chowdsp_simd
        chowdsp_dsp_data_structures
        chowdsp_math
        ${IPP_CORE}
        ${IPP_S}
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
