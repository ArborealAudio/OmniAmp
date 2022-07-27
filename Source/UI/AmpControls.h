// AmpControls.h

#pragma once

struct AmpControls : Component
{
    AmpControls(AudioProcessorValueTreeState& a) : apvts(a)
    {
        addAndMakeVisible(comp);
        compAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "comp", comp);
        comp.setBounds(20, (getHeight() / 2), 70, 70);
        
        addAndMakeVisible(compText);
        compText.setText("Comp");
        compText.setJustification(Justification::centred);
        compText.setBoundingBox(Parallelogram(comp.getBounds().translated(0, 45).toFloat()));

        addAndMakeVisible(dist);
        distAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "dist", dist);
        dist.setBounds(100, (getHeight() / 2), 70, 70);

        addAndMakeVisible(distText);
        distText.setText("Dist");
        distText.setJustification(Justification::centred);
        distText.setBoundingBox(Parallelogram(dist.getBounds().translated(0, 45).toFloat()));

        addAndMakeVisible(inGain);
        inGainAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "inputGain", inGain);
        inGain.setBounds(180, (getHeight() / 2), 70, 70);

        addAndMakeVisible(preampText);
        preampText.setText("Preamp");
        preampText.setJustification(Justification::centred);
        preampText.setBoundingBox(Parallelogram(inGain.getBounds().translated(0, 45).toFloat()));

        addAndMakeVisible(bass);
        bassAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "bass", bass);
        bass.setBounds(260, (getHeight() / 2), 70, 70);

        addAndMakeVisible(bassText);
        bassText.setText("Bass");
        bassText.setJustification(Justification::centred);
        bassText.setBoundingBox(Parallelogram(bass.getBounds().translated(0, 45).toFloat()));

        addAndMakeVisible(mid);
        midAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "mid", mid);
        mid.setBounds(340, (getHeight() / 2), 70, 70);

        addAndMakeVisible(midText);
        midText.setText("Mid");
        midText.setJustification(Justification::centred);
        midText.setBoundingBox(Parallelogram(mid.getBounds().translated(0, 45).toFloat()));

        addAndMakeVisible(treble);
        trebleAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "treble", treble);
        treble.setBounds(420, (getHeight() / 2), 70, 70);

        addAndMakeVisible(trebText);
        trebText.setText("Treble");
        trebText.setJustification(Justification::centred);
        trebText.setBoundingBox(Parallelogram(treble.getBounds().translated(0, 45).toFloat()));

        addAndMakeVisible(outGain);
        outGainAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "outputGain", outGain);
        outGain.setBounds(500, (getHeight() / 2), 70, 70);

        addAndMakeVisible(powerampText);
        powerampText.setText("Power Amp");
        powerampText.setJustification(Justification::centred);
        powerampText.setBoundingBox(Parallelogram(outGain.getBounds().translated(0, 45).toFloat()));

        addAndMakeVisible(hiGain);
        hiGainAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(apvts, "hiGain", hiGain);
        hiGain.setButtonText("Hi Gain");
        hiGain.setColour(TextButton::buttonColourId, Colours::transparentBlack);
        hiGain.setColour(TextButton::buttonOnColourId, Colours::white);
        hiGain.setColour(TextButton::textColourOffId, Colours::white);
        hiGain.setColour(TextButton::textColourOnId, Colours::black);
        hiGain.setClickingTogglesState(true);
        hiGain.setBounds(inGain.getX(), inGain.getBottom() + 30, 70, 25);
    }

    void paint(Graphics& g) override
    {
        g.setColour(Colours::beige.withAlpha(0.5f));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 5.f);
    }

private:
    AudioProcessorValueTreeState& apvts;

    Knob comp{Knob::Type::Regular}, dist{Knob::Type::Regular}, inGain{Knob::Type::Regular}, outGain{Knob::Type::Regular}, bass{Knob::Type::Regular}, mid{Knob::Type::Regular}, treble{Knob::Type::Regular};
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> compAttach, distAttach, inGainAttach, outGainAttach, bassAttach, midAttach, trebleAttach;

    TextButton hiGain;
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> hiGainAttach;

    DrawableText compText, distText, preampText, bassText, midText, trebText, powerampText;
};
