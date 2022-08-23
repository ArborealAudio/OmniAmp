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

private:
    GammaAudioProcessor& audioProcessor;

    std::unique_ptr<Drawable> logo, mesh;
    std::unique_ptr<Image> blur;

    AmpControls ampControls;

    Knob hfEnhance{KnobType::HF}, lfEnhance{KnobType::LF};
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> hfAttach, lfAttach;

    Knob inGain{KnobType::Simple}, outGain{KnobType::Simple}, gate{KnobType::Simple};
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> inGainAttach, outGainAttach, gateAttach;

    strix::SineWaveComponent wave;

    CabsComponent cabComponent;

    ReverbComponent reverbComp;

    Label pluginTitle;

#if JUCE_WINDOWS
    OpenGLContext opengl;
#endif

    TooltipWindow tooltip;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GammaAudioProcessorEditor)
};
