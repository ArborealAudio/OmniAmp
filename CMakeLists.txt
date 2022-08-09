cmake_minimum_required(VERSION 3.15)

project(Gamma VERSION 0.3)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(JUCE CONFIG REQUIRED)
add_subdirectory(modules/clap-juce-extensions EXCLUDE_FROM_ALL)

juce_set_vst2_sdk_path("${VST2_SDK}")

set(JUCE_FORMATS AU VST3)

if(UNIX AND NOT APPLE)
    list(APPEND JUCE_FORMATS LV2)
endif()

if(TARGET juce_set_vst2_sdk)
    list(APPEND JUCE_FORMATS VST)
endif()

juce_add_plugin(Gamma
    COMPANY_NAME "Arboreal Audio"
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Arau
    PLUGIN_CODE Gamm
    FORMATS ${JUCE_FORMATS}
    VST2_CATEGORY kPlugCategEffect
    VST3_CATEGORY Fx
    AU_MAIN_TYPE kAudioUnitType_Effect
    PRODUCT_NAME "Gamma"
    HARDENED_RUNTIME_ENABLE TRUE)

clap_juce_extensions_plugin(TARGET Gamma
    CLAP_ID "com.ArborealAudio.Gamma.clap"
    CLAP_FEATURES amp "channel strip" guitar bass)

juce_generate_juce_header(Gamma)

target_sources(Gamma
    PRIVATE
        Source/PluginEditor.cpp
        Source/PluginProcessor.cpp)

add_subdirectory(Source/Processors)
add_subdirectory(Source/UI)
add_subdirectory(modules/chowdsp_wdf)

juce_add_modules(
    "$ENV{HOME}/modules/Gin/modules/gin"
    "$ENV{HOME}/modules/Gin/modules/gin_gui"
    "modules/DSPFilters/modules/dsp_filters"
    "$ENV{HOME}/modules/Arbor_modules")

target_compile_definitions(Gamma
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0
        JUCE_COREGRAPHICS_RENDER_WITH_MULTIPLE_PAINT_CALLS=1)

juce_add_binary_data(BinaryData SOURCES
    "Resources/2x12.svg"
    "Resources/4x12.svg"
    "Resources/6x10.svg"
    "Resources/amp_mesh_2.svg"
    "Resources/logo.svg"
    "Resources/LiberationSans-Regular.ttf")

# -fPIC flag mainly for Linux
set_target_properties(BinaryData PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE)

target_link_libraries(Gamma
    PRIVATE
        BinaryData
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_opengl
        gin
        gin_gui
        dsp_filters
        Arbor_modules
        chowdsp_wdf
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)