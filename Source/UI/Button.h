// Button.h

#pragma once
#include <JuceHeader.h>

struct ButtonLookAndFeel : LookAndFeel_V4
{
    ButtonLookAndFeel() = default;

    void drawButtonBackground(Graphics &g, Button &button, const Colour &, bool, bool) override
    {
        g.setColour(Colours::white);
        auto bounds = button.getLocalBounds().reduced(3).toFloat();
        g.drawRoundedRectangle(bounds, cornerRadius, 1.f);

        auto state = button.getToggleState() && shouldFillOnToggle;
        auto currentColor = state ? Colours::white : Colours::transparentBlack;

        if (button.isMouseOver())
        {
            g.setColour(currentColor.contrasting(0.2f));
            g.fillRoundedRectangle(bounds, cornerRadius);
        }
        else
        {
            g.setColour(currentColor);
            g.fillRoundedRectangle(bounds, cornerRadius);
        }
    }

    void drawButtonText(Graphics &g, TextButton &button, bool, bool) override
    {
        auto text = button.getButtonText();
        auto state = button.getToggleState() && shouldFillOnToggle;
        auto currentColor = state ? Colours::white : Colours::transparentBlack;
        g.setColour(currentColor.contrasting(0.8f));
        g.drawText(text, button.getLocalBounds(), Justification::centred, false);
    }

    float cornerRadius = 0.f;
    bool shouldFillOnToggle = true; /*disable this to avoid filling the button with white when it is on,
    if the button is intended to be a two-way switch rather than an on-off toggle*/
};

struct LightButton : TextButton
{
    ButtonLookAndFeel lnf;

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
        b.setWidth(jmin(b.getWidth(), b.getHeight()));
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

/**
 * A better version of DrawableButton for our gain link button
*/
struct LinkButton : DrawableButton
{
    LinkButton(AudioProcessorValueTreeState &a) : apvts(a), DrawableButton("Gain Link", DrawableButton::ButtonStyle::ImageFitted)
    {
        attach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(apvts, "gainLink", *this);
        on = Drawable::createFromImageData(BinaryData::link_svg, BinaryData::link_svgSize);
        overOn = Drawable::createFromImageData(BinaryData::link_over_on_svg, BinaryData::link_over_on_svgSize);
        overOff = Drawable::createFromImageData(BinaryData::link_over_off_svg, BinaryData::link_over_off_svgSize);
        off = Drawable::createFromImageData(BinaryData::link_off_svg, BinaryData::link_off_svgSize);
        setClickingTogglesState(true);
        setTooltip("Link Input and Output gains. Output gain can still be adjusted with linking on.");
    }

    void paint(Graphics &g) override
    {
        if (isMouseOver())
        {
            if (getToggleState())
                overOn->drawWithin(g, getLocalBounds().toFloat(), RectanglePlacement::centred, 1.f);
            else
                overOff->drawWithin(g, getLocalBounds().toFloat(), RectanglePlacement::centred, 1.f);
        }
        else if (getToggleState())
        {
            on->drawWithin(g, getLocalBounds().toFloat(), RectanglePlacement::centred, 1.f);
        }
        else
        {
            off->drawWithin(g, getLocalBounds().toFloat(), RectanglePlacement::centred, 1.f);
        }
    }

private:
    AudioProcessorValueTreeState &apvts;
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> attach;
    std::unique_ptr<Drawable> on, overOn, overOff, off;
};

/*
    Button for minimizing dynamically-sized components
*/
struct ResizeButton : TextButton
{
    ResizeButton()
    {
        setClickingTogglesState(true);
    }

    void paint (Graphics &g) override
    {
        auto bounds = getLocalBounds().reduced(6);
        float x = bounds.getX();
        float y = bounds.getY();
        float r = bounds.getRight();
        float bot = bounds.getBottom();
        float midX = bounds.getCentreX();
        float midY = bounds.getCentreY();

        bool state = getToggleState();
        if (isMouseOver())
        {
            g.setColour(Colours::grey);
            g.fillEllipse(getLocalBounds().toFloat());
            g.setColour(Colours::antiquewhite);
        }
        else
            g.setColour(Colours::grey);
        g.drawEllipse(getLocalBounds().reduced(1).toFloat(), 1.f);
        Path p;
        if (state)
        {
            p.startNewSubPath(x, y);
            p.lineTo(midX , bot);
            p.lineTo(r, y);
        }
        else
        {
            p.startNewSubPath(x, y);
            p.lineTo(r, midY);
            p.lineTo(x, bot);
        }
        g.strokePath(p, PathStrokeType(2.f, PathStrokeType::JointStyle::curved, PathStrokeType::EndCapStyle::rounded));
    }
};