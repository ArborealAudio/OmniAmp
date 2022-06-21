// AmpControls.h

#pragma once

struct AmpControls : Component
{
    AmpControls(AudioProcessorValueTreeState& a) : apvts(a)
    {
        addAndMakeVisible(comp);
        compAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "comp", comp);
        comp.setBounds(20, (getHeight() / 2), 70, 70);

        addAndMakeVisible(dist);
        distAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "dist", dist);
        dist.setBounds(100, (getHeight() / 2), 70, 70);

        addAndMakeVisible(inGain);
        inGainAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "inputGain", inGain);
        inGain.setBounds(180, (getHeight() / 2), 70, 70);

        addAndMakeVisible(bass);
        bassAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "bass", bass);
        bass.setBounds(260, (getHeight() / 2), 70, 70);

        addAndMakeVisible(mid);
        midAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "mid", mid);
        mid.setBounds(340, (getHeight() / 2), 70, 70);

        addAndMakeVisible(treble);
        trebleAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "treble", treble);
        treble.setBounds(420, (getHeight() / 2), 70, 70);

        addAndMakeVisible(outGain);
        outGainAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "outputGain", outGain);
        outGain.setBounds(500, (getHeight() / 2), 70, 70);

        addAndMakeVisible(hiGain);
        hiGainAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(apvts, "hiGain", hiGain);
        hiGain.setButtonText("Hi Gain");
        hiGain.setColour(TextButton::buttonColourId, Colours::transparentBlack);
        hiGain.setColour(TextButton::buttonOnColourId, Colours::white);
        hiGain.setColour(TextButton::textColourOffId, Colours::white);
        hiGain.setColour(TextButton::textColourOnId, Colours::black);
        hiGain.setClickingTogglesState(true);
        hiGain.setBounds(inGain.getX(), inGain.getBottom() + 2, 70, 25);
    }
    ~AmpControls(){}

    void paint(Graphics& g) override
    {
        g.setColour(Colours::grey);
        g.drawRoundedRectangle(getLocalBounds().toFloat(), 5.f, 3.f);
    }

private:
    AudioProcessorValueTreeState& apvts;

    Knob comp{Knob::Type::Regular}, dist{Knob::Type::Regular}, inGain{Knob::Type::Regular}, outGain{Knob::Type::Regular}, bass{Knob::Type::Regular}, mid{Knob::Type::Regular}, treble{Knob::Type::Regular};
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> compAttach, distAttach, inGainAttach, outGainAttach, bassAttach, midAttach, trebleAttach;

    TextButton hiGain;
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> hiGainAttach;
};
