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

        inGainAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(a, "preampGain", inGain);
        inGain.setLabel("Preamp");
        inGain.setTooltip("Preamp gain stage. In Channel mode, is bypassed at 0.\n\nAlt/Option-click to enable Auto Gain.");
        inGain.autoGain.store(*a.getRawParameterValue("preampAutoGain"));
        inGain.onAltClick = [&](bool state)
        {
            a.getParameterAsValue("preampAutoGain") = state;
            repaint();
        };
        inGain.setValueToStringFunction(zeroToTen);

        String eqTooltip = "EQ section of the amp. In Channel mode, the EQ knobs will cut below 50% and add above 50%.\n\nIn Guitar & Bass mode, these controls will work like a traditional amp tone stack.\n\nIn Channel mode, Alt/Option-click will enable frequency-weighted Auto Gain.";

        bassAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(a, "bass", bass);
        bass.setLabel("Bass");
        bass.setTooltip(eqTooltip);
        bass.autoGain = *a.getRawParameterValue("eqAutoGain");
        bass.onAltClick = [&](bool state)
        {
            if (*a.getRawParameterValue("mode") < 2)
                return;
            mid.autoGain.store(state);
            treble.autoGain.store(state);
            a.getParameterAsValue("eqAutoGain") = state;
            repaint();
        };
        bass.setValueToStringFunction(eqFunc);

        midAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(a, "mid", mid);
        mid.setLabel("Mid");
        mid.setTooltip(eqTooltip);
        mid.autoGain = *a.getRawParameterValue("eqAutoGain");
        mid.onAltClick = [&](bool state)
        {
            if (*a.getRawParameterValue("mode") < 2)
                return;
            bass.autoGain.store(state);
            treble.autoGain.store(state);
            a.getParameterAsValue("eqAutoGain") = state;
            repaint();
        };
        mid.setValueToStringFunction(eqFunc);

        trebleAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(a, "treble", treble);
        treble.setLabel("Treble");
        treble.setTooltip(eqTooltip);
        treble.autoGain = *a.getRawParameterValue("eqAutoGain");
        treble.onAltClick = [&](bool state)
        {
            if (*a.getRawParameterValue("mode") < 2)
                return;
            mid.autoGain.store(state);
            bass.autoGain.store(state);
            a.getParameterAsValue("eqAutoGain") = state;
            repaint();
        };
        treble.setValueToStringFunction(eqFunc);

        outGainAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(a, "powerampGain", outGain);
        outGain.setLabel("Power Amp");
        outGain.setTooltip("Power amp stage. In Channel mode, fully bypassed at 0.\n\nAlt/Option-click to enable Auto Gain.");
        outGain.autoGain.store(*a.getRawParameterValue("powerampAutoGain"));
        outGain.onAltClick = [&](bool state)
        {
            a.getParameterAsValue("powerampAutoGain") = state;
            repaint();
        };
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

        bass.autoGain.store(currentMode > 1 && *vts.getRawParameterValue("eqAutoGain"));
        mid.autoGain.store(currentMode > 1 && *vts.getRawParameterValue("eqAutoGain"));
        treble.autoGain.store(currentMode > 1 && *vts.getRawParameterValue("eqAutoGain"));

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
            if ((ChannelMode)subMode_p->load() == A)
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
        // g.setColour(backgroundColor);
        ColourGradient grad(backgroundColor, getLocalBounds().getCentreX(), getLocalBounds().getCentreY(), Colour(BACKGROUND_COLOR), getWidth(), getHeight(), true);
        g.setGradientFill(grad);
        g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(2.f), 5.f);
        g.setColour(secondaryColor);
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(2.f), 5.f, 2.f);

        // g.fillRoundedRectangle(dist.getRight() - 1, 20, 2, dist.getHeight() - 20, 20.f);

        auto paintAuto = [&](Rectangle<int> bounds)
        {
            g.setColour(secondaryColor);
            g.drawText("Auto", bounds, Justification::centred, false);

            Path p;
            p.startNewSubPath(bounds.getX(), bounds.getY());
            p.lineTo(bounds.getX(), bounds.getBottom());
            p.lineTo(bounds.getX() + bounds.getWidth(), bounds.getBottom());
            p.lineTo(bounds.getX() + bounds.getWidth(), bounds.getY());

            g.strokePath(p, PathStrokeType(1.f));
        };

        if (inGain.autoGain.load())
            paintAuto(Rectangle<int>(inGain.getX(), inGain.getBottom() + 3, inGain.getWidth(), 10));

        auto toneControls = bass.getBounds().getUnion(mid.getBounds()).getUnion(treble.getBounds());

        if ((bass.autoGain.load() || mid.autoGain.load() || treble.autoGain.load()) && lastMode > 1)
            paintAuto(Rectangle<int>(toneControls.getX(), toneControls.getBottom() + 3, toneControls.getWidth(), 10));

        if (outGain.autoGain.load())
            paintAuto(Rectangle<int>(outGain.getX(), outGain.getBottom() + 3, outGain.getWidth(), 10));
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        auto mb = bounds.removeFromTop(bounds.getHeight() * 0.7f); // bounds becomes bottom section
        auto w = mb.getWidth() / getKnobs().size(); // knob chunk

        for (auto &k : getKnobs())
        {
            k->setBounds(mb.removeFromLeft(w).reduced(5));
            k->setOffset(0, -5);
        }

        auto hiGainBounds = bounds.removeFromLeft(w);
        hiGain.setSize(getWidth() * 0.07f, getHeight() * 0.15f);
        hiGain.setCentrePosition(hiGainBounds.getCentreX(), hiGainBounds.getCentreY());

        // bounds is bottom w/ boost removed from left
        auto botItemBounds = bounds.reduced(w * 0.5f, 0);
        float third = botItemBounds.getWidth() / 3;
        auto modeBounds = botItemBounds.removeFromLeft(third);
        auto subModeBounds = botItemBounds.removeFromLeft(third);
        auto powerBounds = botItemBounds.removeFromLeft(third);

        mode.setSize(getWidth() * 0.15f, getHeight() * 0.15f);
        mode.setCentrePosition(modeBounds.getCentreX(), modeBounds.getCentreY());

        subMode->setSize(getWidth() * 0.15f, getHeight() * 0.15f);
        subMode->setCentrePosition(subModeBounds.getCentreX(), subModeBounds.getCentreY());

        power.setSize(powerBounds.getHeight(), powerBounds.getHeight());
        power.setCentrePosition(powerBounds.getCentreX(), powerBounds.getCentreY());
    }

private:
    AudioProcessorValueTreeState &vts;

    Knob::flags_t knobFlags = Knob::DRAW_GRADIENT | Knob::DRAW_TICKS | Knob::DRAW_SHADOW;

    Knob inGain{knobFlags}, outGain{knobFlags}, bass{knobFlags}, mid{knobFlags}, treble{knobFlags};
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> inGainAttach, outGainAttach, bassAttach, midAttach, trebleAttach;

    ChoiceMenu mode, guitarMode, bassMode, channelMode;
    std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> modeAttach, gtrModeAttach, bassModeAttach, chanModeAttach;
    ChoiceMenu *subMode = nullptr;

    LightButton hiGain;
    PowerButton power;
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> hiGainAttach, powerAttach;

    Colour backgroundColor = Colours::black;
    Colour secondaryColor = Colours::white;

    std::atomic<float> *mode_p = nullptr, *subMode_p = nullptr;
    int lastMode = 0, lastSubMode = 0;

    std::vector<Knob *> getKnobs()
    {
        return {
            &inGain,
            &bass,
            &mid,
            &treble,
            &outGain};
    }
};
