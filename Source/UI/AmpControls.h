// AmpControls.h

#pragma once

struct AmpControls : Component
{
    AmpControls(AudioProcessorValueTreeState& a) : apvts(a)
    {
        for (auto& k : getKnobs())
            addAndMakeVisible(k);

        compAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "comp", comp);
        comp.setLabel("Opto");

        distAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "dist", dist);
        dist.setLabel("Pedal");

        inGainAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "inputGain", inGain);
        inGain.setLabel("Preamp");
        inGain.onAltClick = [&](bool state)
        {
            apvts.getParameterAsValue("inputAutoGain") = state;
            repaint();
        };

        bassAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "bass", bass);
        bass.setLabel("Bass");
        bass.onAltClick = [&](bool state)
        {
            mid.autogain.store(state);
            treble.autogain.store(state);
            apvts.getParameterAsValue("eqAutoGain") = state;
            repaint();
        };

        midAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "mid", mid);
        mid.setLabel("Mid");
        mid.onAltClick = [&](bool state)
        {
            bass.autogain.store(state);
            treble.autogain.store(state);
            apvts.getParameterAsValue("eqAutoGain") = state;
            repaint();
        };

        trebleAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "treble", treble);
        treble.setLabel("Treble");
        treble.onAltClick = [&](bool state)
        {
            mid.autogain.store(state);
            bass.autogain.store(state);
            apvts.getParameterAsValue("eqAutoGain") = state;
            repaint();
        };

        outGainAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "outputGain", outGain);
        outGain.setLabel("Power Amp");
        outGain.onAltClick = [&](bool state)
        {
            apvts.getParameterAsValue("outputAutoGain") = state;
            repaint();
        };

        addAndMakeVisible(hiGain);
        hiGainAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(apvts, "hiGain", hiGain);
        hiGain.setButtonText("Boost");
    }

    void paint(Graphics& g) override
    {
        g.setColour(Colour(AMP_COLOR));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 5.f);

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

        hiGain.setBounds(inGain.getX() + 15, inGain.getY() - 10, inGain.getWidth() - 15, (float)getHeight() * 0.3 - 15);
    }

private:
    AudioProcessorValueTreeState& apvts;

    Knob comp{KnobType::Regular}, dist{KnobType::Regular}, inGain{KnobType::Regular}, outGain{KnobType::Regular}, bass{KnobType::Regular}, mid{KnobType::Regular}, treble{KnobType::Regular};
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> compAttach, distAttach, inGainAttach, outGainAttach, bassAttach, midAttach, trebleAttach;

    LightButton hiGain;
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> hiGainAttach;

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
