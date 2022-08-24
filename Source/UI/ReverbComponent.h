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

class ReverbComponent : public Component
{
    Knob reverbAmount{KnobType::Simple};
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> amtAttach;

    std::array<ReverbButton, 2> reverb{ReverbButton::Placement::Left, ReverbButton::Placement::Right};

    std::array<String, 2> names{"Room", "Hall"};

public:
    ReverbComponent(AudioProcessorValueTreeState &v)
    {
        addAndMakeVisible(reverbAmount);
        amtAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(v, "roomAmt", reverbAmount);
        reverbAmount.setLabel("Amount");

        for (int i = 0; i < reverb.size(); ++i)
        {
            addAndMakeVisible(reverb[i]);
            reverb[i].setColour(TextButton::ColourIds::buttonColourId, Colours::grey);
            reverb[i].setColour(TextButton::ColourIds::buttonOnColourId, Colours::darkslategrey);

            reverb[i].setButtonText(names[i]);

            reverb[i].setClickingTogglesState(true);
            reverb[i].setToggleState(i + 1 == *v.getRawParameterValue("reverbType"), NotificationType::sendNotification);
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
    }

    void paint(Graphics &g) override
    {
        ColourGradient gradient{Colour(0xff256184), (float)getLocalBounds().getCentreX(), (float)getLocalBounds().getCentreY(), Colour(0xff256184).withMultipliedBrightness(0.75f), 25.f, 25.f, true};

        g.setGradientFill(gradient);
        g.fillAll();
    }

    void resized() override
    {
        auto b = getLocalBounds().reduced(10);
        reverbAmount.setBounds(b.removeFromBottom(b.getHeight() / 2));

        auto half = b.getWidth() / reverb.size();
        reverb[0].setBounds(b.removeFromLeft(half));
        reverb[1].setBounds(b);
    }
};