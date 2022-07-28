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

        bassAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "bass", bass);
        bass.setLabel("Bass");

        midAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "mid", mid);
        mid.setLabel("Mid");

        trebleAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "treble", treble);
        treble.setLabel("Treble");

        outGainAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "outputGain", outGain);
        outGain.setLabel("Power Amp");

        addAndMakeVisible(hiGain);
        hiGainAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(apvts, "hiGain", hiGain);
        hiGain.setButtonText("Hi Gain");
        hiGain.setColour(TextButton::buttonColourId, Colours::transparentBlack);
        hiGain.setColour(TextButton::buttonOnColourId, Colours::white);
        hiGain.setColour(TextButton::textColourOffId, Colours::white);
        hiGain.setColour(TextButton::textColourOnId, Colours::black);
        hiGain.setClickingTogglesState(true);
    }

    void paint(Graphics& g) override
    {
        g.setColour(Colours::beige.withAlpha(0.5f));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 5.f);
    }

    void resized() override
    {
        auto mb = getLocalBounds().removeFromTop(getHeight() * 0.7);
        auto w = mb.getWidth() / 7;

        for (auto& k : getKnobs())
            k->setBounds(mb.removeFromLeft(w));

        hiGain.setBounds(inGain.getX(), inGain.getBottom() + 10, inGain.getWidth(), getHeight() * 0.3 - 10);
    }

private:
    AudioProcessorValueTreeState& apvts;

    Knob comp{KnobType::Regular}, dist{KnobType::Regular}, inGain{KnobType::Regular}, outGain{KnobType::Regular}, bass{KnobType::Regular}, mid{KnobType::Regular}, treble{KnobType::Regular};
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> compAttach, distAttach, inGainAttach, outGainAttach, bassAttach, midAttach, trebleAttach;

    TextButton hiGain;
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
