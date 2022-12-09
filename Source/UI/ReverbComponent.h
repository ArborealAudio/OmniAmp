// ReverbComponent.h

#pragma once

class ReverbComponent : public Component
{
    Knob::flags_t knobFlags = 0;
    Knob reverbAmount{knobFlags}, reverbDecay{knobFlags}, reverbSize{knobFlags}, predelay{knobFlags};
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> amtAttach, decayAttach, sizeAttach, predelayAttach;

    ChoiceMenu reverb;
    std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> reverbAttach;

public:
    ReverbComponent(AudioProcessorValueTreeState &v) : reverb(v.getParameter("reverbType")->getAllValueStrings())
    {
        addAndMakeVisible(reverbAmount);
        amtAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(v, "reverbAmt", reverbAmount);
        reverbAmount.setLabel("Amount");
        reverbAmount.setTooltip("Mix for reverb. At 50%, wet and dry signals are both at full volume, and the dry signal begins to decrease after 50%");
        reverbAmount.setValueToStringFunction([](float val)
                                              { auto str = String(val * 100.0, 0); str.append("%", 1); return str; });

        addAndMakeVisible(reverbDecay);
        decayAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(v, "reverbDecay", reverbDecay);
        reverbDecay.setLabel("Decay");
        reverbDecay.setTooltip("Decay control for the reverb time");
        reverbDecay.setValueToStringFunction([](float val)
                                              { auto str = String(val * 100.0, 0); str.append("%", 1); return str; });

        addAndMakeVisible(reverbSize);
        sizeAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(v, "reverbSize", reverbSize);
        reverbSize.setLabel("Size");
        reverbSize.setTooltip("Control for the size of the reverb algorithm");
        reverbSize.setValueToStringFunction([](float val)
                                              { auto str = String(val * 100.0, 0); str.append("%", 1); return str; });

        addAndMakeVisible(predelay);
        predelayAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(v, "reverbPredelay", predelay);
        predelay.setLabel("Predelay");
        predelay.setTooltip("Predelay in ms of reverb");
        predelay.setValueToStringFunction([](float val)
                                          { auto str = String((int)val); str.append("ms", 2); return str; });

        addAndMakeVisible(reverb);
        reverbAttach = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(v, "reverbType", reverb);
    }

    void paint(Graphics &g) override
    {
        g.setColour(Colour(0xff256184));
        g.drawRoundedRectangle(getLocalBounds().reduced(1).toFloat(), 5.f, 3.f);
        reverb.lnf.backgroundColor = reverb.getSelectedId() > 1 ? Colour(0xff256184) : Colours::grey;
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced(10);
        auto bottom = b.removeFromBottom(b.getHeight() / 2);
        auto chunk = b.getWidth() / 4;
        auto amountBounds = bottom.removeFromRight(chunk);
        auto sizeBounds = bottom.removeFromRight(chunk);
        auto decayBounds = bottom.removeFromRight(chunk);
        auto predelayBounds = bottom;
        reverbAmount.setBounds(amountBounds);
        reverbSize.setBounds(sizeBounds);
        reverbDecay.setBounds(decayBounds);
        predelay.setBounds(predelayBounds);

        reverb.setBounds(b.reduced(30 , 15));
    }
};