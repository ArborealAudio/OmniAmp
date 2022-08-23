// AmpControls.h

#pragma once

struct AmpControls : Component
{
    AmpControls(strix::VolumeMeterSource& vs, AudioProcessorValueTreeState& a) : grMeter(vs, a.getRawParameterValue("comp"))
    {
        for (auto& k : getKnobs())
            addAndMakeVisible(k);

        compAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(a, "comp", comp);
        comp.setLabel("Opto");

        distAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(a, "dist", dist);
        dist.setLabel("Pedal");

        inGainAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(a, "preampGain", inGain);
        inGain.setLabel("Preamp");
        inGain.onAltClick = [&](bool state)
        {
            a.getParameterAsValue("preampAutoGain") = state;
            repaint();
        };

        bassAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(a, "bass", bass);
        bass.setLabel("Bass");
        bass.onAltClick = [&](bool state)
        {
            mid.autogain.store(state);
            treble.autogain.store(state);
            a.getParameterAsValue("eqAutoGain") = state;
            repaint();
        };

        midAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(a, "mid", mid);
        mid.setLabel("Mid");
        mid.onAltClick = [&](bool state)
        {
            bass.autogain.store(state);
            treble.autogain.store(state);
            a.getParameterAsValue("eqAutoGain") = state;
            repaint();
        };

        trebleAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(a, "treble", treble);
        treble.setLabel("Treble");
        treble.onAltClick = [&](bool state)
        {
            mid.autogain.store(state);
            bass.autogain.store(state);
            a.getParameterAsValue("eqAutoGain") = state;
            repaint();
        };

        outGainAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(a, "powerampGain", outGain);
        outGain.setLabel("Power Amp");
        outGain.onAltClick = [&](bool state)
        {
            a.getParameterAsValue("powerampAutoGain") = state;
            repaint();
        };

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

        if (inGain.autogain.load())
            paintAuto(Rectangle<int>(inGain.getX(), inGain.getBottom() + 3, inGain.getWidth(), 10));

        auto toneControls = bass.getBounds().getUnion(mid.getBounds()).getUnion(treble.getBounds());
        
        if (bass.autogain.load() || mid.autogain.load() || treble.autogain.load())
            paintAuto(Rectangle<int>(toneControls.getX(), toneControls.getBottom() + 3, toneControls.getWidth(), 10));
        
        if (outGain.autogain.load())
            paintAuto(Rectangle<int>(outGain.getX(), outGain.getBottom() + 3, outGain.getWidth(), 10));
    }

    void resized() override
    {
        auto bounds = getLocalBounds();

        auto mb = bounds.removeFromTop(bounds.getHeight() * 0.7f);
        auto w = mb.getWidth() / 7;

        for (auto& k : getKnobs())
            k->setBounds(mb.removeFromLeft(w));

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
