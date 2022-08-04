// Button.h

#pragma once

struct ButtonLookAndFeel : LookAndFeel_V4
{
    ButtonLookAndFeel() = default;

    // void drawButtonBackground(Graphics& g, Button& button, const Colour& backgroundColour, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    // {
    //     g.setColour(Colours::black);
    //     g.drawRoundedRectangle(button.getLocalBounds().toFloat(), 10.f, 1.f);
    //     if (shouldDrawButtonAsDown){
    //         g.setColour(Colours::white);
    //         g.fillRoundedRectangle(button.getLocalBounds().toFloat(), 10.f);
    //     }
    // }

    // void drawButtonText(Graphics& g, TextButton& button, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    // {
    //     auto text = button.getButtonText();
    //     g.drawText(text, button.getLocalBounds(), Justification::centredRight, false);
    // }
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