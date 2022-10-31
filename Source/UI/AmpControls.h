// AmpControls.h

#pragma once

struct PowerButton : TextButton
{
    PowerButton()
    {
        setClickingTogglesState(true);
        // setBufferedToImage(true);
    }

    void paint(Graphics &g) override
    {
        Colour icon, s_backgnd = background;
        auto b = getLocalBounds();
        auto padding = b.getHeight() * 0.1f;

        auto drawPowerIcon = [&](Graphics& gc)
        {
            gc.setColour(icon);
            gc.drawEllipse(b.reduced(padding * 2).toFloat(), 5.f);
            gc.fillRoundedRectangle(b.getCentreX() - 2.5f, padding, 5.f, b.getCentreY() - padding, 2.f);

            // gc.setColour(s_backgnd);
            // gc.drawRoundedRectangle(b.getCentreX() - 3.f, padding, 6.f, b.getCentreY() - padding, 2.f, 2.f);
        };

        if (isMouseOver()) {
            s_backgnd = background.contrasting(0.2f);
            g.setColour(s_backgnd);
            g.fillEllipse(b.reduced(padding).toFloat());
        }

        if (!getToggleState())
            icon = s_backgnd.contrasting(0.2f);
        else {
            icon = Colours::white;
            Image blur(Image::PixelFormat::ARGB, getWidth(), getHeight(), true);
            Graphics g_blur(blur);

            drawPowerIcon(g_blur);
            gin::applyStackBlur(blur, 5);

            g.drawImage(blur, getLocalBounds().toFloat(), RectanglePlacement::centred);
        }

        drawPowerIcon(g);
    }

    void setBackgroundColor(Colour newColor)
    {
        background = newColor;
    }

private:
    Colour background;
};

struct AmpControls : Component, private Timer
{
    AmpControls(strix::VolumeMeterSource& vs, AudioProcessorValueTreeState& a) : vts(a),
    grMeter(vs, a.getRawParameterValue("comp"))
    {
        for (auto& k : getKnobs())
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

        compAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(a, "comp", comp);
        comp.setLabel("Opto");
        comp.setValueToStringFunction(percent);

        distAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(a, "dist", dist);
        dist.setLabel("Pedal");
        dist.setValueToStringFunction(zeroToTen);

        inGainAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(a, "preampGain", inGain);
        inGain.setLabel("Preamp");
        inGain.autoGain.store(*a.getRawParameterValue("preampAutoGain"));
        inGain.onAltClick = [&](bool state)
        {
            a.getParameterAsValue("preampAutoGain") = state;
            repaint();
        };
        inGain.setValueToStringFunction(zeroToTen);

        bassAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(a, "bass", bass);
        bass.setLabel("Bass");
        bass.autoGain.store(*a.getRawParameterValue("eqAutoGain"));
        bass.onAltClick = [&](bool state)
        {
            if (*a.getRawParameterValue("mode") < 2) return;
            mid.autoGain.store(state);
            treble.autoGain.store(state);
            a.getParameterAsValue("eqAutoGain") = state;
            repaint();
        };
        bass.setValueToStringFunction(eqFunc);

        midAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(a, "mid", mid);
        mid.setLabel("Mid");
        mid.autoGain.store(*a.getRawParameterValue("eqAutoGain"));
        mid.onAltClick = [&](bool state)
        {
            if (*a.getRawParameterValue("mode") < 2) return;
            bass.autoGain.store(state);
            treble.autoGain.store(state);
            a.getParameterAsValue("eqAutoGain") = state;
            repaint();
        };
        mid.setValueToStringFunction(eqFunc);

        trebleAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(a, "treble", treble);
        treble.setLabel("Treble");
        treble.autoGain.store(*a.getRawParameterValue("eqAutoGain"));
        treble.onAltClick = [&](bool state)
        {
            if (*a.getRawParameterValue("mode") < 2) return;
            mid.autoGain.store(state);
            bass.autoGain.store(state);
            a.getParameterAsValue("eqAutoGain") = state;
            repaint();
        };
        treble.setValueToStringFunction(eqFunc);

        outGainAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(a, "powerampGain", outGain);
        outGain.setLabel("Power Amp");
        outGain.autoGain.store(*a.getRawParameterValue("powerampAutoGain"));
        outGain.onAltClick = [&](bool state)
        {
            a.getParameterAsValue("powerampAutoGain") = state;
            repaint();
        };
        outGain.setValueToStringFunction(zeroToTen);

        addAndMakeVisible(mode);
        modeAttach = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(a, "mode", mode);

        addAndMakeVisible(hiGain);
        hiGainAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(a, "hiGain", hiGain);
        hiGain.setButtonText("Boost");

        addAndMakeVisible(power);
        powerAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(a, "ampOn", power);

        grMeter.setMeterType(strix::VolumeMeterComponent::Type::Reduction);
        grMeter.setMeterLayout(strix::VolumeMeterComponent::Layout::Horizontal);
        grMeter.setMeterColor(Colours::oldlace);
        addAndMakeVisible(grMeter);

        startTimerHz(30);
    }

    ~AmpControls()
    {
        stopTimer();
    }

    void timerCallback() override
    {
        if (*vts.getRawParameterValue("ampOn"))
            grMeter.setState(vts.getRawParameterValue("comp"));
        else
            grMeter.setState(vts.getRawParameterValue("ampOn"));

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
            for (auto& k : getKnobs())
            {
                if (k == &comp)
                    k->setColor(Colours::wheat, secondaryColor);
                else if (k == &dist)
                    k->setColor(Colours::azure, secondaryColor);
                else
                    k->setColor(Colours::antiquewhite, secondaryColor);
                k->repaint();
            }
            break;
        case 1:
            backgroundColor = Colours::slategrey;
            secondaryColor = Colours::lightgrey;
            for (auto& k : getKnobs())
            {
                if (k == &comp)
                    k->setColor(Colours::wheat.withMultipliedLightness(0.25f), secondaryColor);
                else if (k == &dist)
                    k->setColor(Colours::azure.withMultipliedLightness(0.25f), secondaryColor);
                else
                    k->setColor(Colours::black, secondaryColor);
                k->repaint();
            }
            break;
        case 2:
            backgroundColor = Colours::darkgrey;
            secondaryColor = Colours::oldlace;
            for (auto& k : getKnobs())
            {
                if (k == &comp)
                    k->setColor(Colours::wheat, secondaryColor);
                else if (k == &dist)
                    k->setColor(Colours::azure, secondaryColor);
                else if (k == &bass)
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
            for (auto& k : getKnobs())
            {
                if (k == &comp)
                    k->setColor(Colours::wheat, secondaryColor);
                else if (k == &dist)
                    k->setColor(Colours::azure, secondaryColor);
                else if (k == &bass)
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

    void paint(Graphics& g) override
    {
        g.setColour(backgroundColor);
        g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(2.f), 5.f);
        g.setColour(secondaryColor);
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(2.f), 5.f, 2.f);

        g.fillRoundedRectangle(dist.getRight() - 1, 20, 2, dist.getHeight() - 20, 20.f);

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
        
        if (bass.autoGain.load() || mid.autoGain.load() || treble.autoGain.load())
            paintAuto(Rectangle<int>(toneControls.getX(), toneControls.getBottom() + 3, toneControls.getWidth(), 10));
        
        if (outGain.autoGain.load())
            paintAuto(Rectangle<int>(outGain.getX(), outGain.getBottom() + 3, outGain.getWidth(), 10));
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        auto mb = bounds.removeFromTop(bounds.getHeight() * 0.7f);
        auto w = mb.getWidth() / 7;

        for (auto& k : getKnobs())
            k->setBounds(mb.removeFromLeft(w).reduced(5));

        auto grMeterBounds = bounds.removeFromLeft(w * 2);
        grMeter.setBounds(grMeterBounds.reduced(10));

        auto hiGainBounds = bounds.removeFromLeft(w);
        hiGain.setSize(50, 30);
        hiGain.setCentrePosition(hiGainBounds.getCentreX(), hiGainBounds.getCentreY());

        auto modeBounds = bounds.removeFromLeft(bounds.getWidth() * 0.5);
        auto powerBounds = bounds;

        mode.setSize(100, 40);
        mode.setCentrePosition(modeBounds.getCentreX(), modeBounds.getCentreY());

        power.setSize(powerBounds.getHeight(), powerBounds.getHeight());
        power.setCentrePosition(powerBounds.getCentreX(), powerBounds.getCentreY());
    }

private:
    AudioProcessorValueTreeState& vts;

    Knob comp{KnobType::Regular}, dist{KnobType::Regular}, inGain{KnobType::Regular}, outGain{KnobType::Regular}, bass{KnobType::Regular}, mid{KnobType::Regular}, treble{KnobType::Regular};
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> compAttach, distAttach, inGainAttach, outGainAttach, bassAttach, midAttach, trebleAttach;

    ChoiceMenu mode;
    std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> modeAttach;

    LightButton hiGain;
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> hiGainAttach, powerAttach;

    PowerButton power;

    strix::VolumeMeterComponent grMeter;

    Colour backgroundColor = Colours::black;
    Colour secondaryColor = Colours::white;

    std::atomic<float> *mode_p;
    int lastMode = 0;

    std::vector<Knob*> getKnobs()
    {
        return
        {
            &comp,
            &dist,
            &inGain,
            &bass,
            &mid,
            &treble,
            &outGain
        };
    }
};
