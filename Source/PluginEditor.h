/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"


/**
 * Thread module for quick 'n dirty worker thread (& bc I live in hell)
*/

struct LiteThread : Thread
{
    LiteThread() : Thread("LiteThread")
    {
        DBG("Starting check thread...");
        startThread(Thread::Priority::background);
    }

    ~LiteThread() override
    {
        DBG("Stopping check thread...");
        stopThread(1000);
    }

    void run() override
    {
        while (!threadShouldExit())
        {
            if (!jobs.empty())
            {
                auto job = jobs.front();
                jobs.pop();
                job();
                jobCount += 1;
#if BETA_BUILD || DEV_BUILD
                if (jobCount >= 2)
                    signalThreadShouldExit(); /** janky way of ensuring we ran both checks */
#else
                if (jobCount >= 1)
                    signalThreadShouldExit();
#endif
            }
            else
                wait(100);
        }
        DBG("Check thread exited loop");
        if (onLoopExit)
            onLoopExit();
    }

    /* locking method for adding jobs to the thread */
    void addJob(std::function<void()> job)
    {
        std::unique_lock<std::mutex> lock(mutex);
        jobs.emplace(job);
        notify();
    }

    std::function<void()> onLoopExit;

private:
    std::mutex mutex;
    std::queue<std::function<void()>> jobs;
    int jobCount = 0;
};

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
    void mouseDown(const MouseEvent &event) override
    {
        auto pos = getMouseXYRelative().toFloat();

        if (logoBounds.contains(pos))
        {
            if (splash.onLogoClick)
                splash.onLogoClick();
            repaint();
        }
        else if (splash.isVisible())
        {
            if (splash.getBounds().contains(pos.toInt()))
                splash.mouseDown(event);
            else
            {
                splash.setVisible(false);
                repaint();
            }
        }
        else
            AudioProcessorEditor::mouseDown(event);
    }

    void resetWindowSize();

private:
    GammaAudioProcessor &audioProcessor;

    std::unique_ptr<Drawable> logo;
    Rectangle<float> logoBounds;

    AmpControls ampControls;

    Knob::flags_t knobFlags = 0;
    Knob gate{knobFlags}, inGain{knobFlags}, outGain{knobFlags}, width{knobFlags}, mix{knobFlags};
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> inGainAttach, outGainAttach, gateAttach, widthAttach, mixAttach;
    LightButton bypass;
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> bypassAttach;

    LinkButton link;

    std::vector<Component *> getTopComponents()
    {
        return {
            &gate,
            &inGain,
            &link,
            &outGain,
            &width,
            &mix/* ,
            &bypass */};
    }

    PreComponent preComponent;
    CabsComponent cabComponent;
    ReverbComponent reverbComp;
    EnhancerComponent enhancers;

    Label pluginTitle;
    MenuComponent menu;
    PresetComp presetMenu;

    std::unique_ptr<LiteThread> lThread;

    DownloadManager dl;
    Splash splash;
    ActivationComponent activation;

#if JUCE_WINDOWS || JUCE_LINUX
    OpenGLContext opengl;
#endif

    std::unique_ptr<TooltipWindow> tooltip = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GammaAudioProcessorEditor)
};