// AmpControls.h

#pragma once

struct AmpControls : Component
{
    AmpControls(strix::VolumeMeterSource& vs, AudioProcessorValueTreeState& a) : grMeter(vs, a.getRawParameterValue("comp"))
    {
        for (auto& k : getKnobs())
            addAndMakeVisible(k);

        auto zeroToTen = [](float val)
        {
            return String(val * 10.0, 1);
        };

        auto percent = [](float val)
        {
            auto str = String(val * 100.0, 0);
            str.append("%", 1);
            return str;
        };

        compAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(a, "comp", comp);
        comp.setLabel("Opto");
        comp.valueToString = percent;

        distAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(a, "dist", dist);
        dist.setLabel("Pedal");
        dist.valueToString = zeroToTen;

        inGainAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(a, "preampGain", inGain);
        inGain.setLabel("Preamp");
        inGain.autoGain.store(*a.getRawParameterValue("preampAutoGain"));
        inGain.onAltClick = [&](bool state)
        {
            a.getParameterAsValue("preampAutoGain") = state;
            repaint();
        };
        inGain.valueToString = zeroToTen;

        bassAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(a, "bass", bass);
        bass.setLabel("Bass");
        bass.autoGain.store(*a.getRawParameterValue("eqAutoGain"));
        bass.onAltClick = [&](bool state)
        {
            mid.autoGain.store(state);
            treble.autoGain.store(state);
            a.getParameterAsValue("eqAutoGain") = state;
            repaint();
        };
        bass.valueToString = zeroToTen;

        midAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(a, "mid", mid);
        mid.setLabel("Mid");
        mid.autoGain.store(*a.getRawParameterValue("eqAutoGain"));
        mid.onAltClick = [&](bool state)
        {
            bass.autoGain.store(state);
            treble.autoGain.store(state);
            a.getParameterAsValue("eqAutoGain") = state;
            repaint();
        };
        mid.valueToString = zeroToTen;

        trebleAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(a, "treble", treble);
        treble.setLabel("Treble");
        treble.autoGain.store(*a.getRawParameterValue("eqAutoGain"));
        treble.onAltClick = [&](bool state)
        {
            mid.autoGain.store(state);
            bass.autoGain.store(state);
            a.getParameterAsValue("eqAutoGain") = state;
            repaint();
        };
        treble.valueToString = zeroToTen;

        outGainAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(a, "powerampGain", outGain);
        outGain.setLabel("Power Amp");
        outGain.autoGain.store(*a.getRawParameterValue("powerampAutoGain"));
        outGain.onAltClick = [&](bool state)
        {
            a.getParameterAsValue("powerampAutoGain") = state;
            repaint();
        };
        outGain.valueToString = zeroToTen;

        addAndMakeVisible(mode);
        modeAttach = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(a, "mode", mode);

        addAndMakeVisible(hiGain);
        hiGainAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(a, "hiGain", hiGain);
        hiGain.setButtonText("Boost");

        grMeter.setMeterType(strix::VolumeMeterComponent::Type::Reduction);
        grMeter.setMeterLayout(strix::VolumeMeterComponent::Layout::Horizontal);
        grMeter.setMeterColor(Colours::oldlace);
        addAndMakeVisible(grMeter);
    }

    void paint(Graphics& g) override
    {
        g.setColour(Colour(AMP_COLOR));
        g.fillRoundedRectangle(getLocalBounds().toFloat().reduced(2.f), 5.f);
        g.setColour(Colours::grey);
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(2.f), 5.f, 2.f);

        g.fillRoundedRectangle(dist.getRight() - 1, 20, 2, dist.getHeight() - 20, 20.f);

        auto paintAuto = [&](Rectangle<int> bounds)
        {
            g.setColour(Colours::slategrey);
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

        auto leftSide = bounds.removeFromLeft(w * 2);

        mode.setSize(100, 40);
        mode.setCentrePosition(bounds.getCentreX(), bounds.getBottom() - 25);

        hiGain.setSize(50, 30);
        hiGain.setCentrePosition(inGain.getBounds().getCentreX(), mode.getBounds().getCentreY());

        grMeter.setBounds(leftSide.reduced(10));
    }

private:

    Knob comp{KnobType::Regular}, dist{KnobType::Regular}, inGain{KnobType::Regular}, outGain{KnobType::Regular}, bass{KnobType::Regular}, mid{KnobType::Regular}, treble{KnobType::Regular};
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> compAttach, distAttach, inGainAttach, outGainAttach, bassAttach, midAttach, trebleAttach;

    ChoiceMenu mode;
    std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> modeAttach;

    LightButton hiGain;
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> hiGainAttach;

    strix::VolumeMeterComponent grMeter;

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
