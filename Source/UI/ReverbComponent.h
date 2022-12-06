// ReverbComponent.h

#pragma once

struct ReverbButton : TextButton
{
    enum Placement
    {
        Left,
        Center,
        Right
    };

    struct LookAndFeelMethods : LookAndFeel_V4
    {
        void drawButtonBackground(Graphics &g, Button &button, const Colour &backgroundColour, bool drawAsHighlighted, bool drawAsDown) override
        {
            auto bounds = button.getLocalBounds().reduced(5, 15);

#define DIMENSIONS bounds.getX(), bounds.getY(), bounds.getWidth(), bounds.getHeight()
#define CORNERS 5.f, 5.f

            Path p;

            switch (placement)
            {
            case Left:
                p.addRoundedRectangle(DIMENSIONS, CORNERS, true, false, true, false);
                break;
            case Center:
                p.addRectangle(DIMENSIONS);
                break;
            case Right:
                p.addRoundedRectangle(DIMENSIONS, CORNERS, false, true, false, true);
                break;
            default:
                p.addRectangle(DIMENSIONS);
                break;
            }

            g.setColour(Colours::black);
            g.strokePath(p, PathStrokeType(1.f));

            g.setColour(drawAsHighlighted ? backgroundColour.withMultipliedBrightness(1.5f) : backgroundColour);
            g.fillPath(p);
        }

        void setPlacement(Placement newPlacement) { placement = newPlacement; }

    private:
        Placement placement = Placement::Center;
    };

    ReverbButton(Placement p) : placement(p)
    {
        setLookAndFeel(&lnf);
        lnf.setPlacement(placement);
    }

    ~ReverbButton()
    {
        setLookAndFeel(nullptr);
    }

protected:
    Placement placement;
    LookAndFeelMethods lnf;
};

class ReverbComponent : public Component, private Timer
{
    Knob reverbAmount{KnobType::Simple}, reverbDecay{KnobType::Simple}, reverbSize{KnobType::Simple};
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> amtAttach, decayAttach, sizeAttach;

    std::array<ReverbButton, 2> reverb{ReverbButton::Placement::Left, ReverbButton::Placement::Right};

    std::array<String, 2> names{"Room", "Hall"};

    std::atomic<float> *reverbType;
    int lastType = 0;

public:
    ReverbComponent(AudioProcessorValueTreeState &v)
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

        reverbType = v.getRawParameterValue("reverbType");

        for (int i = 0; i < reverb.size(); ++i)
        {
            addAndMakeVisible(reverb[i]);
            reverb[i].setColour(TextButton::ColourIds::buttonColourId, Colours::grey);
            reverb[i].setColour(TextButton::ColourIds::buttonOnColourId, Colours::darkslategrey);

            reverb[i].setButtonText(names[i]);

            reverb[i].setClickingTogglesState(true);
            reverb[i].setToggleState(i + 1 == (int)*reverbType, NotificationType::sendNotification);
        }

        reverb[0].onClick = [&]
        {
            if (reverb[0].getToggleState())
            {
                reverb[1].setToggleState(false, NotificationType::dontSendNotification);
                v.getParameterAsValue("reverbType") = 1;
            }
            else
                v.getParameterAsValue("reverbType") = 0;
        };
        reverb[1].onClick = [&]
        {
            if (reverb[1].getToggleState())
            {
                reverb[0].setToggleState(false, NotificationType::dontSendNotification);
                v.getParameterAsValue("reverbType") = 2;
            }
            else
                v.getParameterAsValue("reverbType") = 0;
        };

        startTimerHz(15);
    }

    ~ReverbComponent()
    {
        stopTimer();
    }

    void updateState()
    {
        for (int i = 0; i < reverb.size(); ++i)
            reverb[i].setToggleState(i + 1 == (int)*reverbType, NotificationType::dontSendNotification);
    }

    void paint(Graphics &g) override
    {
        g.setColour(Colour(0xff256184));
        g.drawRoundedRectangle(getLocalBounds().reduced(1).toFloat(), 5.f, 3.f);
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced(10);
        {
            auto bottom = b.removeFromBottom(b.getHeight() / 2);
            auto chunk = b.getWidth() / 3;
            auto amountBounds = bottom.removeFromRight(chunk);
            auto sizeBounds = bottom.removeFromRight(chunk);
            auto decayBounds = bottom;
            reverbAmount.setBounds(amountBounds);
            reverbSize.setBounds(sizeBounds);
            reverbDecay.setBounds(decayBounds);
        }

        auto half = b.getWidth() / reverb.size();
        reverb[0].setBounds(b.removeFromLeft(half));
        reverb[1].setBounds(b);
    }

    void timerCallback() override
    {
        if (lastType != (int)*reverbType)
            updateState();
        lastType = (int)*reverbType;
    }
};