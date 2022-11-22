// Button.h

#pragma once

struct ButtonLookAndFeel : LookAndFeel_V4
{
    ButtonLookAndFeel() = default;

    void drawButtonBackground(Graphics &g, Button &button, const Colour &backgroundColour, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
    {
        float cornerRadius = button.getHeight() * 0.25;
        g.setColour(Colours::white);
        g.drawRoundedRectangle(button.getLocalBounds().reduced(3).toFloat(), cornerRadius, 1.f);
        if (button.getToggleState())
        {
            g.setColour(Colours::white);
            g.fillRoundedRectangle(button.getLocalBounds().reduced(3).toFloat(), cornerRadius);
        }
    }

    void drawButtonText(Graphics &g, TextButton &button, bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override
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

struct PowerButton : TextButton
{
    PowerButton()
    {
        setClickingTogglesState(true);
    }

    void paint(Graphics &g) override
    {
        Colour icon, s_backgnd = background;
        auto b = getLocalBounds();
        auto padding = b.getHeight() * 0.1f;

        auto drawPowerIcon = [&](Graphics &gc)
        {
            gc.setColour(icon);
            gc.drawEllipse(b.reduced(padding * 2).toFloat(), 3.f);
            gc.fillRoundedRectangle(b.getCentreX() - 1.5f, padding, 3.f, b.getCentreY() - padding, 2.f);
        };

        if (isMouseOver())
        {
            s_backgnd = background.contrasting(0.2f);
            g.setColour(s_backgnd);
            g.fillEllipse(b.reduced(padding).toFloat());
        }

        if (!getToggleState())
            icon = s_backgnd.contrasting(0.2f);
        else
        {
            icon = Colours::white;
            Image blur(Image::PixelFormat::ARGB, getWidth(), getHeight(), true);
            Graphics g_blur(blur);

            drawPowerIcon(g_blur);
            gin::applyStackBlur(blur, 5);

            g.drawImage(blur, getLocalBounds().toFloat(), RectanglePlacement::centred);
        }

        drawPowerIcon(g);
    }

    void setBackgroundColor(Colour newColor)
    {
        background = newColor;
    }

private:
    Colour background;
};