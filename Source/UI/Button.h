// Button.h

#pragma once

struct ButtonLookAndFeel : LookAndFeel_V4
{
    ButtonLookAndFeel() = default;

    void drawButtonBackground(Graphics& g, Button& button, const Colour& backgroundColour, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        g.setColour(Colours::grey);
        g.drawRoundedRectangle(button.getLocalBounds().reduced(3).toFloat(), 10.f, 1.f);
        if (button.getToggleState()){
            g.setColour(Colours::white);
            g.fillRoundedRectangle(button.getLocalBounds().reduced(3).toFloat(), 10.f);
        }
    }

    void drawButtonText(Graphics& g, TextButton& button, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto text = button.getButtonText();
        if (button.getToggleState())
            g.setColour(Colours::black);
        else
            g.setColour(Colours::grey);
        g.drawText(text, button.getLocalBounds(), Justification::centred, false);
    }
};

class LightButton : public TextButton
{
    ButtonLookAndFeel lnf;

public:
    LightButton()
    {
        setLookAndFeel(&lnf);
        setClickingTogglesState(true);
    }
    ~LightButton()
    {
        setLookAndFeel(nullptr);
    }
};