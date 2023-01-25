// AmpControls.h

#pragma once

#define GAMMA_RAY 35.f / 360.f, 0.31f, 0.71f, 0.3f
#define SUNBEAM 50.f / 360.f, 0.28f, 0.75f, 0.3f
#define MOONBEAM 50.f / 360.f, 0.10f, 0.71f, 0.3f
#define XRAY 12.f / 360.f, 0.28f, 0.61f, 0.3f
#define COBALT 210.f / 360.f, 0.25f, 0.5f, 0.5f
#define EMERALD 157.f / 360.f, 0.13f, 0.5f, 0.5f
#define QUARTZ 240.f / 360.f, 0.07f, 0.67f, 0.5f

struct AmpControls : Component, private Timer
{
    AmpControls(AudioProcessorValueTreeState &a) : vts(a),
                                                   mode(static_cast<strix::ChoiceParameter *>(vts.getParameter("mode"))->getAllValueStrings()),
                                                   guitarMode(static_cast<strix::ChoiceParameter *>(vts.getParameter("guitarMode"))->getAllValueStrings()),
                                                   bassMode(static_cast<strix::ChoiceParameter *>(vts.getParameter("bassMode"))->getAllValueStrings()),
                                                   channelMode(static_cast<strix::ChoiceParameter *>(vts.getParameter("channelMode"))->getAllValueStrings())
    {
        for (auto &k : getKnobs())
            addAndMakeVisible(*k);

        mode_p = vts.getRawParameterValue("mode");

        auto zeroToTen = [](float val)
        {
            return String(val * 10.0, 1);
        };

        auto decibels = [](float val)
        {
            val = jmap(val, -6.f, 6.f);
            String str(val, 1);
            str.append("dB", 2);
            return str;
        };

        std::function<String(float)> eqFunc;
        if (*vts.getRawParameterValue("mode") > 1)
            eqFunc = decibels;
        else
            eqFunc = zeroToTen;
        
        distAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(vts, "dist", dist);
        dist.setLabel("Overdrive");
        dist.setTooltip("A one-knob distortion pedal. Fully bypassed at 0.");
        dist.setValueToStringFunction(zeroToTen);

        inGainAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(a, "preampGain", inGain);
        inGain.setLabel("Preamp");
        inGain.setTooltip("Preamp gain stage. In Channel mode, is bypassed at 0.\n\nAlt/Option-click to enable Auto Gain.");
        inGain.setValueToStringFunction(zeroToTen);

        String eqTooltip = "EQ section of the amp. In Channel mode, the EQ knobs will cut below 50% and add above 50%.\n\nIn Guitar & Bass mode, these controls will work like a traditional amp tone stack.\n\nIn Channel mode, Alt/Option-click will enable frequency-weighted Auto Gain.";

        bassAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(a, "bass", bass);
        bass.setLabel("Bass");
        bass.setTooltip(eqTooltip);
        bass.setValueToStringFunction(eqFunc);

        midAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(a, "mid", mid);
        mid.setLabel("Mid");
        mid.setTooltip(eqTooltip);
        mid.setValueToStringFunction(eqFunc);

        trebleAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(a, "treble", treble);
        treble.setLabel("Treble");
        treble.setTooltip(eqTooltip);
        treble.setValueToStringFunction(eqFunc);

        outGainAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(a, "powerampGain", outGain);
        outGain.setLabel("Power Amp");
        outGain.setTooltip("Power amp stage. In Channel mode, fully bypassed at 0.\n\nAlt/Option-click to enable Auto Gain.");
        outGain.setValueToStringFunction(zeroToTen);

        addAndMakeVisible(mode);
        modeAttach = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(a, "mode", mode);

        addChildComponent(guitarMode);
        addChildComponent(bassMode);
        addChildComponent(channelMode);
        gtrModeAttach = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(a, "guitarMode", guitarMode);
        bassModeAttach = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(a, "bassMode", bassMode);
        chanModeAttach = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(a, "channelMode", channelMode);
        setSubModePtr();

        setColorScheme();

        addAndMakeVisible(autoGain);
        autoGainAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(a, "ampAutoGain", autoGain);
        autoGain.setButtonText("Auto");
        autoGain.setTooltip("Enable auto gain compensation for the entire amp section.");

        addAndMakeVisible(hiGain);
        hiGainAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(a, "hiGain", hiGain);
        hiGain.setButtonText("Boost");
        hiGain.setTooltip("Boost for the preamp stage. In addition to adding volume, it also adds new tubes to the circuit.");

        addAndMakeVisible(power);
        powerAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(a, "ampOn", power);

        startTimerHz(30);
    }

    ~AmpControls()
    {
        subMode = nullptr;
        stopTimer();
    }

    void setSubModePtr()
    {
        Rectangle<int> bounds;
        if (subMode != nullptr)
        {
            subMode->setVisible(false);
            bounds = subMode->getBounds();
        }
        switch ((int)*mode_p)
        {
        case 0:
            subMode = &guitarMode;
            subMode_p = vts.getRawParameterValue("guitarMode");
            break;
        case 1:
            subMode = &bassMode;
            subMode_p = vts.getRawParameterValue("bassMode");
            break;
        case 2:
            subMode = &channelMode;
            subMode_p = vts.getRawParameterValue("channelMode");
            break;
        }
        subMode->setVisible(true);
        subMode->setBounds(bounds);
        lastSubMode = (int)*subMode_p;
    }

    void timerCallback() override
    {
        if (*mode_p == lastMode && *subMode_p == lastSubMode)
            return;

        int currentMode = (int)*mode_p;

        setSubModePtr();

        std::function<String(float)> function;
        if (currentMode < 2)
        {
            function = [](float val)
            {
                return String(val * 10.0, 1);
            };
        }
        else
        {
            function = [](float val)
            {
                val = jmap(val, -6.f, 6.f);
                String str(val, 1);
                str.append("dB", 2);
                return str;
            };
        }

        bass.setValueToStringFunction(function);
        mid.setValueToStringFunction(function);
        treble.setValueToStringFunction(function);

        setColorScheme();

        lastMode = currentMode;

        repaint(getLocalBounds());
    }

    void setColorScheme()
    {
        using namespace Processors;
        switch ((ProcessorType)mode_p->load())
        {
        case ProcessorType::Guitar:
        {
            switch ((GuitarMode)subMode_p->load())
            {
            case (GuitarMode::GammaRay):
                backgroundColor = Colour(GAMMA_RAY);
                break;
            case (GuitarMode::Sunbeam):
                backgroundColor = Colour(SUNBEAM);
                break;
            case (GuitarMode::Moonbeam):
                backgroundColor = Colour(MOONBEAM);
                break;
            case (GuitarMode::XRay):
                backgroundColor = Colour(XRAY);
                break;
            }
            secondaryColor = Colour(BACKGROUND_COLOR).contrasting(1.f);
            for (auto &k : getKnobs())
            {
                k->setColor(Colours::antiquewhite, secondaryColor);
                k->repaint();
            }
        }
        break;
        case Processors::ProcessorType::Bass:
            switch ((BassMode)subMode_p->load())
            {
            case Cobalt:
                backgroundColor = Colour(COBALT);
                break;
            case Emerald:
                backgroundColor = Colour(EMERALD);
                break;
            case Quartz:
                backgroundColor = Colour(QUARTZ);
                break;
            }
            secondaryColor = Colours::lightgrey;
            for (auto &k : getKnobs())
            {
                k->setColor(Colours::black, secondaryColor);
                k->repaint();
            }
            break;
        case Processors::ProcessorType::Channel:
            backgroundColor = Colours::darkgrey;
            secondaryColor = Colours::oldlace;
            if ((ChannelMode)subMode_p->load() == Modern)
            {
                for (auto &k : getKnobs())
                {
                    if (k == &bass)
                        k->setColor(Colour(142.f/360.f, 0.42f, 0.50f, 1.f), secondaryColor);
                    else if (k == &mid)
                        k->setColor(Colour(200.f/360.f, 0.42f, 0.50f, 1.f), secondaryColor);
                    else if (k == &treble)
                        k->setColor(Colour(345.f/360.f, 0.42f, 0.50f, 1.f), secondaryColor);
                    else
                        k->setColor(Colours::slategrey, secondaryColor);

                    k->repaint();
                }
            }
            else
            {
                for (auto &k : getKnobs())
                {
                    if (k == &inGain || k == &outGain)
                        k->setColor(Colour(345.f/360.f, 0.42f, 0.50f, 1.f), secondaryColor);
                    else
                        k->setColor(Colours::lightgrey, secondaryColor);
                    k->repaint();
                }
            }
            break;
        };

        power.setBackgroundColor(backgroundColor.contrasting(0.2f));
    }

    void paint(Graphics &g) override
    {
        ColourGradient grad(backgroundColor, getLocalBounds().getCentreX(), getLocalBounds().getCentreY(), Colour(BACKGROUND_COLOR), getWidth(), getHeight(), true);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(2.f), 5.f);
        g.setColour(secondaryColor);
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(2.f), 5.f, 2.f);
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        auto mb = bounds.removeFromTop(bounds.getHeight() * 0.7f)/* .reduced(5) */; // bounds becomes bottom section
        auto chunk = mb.getWidth() / getKnobs().size(); // knob chunk

        for (auto &k : getKnobs())
        {
            k->setBounds(mb.removeFromLeft(chunk).reduced(5));
            k->setOffset(0, -5);
        }

        // bounds at this point is bottom 30% 
        auto botItemBounds = bounds.withTrimmedRight(chunk); // reduce by knob-width on either side
        float botChunk = botItemBounds.getWidth() / 5;
        auto autoBounds = botItemBounds.removeFromLeft(botChunk).reduced(botChunk * 0.1f);
        auto boostBounds = botItemBounds.removeFromLeft(botChunk).reduced(botChunk * 0.1f);
        auto modeBounds = botItemBounds.removeFromLeft(botChunk).reduced(botChunk * 0.1f);
        auto subModeBounds = botItemBounds.removeFromLeft(botChunk).reduced(botChunk * 0.1f);
        auto powerBounds = botItemBounds.removeFromLeft(botChunk).reduced(5);
        powerBounds.setWidth(powerBounds.getHeight());

        autoGain.setBounds(autoBounds);
        autoGain.lnf.cornerRadius = autoGain.getHeight() * 0.25f;
        hiGain.setBounds(boostBounds);
        hiGain.lnf.cornerRadius = hiGain.getHeight() * 0.25f;
        mode.setBounds(modeBounds);
        subMode->setBounds(subModeBounds);
        power.setBounds(powerBounds);
    }

private:
    AudioProcessorValueTreeState &vts;

    Knob::flags_t knobFlags = Knob::DRAW_GRADIENT | Knob::DRAW_TICKS | Knob::DRAW_SHADOW;
    Knob dist{knobFlags}, inGain{knobFlags}, outGain{knobFlags}, bass{knobFlags}, mid{knobFlags}, treble{knobFlags};
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> distAttach, inGainAttach, outGainAttach, bassAttach, midAttach, trebleAttach;

    ChoiceMenu mode, guitarMode, bassMode, channelMode;
    std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> modeAttach, gtrModeAttach, bassModeAttach, chanModeAttach;
    ChoiceMenu *subMode = nullptr;

    LightButton hiGain, autoGain;
    PowerButton power;
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> hiGainAttach, autoGainAttach, powerAttach;

    Colour backgroundColor = Colours::black;
    Colour secondaryColor = Colours::white;

    std::atomic<float> *mode_p = nullptr, *subMode_p = nullptr;
    int lastMode = 0, lastSubMode = 0;

    std::vector<Knob *> getKnobs()
    {
        return {
            &dist,
            &inGain,
            &bass,
            &mid,
            &treble,
            &outGain};
    }
};
