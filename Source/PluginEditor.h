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

    void writeConfigFile(const String& property, int value)
    {
        File config {File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName() + "/Arboreal Audio/Gamma/config.xml"};
        if (!config.existsAsFile())
            config.create();
        
        auto xml = parseXML(config);
        if (xml == nullptr)
        {
            xml.reset(new XmlElement("Config"));
        }

        xml->setAttribute(property, value);
        xml->writeTo(config);
    }

    /*returns integer value of read property, or -1 if it doesn't exist*/
    int readConfigFile(const String& property)
    {
        File config {File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName() + "/Arboreal Audio/Gamma/config.xml"};
        if (!config.existsAsFile())
            return -1;
        
        auto xml = parseXML(config);
        if (xml != nullptr && xml->hasTagName("Config"))
            return xml->getIntAttribute(property, -1);
        
        return -1;
    }

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

    MenuComponent menu;

#if JUCE_WINDOWS || JUCE_LINUX
    OpenGLContext opengl;
#endif

    TooltipWindow tooltip;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GammaAudioProcessorEditor)
};