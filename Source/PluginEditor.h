/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

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

    void resetWindowSize() noexcept;
    void checkUpdate() noexcept;

private:

    GammaAudioProcessor& audioProcessor;

    std::unique_ptr<Drawable> logo;

    TopComponent top;

    AmpControls ampControls;

    Knob inGain{KnobType::Simple}, outGain{KnobType::Simple}, gate{KnobType::Simple};
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> inGainAttach, outGainAttach, gateAttach;

    CabsComponent cabComponent;

    ReverbComponent reverbComp;

    Label pluginTitle;

    MenuComponent menu;

#if JUCE_WINDOWS || JUCE_LINUX
    OpenGLContext opengl;
#endif

    TooltipWindow tooltip;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GammaAudioProcessorEditor)
};