// Knob.h

#pragma once
#include "gin_gui/gin_gui.h"

enum KnobType
{
    Regular,
    Enhancer,
    Simple
};

struct KnobLookAndFeel : LookAndFeel_V4
{
    KnobType type;

    Colour baseColor = Colours::antiquewhite;
    Colour accentColor = Colours::grey;

    std::unique_ptr<String> label;

    std::function<String(float)> valueToString = nullptr;

    std::atomic<bool>* autoGain;

    KnobLookAndFeel(KnobType newType) : type(newType)
    {
        label = std::make_unique<String>("");
        getDefaultLookAndFeel().setDefaultSansSerifTypeface(getCustomFont());
    }

    void drawRotarySlider(Graphics& g, int x, int y, int width, int height, float sliderPos,
    const float rotaryStartAngle, const float rotaryEndAngle, Slider& slider) override
    {
        switch (type)
        {
        case Regular: {
            width *= 0.75;
            height *= 0.75;

            auto radius = (float)jmin(width / 2, height / 2) - 4.f;
            auto centerX = (float)slider.getLocalBounds().getCentreX();
            auto centerY = (float)slider.getLocalBounds().getCentreY();
            auto rx = centerX - radius;
            auto ry = centerY - radius;
            auto rw = radius * 2.f;
            auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

            /* draw ticks around knob */
            for (int i = 0; i < 11; ++i)
            {
                Path p;
                const auto tickRadius = (float)jmin(slider.getWidth() / 2, slider.getHeight() / 1) - 4.f;
                const auto pointerLength = tickRadius * 0.25f;
                const auto pointerThickness = 2.5f;
                const auto tickAngle = rotaryStartAngle + (i / 10.f) * (rotaryEndAngle - rotaryStartAngle);
                p.addRectangle(-pointerThickness * 0.5f, -tickRadius, pointerThickness, pointerLength);
                p.applyTransform(AffineTransform::rotation(tickAngle).translated(centerX, centerY));

                g.setColour(accentColor);
                g.fillPath(p);
            }

            /* shadow & highlight */
            Image shadow{Image::PixelFormat::ARGB, slider.getWidth(), slider.getHeight(), true};
            Image highlight{Image::PixelFormat::ARGB, slider.getWidth(), slider.getHeight(), true};
            Graphics sg(shadow);
            Graphics hg(highlight);
            sg.setColour(Colours::black.withAlpha(0.5f));
            hg.setColour(Colours::white.withAlpha(0.5f));
            sg.fillEllipse(rx, ry, rw, rw);
            hg.fillEllipse(rx, ry, rw, rw);

            gin::applyStackBlur(shadow, 12);
            gin::applyStackBlur(highlight, 12);

            g.drawImageAt(shadow, x - 3, y + 3);
            g.drawImageAt(highlight, x + 3, y - 3);

            /* base knob */
            g.setColour(baseColor);
            g.fillEllipse(rx, ry, rw, rw);
            g.setColour(accentColor);
            g.drawEllipse(rx, ry, rw, rw, 3.f);

            ColourGradient gradient{Colours::darkgrey.withAlpha(0.5f), (float)x, (float)y + height, Colour(0xe8e8e8).withAlpha(0.5f), (float)x + width, (float)y, false};
            g.setGradientFill(gradient);
            g.fillEllipse(rx, ry, rw, rw);

            /* draw knob pointer */
            Path p;
            auto pointerLength = radius * 0.75f;
            auto pointerThickness = 2.5f;
            p.addRectangle(-pointerThickness * 0.5f, -radius * 0.8, pointerThickness, pointerLength);
            p.applyTransform(AffineTransform::rotation(angle).translated(centerX, centerY));

            g.setColour(accentColor);
            g.fillPath(p);

            /* text */
            String text;
            if(slider.isMouseOverOrDragging() && valueToString)
                text = valueToString(slider.getValue());
            else if (label)
                text = *label;

            g.setColour(accentColor == Colours::oldlace ? accentColor : Colours::black);
            g.drawText(text, slider.getLocalBounds().removeFromBottom(height * 0.2), Justification::centred, false);
            break;
            }
        case Enhancer: {
            width *= 0.8;
            height *= 0.8;

            auto radius = (float)jmin(width / 2, height / 2) - 4.f;
            auto centerX = (float)slider.getLocalBounds().getCentreX();
            auto centerY = (float)slider.getLocalBounds().getCentreY();
            auto rx = centerX - radius;
            auto ry = centerY - radius;
            auto rw = radius * 2.f;
            auto angle = rotaryStartAngle * 0.8 + sliderPos * (rotaryEndAngle - rotaryStartAngle);

            /* base background */
            g.setColour(Colour(BACKGROUND_COLOR));
            g.fillEllipse(rx, ry, rw, rw);

            /* glow ring */
            Image glow{Image::PixelFormat::ARGB, slider.getWidth(), slider.getHeight(), true};
            Graphics gg(glow);
            gg.setColour(accentColor);
            auto fac = 1.f + (15.f * sliderPos);
            gg.drawEllipse(rx, ry, rw, rw, 1.f * fac);
            gin::applyStackBlur(glow, 5);

            g.drawImageAt(glow, 0, 0);

            /* gradient overlay */
            ColourGradient gradient{baseColor.withAlpha(0.f), centerX, centerY, baseColor, (float)x, (float)y, true};
            g.setGradientFill(gradient);
            g.fillEllipse(rx, ry, rw, rw);

            /* knob thumb */
            Path p;
            auto ellipseWidth = radius * 0.25f;
            p.addEllipse(rx + 2.f, -radius * 0.9, ellipseWidth, ellipseWidth);
            p.applyTransform(AffineTransform::rotation(angle).translated(centerX, centerY));
            g.setColour(Colour(BACKGROUND_COLOR));
            g.fillPath(p);

            /* autogain label */
            if (autoGain->load()) {
                g.setColour(Colour(BACKGROUND_COLOR));
                auto bounds = Rectangle<float>(centerX - rw / 4, centerY - 10, rw * 0.5f, 20);
                g.fillRoundedRectangle(bounds, 5.f);
                g.setColour(Colours::white);
                g.drawFittedText("Auto", bounds.toNearestInt(), Justification::centred, 1);
            }
            break;
            }
        case Simple: {
            width *= 0.75;
            height *= 0.75;

            auto radius = (float)jmin(width / 2, height / 2) - 4.f;
            auto centerX = (float)slider.getLocalBounds().getCentreX();
            auto centerY = (float)slider.getLocalBounds().getCentreY() - 5;
            auto rx = centerX - radius;
            auto ry = centerY - radius;
            auto rw = radius * 2.f;
            auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

            g.setColour(slider.isMouseOverOrDragging() ? Colours::goldenrod.withSaturation(0.45f) : Colours::antiquewhite);
            g.drawEllipse(rx, ry, rw, rw, 3.f);

            Path p;
            auto pointerLength = radius * 0.8f;
            auto pointerThickness = 2.5f;
            p.addRectangle(-pointerThickness * 0.5f, -radius, pointerThickness, pointerLength);
            p.applyTransform(AffineTransform::rotation(angle).translated(centerX, centerY));

            g.fillPath(p);

            if (slider.isMouseOverOrDragging()) {
                g.drawFittedText(valueToString(slider.getValue()), slider.getLocalBounds().removeFromBottom(height * 0.3), Justification::centred, 1);
            }
            else
                g.drawFittedText(*label, slider.getLocalBounds().removeFromBottom(height * 0.3), Justification::centred, 1);
            break;
            }
        }
    }
};

struct Knob : Slider
{
    Knob(KnobType t) : lnf(t), autoGain(false)
    {
        setLookAndFeel(&lnf);
        lnf.autoGain = &autoGain;
        setSliderStyle(Slider::SliderStyle::RotaryVerticalDrag);
        setTextBoxStyle(Slider::NoTextBox, true, 0, 0);

        setPaintingIsUnclipped(true);
        setBufferedToImage(true);
    }
    ~Knob()
    {
        setLookAndFeel(nullptr);
    }

    void setLabel(String newLabel)
    {
        label = newLabel;
        lnf.label = std::make_unique<String>(label);
    }

    void mouseDown(const MouseEvent& event) override
    {
        auto alt = event.mods.isAltDown();
        auto leftClick = event.mods.isLeftButtonDown();

        if (alt && leftClick)
        {
            if (onAltClick) {
                autoGain.store(!autoGain.load());
                onAltClick(autoGain.load());
            }
        }
        else
            Slider::mouseDown(event);
    }

    String getLabel() { return label; }

    std::function<void(bool)> onAltClick;

    void setValueToStringFunction(std::function<String(float)> func)
    {
        lnf.valueToString = func;
    }

    /* sets the base & accent colors for Regular-style knobs */
    void setColor(Colour newBaseColor, Colour newAccentColor = Colours::grey)
    {
        lnf.baseColor = newBaseColor;
        lnf.accentColor = newAccentColor;
    }

    std::atomic<bool> autoGain;

private:
    KnobLookAndFeel lnf;
    String label;
};
