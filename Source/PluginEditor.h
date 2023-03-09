/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

#define SITE_URL "https://arborealaudio.com"
#if JUCE_WINDOWS
#define DL_BIN "Gamma-windows.exe"
#elif JUCE_MAC
#define DL_BIN "Gamma-mac.dmg"
#elif JUCE_LINUX
#define DL_BIN "Gamma-linux.tar.gz"
#endif

static strix::UpdateResult dlResult;

#include "MainComponent.h"

//==============================================================================
/**
 */
class GammaAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    GammaAudioProcessorEditor(GammaAudioProcessor &);
    ~GammaAudioProcessorEditor() override;

    //==============================================================================
    void paint(juce::Graphics &) override;
    void resized() override;

private:

    MainComponent main;

    Viewport viewport;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GammaAudioProcessorEditor)
};