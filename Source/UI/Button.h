// Button.h

#pragma once

struct ButtonLookAndFeel : LookAndFeel_V4
{
    ButtonLookAndFeel() = default;

    void drawButtonBackground(Graphics& g, Button& button, const Colour& backgroundColour, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        float cornerRadius = button.getHeight() * 0.25;
        g.setColour(Colours::white);
        g.drawRoundedRectangle(button.getLocalBounds().reduced(3).toFloat(), cornerRadius, 1.f);
        if (button.getToggleState()){
            g.setColour(Colours::white);
            g.fillRoundedRectangle(button.getLocalBounds().reduced(3).toFloat(), cornerRadius);
        }
    }

    void drawButtonText(Graphics& g, TextButton& button, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        auto text = button.getButtonText();
        if (button.getToggleState())
            g.setColour(Colours::black);
        else
            g.setColour(Colours::white);
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