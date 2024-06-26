#pragma once
#include "PluginProcessor.h"
#include <JuceHeader.h>

#define SITE_URL "https://arborealaudio.com"
#if JUCE_WINDOWS
#define DL_BIN "OmniAmp-windows.exe"
#elif JUCE_MAC
#define DL_BIN "OmniAmp-mac.dmg"
#elif JUCE_LINUX
#define DL_BIN "OmniAmp-linux.tar.xz"
#endif

static strix::UpdateResult dlResult;

//==============================================================================

class GammaAudioProcessorEditor : public AudioProcessorEditor, Timer
{
  public:
    GammaAudioProcessorEditor(GammaAudioProcessor &);
    ~GammaAudioProcessorEditor() override;

    //==============================================================================
    void paint(juce::Graphics &) override;
    void resized() override;
    void mouseDown(const MouseEvent &event) override
    {
        auto pos = getMouseXYRelative().toFloat();

        if (logoBounds.contains(pos)) {
            if (splash.onLogoClick)
                splash.onLogoClick();
            repaint();
        } else if (splash.isVisible()) {
            if (splash.getBounds().contains(pos.toInt()))
                splash.mouseDown(event);
            else {
                splash.setVisible(false);
                repaint();
            }
        } else
            AudioProcessorEditor::mouseDown(event);
    }

    void timerCallback() override
    {
        if (!dl.shouldBeHidden)
            dl.setVisible(dlResult.updateAvailable);
        if (lThread && !lThread->working)
            lThread.reset(nullptr);
    }

    void resetWindowSize();

  private:
    GammaAudioProcessor &audioProcessor;

    std::unique_ptr<Drawable> logo;
    Rectangle<float> logoBounds;

    AmpControls ampControls;

    Knob::Flags knobFlags = 0;
    Knob inGain{knobFlags}, outGain{knobFlags}, width{knobFlags},
        mix{knobFlags};
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment>
        inGainAttach, outGainAttach, widthAttach, mixAttach;
    LightButton bypass;
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment>
        bypassAttach;

    LinkButton link;

    std::vector<Component *> getTopComponents()
    {
        return {// &gate,
                &inGain, &link, &outGain, &width, &mix, &bypass};
    }

    PreComponent preComponent;
    CabsComponent cabComponent;
    ReverbComponent reverbComp;
    EnhancerComponent enhancers;

    Label pluginTitle;
    MenuComponent menu;
    PresetComp presetMenu;

    // FlexBox uiTopFlex, topControlsFlex;

    std::unique_ptr<strix::LiteThread> lThread;

    strix::DownloadManager dl;
    Splash splash;
    ActivationComponent activation;

#if JUCE_WINDOWS || JUCE_LINUX
    OpenGLContext opengl;
#endif

    std::unique_ptr<TooltipWindow> tooltip = nullptr;

    float uiScale = 1.f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GammaAudioProcessorEditor)
};