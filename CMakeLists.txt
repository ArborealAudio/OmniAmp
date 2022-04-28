cmake_minimum_required(VERSION 3.15)

project(Gamma VERSION 0.1)

find_package(JUCE CONFIG REQUIRED)

juce_set_vst2_sdk_path(C:/vstsdk2.4)

juce_add_plugin(Gamma
    COMPANY_NAME "Arboreal Audio"
    COPY_PLUGIN_AFTER_BUILD TRUE
    PLUGIN_MANUFACTURER_CODE Arau
    PLUGIN_CODE Gamm
    FORMATS AU VST3 VST
    VST2_CATEGORY kPlugCategEffect
    VST3_CATEGORY Fx
    AU_MAIN_TYPE kAudioUnitType_Effect
    PRODUCT_NAME "Gamma"
    HARDENED_RUNTIME_ENABLE TRUE)

juce_generate_juce_header(Gamma)

target_sources(Gamma
    PRIVATE
        Source/PluginEditor.cpp
        Source/PluginProcessor.cpp)

add_subdirectory(Source/Processors)

target_compile_definitions(Gamma
    PUBLIC
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_DISPLAY_SPLASH_SCREEN=0
        JUCE_REPORT_APP_USAGE=0)

# juce_add_binary_data(AudioPluginData SOURCES ...)

target_link_libraries(Gamma
    PRIVATE
        # AudioPluginData           # If we'd created a binary data target, we'd link to it here
        juce::juce_audio_utils
        juce::juce_dsp
    PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags)
