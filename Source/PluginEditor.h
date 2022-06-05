/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"
#include "UI/UI.h"

//==============================================================================
/**
*/
class GammaAudioProcessorEditor  : public juce::AudioProcessorEditor
{
public:
    GammaAudioProcessorEditor (GammaAudioProcessor&);
    ~GammaAudioProcessorEditor() override;

    //==============================================================================
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    GammaAudioProcessor& audioProcessor;

    ChoiceMenu mode;
    std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> modeAttach;

    AmpControls ampControls;

    Knob hfEnhance;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> hfAttach;

    Background bkgd;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GammaAudioProcessorEditor)
};
