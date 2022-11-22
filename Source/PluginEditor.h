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
class GammaAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    GammaAudioProcessorEditor(GammaAudioProcessor &);
    ~GammaAudioProcessorEditor() override;

    //==============================================================================
    void paint(juce::Graphics &) override;
    void resized() override;
    bool hitTest(int x, int y) override
    {
        auto leftClick = ModifierKeys::currentModifiers.isLeftButtonDown();

        if (logoBounds.contains(x, y) && leftClick)
        {
            if (splash.onLogoClick)
                splash.onLogoClick();

            return true;
        }
        else if (splash.isVisible())
        {
            if (splash.getBounds().contains(x, y))
                return splash.hitTest(x, y);
            else if (leftClick)
            {
                splash.setVisible(false);
                return true;
            }
            else
                return false;
        }
        // else if (init.isVisible()) {
        //     if (init.getBounds().contains(x, y))
        //         return init.hitTest(x, y);
        //     else
        //         return false;
        // }
        else
            return AudioProcessorEditor::hitTest(x, y);
    }

    void resetWindowSize() noexcept;
    void checkUpdate() noexcept;

private:
    GammaAudioProcessor &audioProcessor;

    std::unique_ptr<Drawable> logo;
    Rectangle<float> logoBounds;

    AmpControls ampControls;

    Knob gate{KnobType::Simple}, inGain{KnobType::Simple}, outGain{KnobType::Simple}, width{KnobType::Simple}, mix{KnobType::Simple};
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> inGainAttach, outGainAttach, gateAttach, widthAttach, mixAttach;

    LightButton link;
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> linkAttach;

    std::vector<Component *> getTopComponents()
    {
        return {
            &gate,
            &inGain,
            &link,
            &outGain,
            &width,
            &mix};
    }

    PreComponent preComponent;
    CabsComponent cabComponent;
    ReverbComponent reverbComp;
    EnhancerComponent enhancers;

    Label pluginTitle;
    MenuComponent menu;
    PresetComp presetMenu;

    DownloadManager dl;
    Splash splash;
    ActivationComponent activation;

#if JUCE_WINDOWS || JUCE_LINUX
    OpenGLContext opengl;
#endif

    TooltipWindow tooltip;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GammaAudioProcessorEditor)
};