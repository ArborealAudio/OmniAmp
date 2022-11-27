// AmpControls.h

#pragma once

struct AmpControls : Component, private Timer
{
    AmpControls(AudioProcessorValueTreeState &a) : vts(a),
                                                   mode(static_cast<strix::ChoiceParameter *>(vts.getParameter("mode"))->getAllValueStrings()),
                                                   subMode(static_cast<strix::ChoiceParameter *>(vts.getParameter("guitarMode"))->getAllValueStrings())
    {
        for (auto &k : getKnobs())
            addAndMakeVisible(*k);

        mode_p = vts.getRawParameterValue("mode");

        setColorScheme((int)*mode_p);

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

        auto percent = [](float val)
        {
            auto str = String(val * 100.0, 0);
            str.append("%", 1);
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

        addAndMakeVisible(subMode);
        subModeAttach = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(a, "guitarMode", subMode);

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
        stopTimer();
    }

    void timerCallback() override
    {
        if (*mode_p == lastMode)
            return;

        int currentMode = *mode_p;

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

        setColorScheme(currentMode);

        lastMode = currentMode;

        repaint(getLocalBounds());
    }

    void setColorScheme(int m)
    {
        switch (m)
        {
        case 0:
            backgroundColor = Colour(AMP_COLOR).darker(0.1f);
            secondaryColor = backgroundColor.contrasting(0.5f);
            for (auto &k : getKnobs())
            {
                k->setColor(Colours::antiquewhite, secondaryColor);
                k->repaint();
            }
            break;
        case 1:
            backgroundColor = Colours::slategrey;
            secondaryColor = Colours::lightgrey;
            for (auto &k : getKnobs())
            {
                k->setColor(Colours::black, secondaryColor);
                k->repaint();
            }
            break;
        case 2:
            backgroundColor = Colours::darkgrey;
            secondaryColor = Colours::oldlace;
            for (auto &k : getKnobs())
            {
                if (k == &bass)
                    k->setColor(Colours::forestgreen, secondaryColor);
                else if (k == &mid)
                    k->setColor(Colours::blue.withMultipliedSaturation(0.5f), secondaryColor);
                else if (k == &treble)
                    k->setColor(Colours::crimson, secondaryColor);
                else
                    k->setColor(Colours::slategrey, secondaryColor);

                k->repaint();
            }
            break;
        default:
            backgroundColor = Colours::darkgrey;
            secondaryColor = Colours::oldlace;
            for (auto &k : getKnobs())
            {
                if (k == &bass)
                    k->setColor(Colours::forestgreen, secondaryColor);
                else if (k == &mid)
                    k->setColor(Colours::blue.withMultipliedSaturation(0.5f), secondaryColor);
                else if (k == &treble)
                    k->setColor(Colours::crimson, secondaryColor);
                else
                    k->setColor(Colours::slategrey, secondaryColor);

                k->repaint();
            }
            break;
        };

        power.setBackgroundColor(backgroundColor);
    }

    void paint(Graphics &g) override
    {
        g.setColour(backgroundColor);
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

        auto mb = bounds.removeFromTop(bounds.getHeight() * 0.7f);
        auto w = mb.getWidth() / getKnobs().size();

        for (auto &k : getKnobs())
            k->setBounds(mb.removeFromLeft(w).reduced(5));

        auto hiGainBounds = bounds.removeFromLeft(w);
        hiGain.setSize(getWidth() * 0.07f, getHeight() * 0.15f);
        hiGain.setCentrePosition(hiGainBounds.getCentreX(), hiGainBounds.getCentreY());

        auto modeBounds = bounds.removeFromLeft(bounds.getWidth() * 0.25f);
        auto subModeBounds = bounds.removeFromLeft(bounds.getWidth() * 0.25f);
        auto powerBounds = bounds.reduced(w * 0.1);

        mode.setSize(getWidth() * 0.125f, getHeight() * 0.15f);
        mode.setCentrePosition(modeBounds.getCentreX(), modeBounds.getCentreY());

        subMode.setSize(getWidth() * 0.125f, getHeight() * 0.15f);
        subMode.setCentrePosition(subModeBounds.getCentreX(), subModeBounds.getCentreY());

        power.setSize(powerBounds.getHeight(), powerBounds.getHeight());
        power.setCentrePosition(powerBounds.getCentreX(), powerBounds.getCentreY());
    }

private:
    AudioProcessorValueTreeState &vts;

    Knob inGain{KnobType::Amp}, outGain{KnobType::Amp}, bass{KnobType::Amp}, mid{KnobType::Amp}, treble{KnobType::Amp};
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> inGainAttach, outGainAttach, bassAttach, midAttach, trebleAttach;

    ChoiceMenu mode, subMode;
    std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> modeAttach, subModeAttach;

    LightButton hiGain;
    PowerButton power;
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> hiGainAttach, powerAttach;

    Colour backgroundColor = Colours::black;
    Colour secondaryColor = Colours::white;

    std::atomic<float> *mode_p;
    int lastMode = 0;

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
