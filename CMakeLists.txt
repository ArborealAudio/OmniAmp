cmake_minimum_required(VERSION 3.15)

set(CMAKE_OSX_DEPLOYMENT_TARGET 10.9 CACHE INTERNAL "")
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if (PRODUCTION_BUILD)
    message("Setting PRODUCTION_BUILD")
    if (WIN32)
        set(CMAKE_SYSTEM_VERSION 7 CACHE INTERNAL "")
    elseif(APPLE)
        set(CMAKE_OSX_ARCHITECTURES "arm64;x86_64" CACHE INTERNAL "")
    endif()
    set(CMAKE_BUILD_TYPE Release CACHE INTERNAL "")
endif()

project(Gamma VERSION 0.6)



add_subdirectory(modules/juce)
add_subdirectory(modules/clap-juce-extensions EXCLUDE_FROM_ALL)

juce_set_vst2_sdk_path("$ENV{HOME}/vst2_sdk")

set(JUCE_FORMATS AU VST3)

if(UNIX AND NOT APPLE)
    list(APPEND JUCE_FORMATS LV2)
endif()

if(TARGET juce_vst2_sdk)
    list(APPEND JUCE_FORMATS VST)
endif()

juce_add_plugin(Gamma
    COMPANY_NAME "Arboreal Audio"
    COMPANY_COPYRIGHT "(c) 2022 Arboreal Audio, LLC"
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Arau
    PLUGIN_CODE Gamm
    FORMATS ${JUCE_FORMATS}
    VST2_CATEGORY kPlugCategEffect
    VST3_CATEGORY Fx Distortion
    AU_MAIN_TYPE kAudioUnitType_Effect
    PRODUCT_NAME "Gamma"
    HARDENED_RUNTIME_ENABLE TRUE)

clap_juce_extensions_plugin(TARGET Gamma
    CLAP_ID "com.ArborealAudio.Gamma.clap"
    CLAP_PROCESS_EVENTS_RESOLUTION_SAMPLES 32
    CLAP_FEATURES amp "channel strip" guitar bass)

if (BETA_BUILD)
    message("Setting BETA_BUILD")
    target_compile_definitions(Gamma PUBLIC BETA_BUILD=1)
endif()

juce_generate_juce_header(Gamma)

target_sources(Gamma
    PRIVATE
        Source/PluginEditor.cpp
        Source/PluginProcessor.cpp
        Source/Activation.hpp)

add_subdirectory(Source/Processors)
add_subdirectory(Source/UI)
add_subdirectory(Source/Presets)
target_include_directories(Gamma PRIVATE "modules/chowdsp_wdf/include")

juce_add_modules(
    "modules/gin/modules/gin"
    "modules/gin/modules/gin_gui"
    "modules/Arbor_modules")

target_compile_definitions(Gamma
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
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
    "Resources/Link/link_over.svg"
    "Resources/logo.svg"
    "Resources/LiberationSans-Regular.ttf"
    "Resources/Hamburger_icon.svg"
    "Resources/Presets/Default Bass.aap"
    "Resources/Presets/Default Guitar.aap"
    "Resources/Presets/Default Channel.aap")

# -fPIC flag mainly for Linux
set_target_properties(BinaryData PROPERTIES
    POSITION_INDEPENDENT_CODE TRUE)

target_link_libraries(Gamma
    PRIVATE
        BinaryData
        juce::juce_audio_utils
        juce::juce_dsp
        juce::juce_cryptography
        gin
        gin_gui
        Arbor_modules
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)

if (NOT APPLE)
target_link_libraries(Gamma
    PRIVATE
    juce::juce_opengl)
endif()