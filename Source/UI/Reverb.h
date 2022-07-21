// Reverb.h

#pragma once

class ReverbComponent : public Component
{
    Knob reverbAmount;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> amtAttach;

    TextButton room{"Room"};

    TextButton hall{"Hall"};

public:
    ReverbComponent(AudioProcessorValueTreeState& v) : reverbAmount(Knob::Type::Regular)
    {
        addAndMakeVisible(reverbAmount);
        amtAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(v, "roomAmt", reverbAmount);

        addAndMakeVisible(room);
        room.setClickingTogglesState(true);
        addAndMakeVisible(hall);
        hall.setClickingTogglesState(true);

        hall.onClick = [&]
        {
            v.getParameterAsValue("roomOn") = hall.getToggleState();
        };
    }

    void paint(Graphics& g) override
    {
        g.fillAll(Colour(0xff256184).withAlpha(0.5f));
    }

    void resized() override
    {
        auto b = getLocalBounds();
        reverbAmount.setBounds(b.removeFromBottom(b.getHeight() / 2));
        room.setBounds(b.removeFromLeft(b.getWidth() / 2));
        hall.setBounds(b);
    }
};