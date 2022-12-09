// Knob.h

#pragma once
#include "gin_gui/gin_gui.h"

struct Knob : Slider
{
    enum
    {
        DRAW_GRADIENT = 1,
        DRAW_SHADOW = 1 << 2,
        DRAW_ARC = 1 << 3,
        DRAW_TICKS = 1 << 4
    };
    typedef uint8_t flags_t;

    Knob(flags_t f) : lnf(f)
    {
        setLookAndFeel(&lnf);
        lnf.autoGain = &autoGain;
        setSliderStyle(Slider::SliderStyle::RotaryVerticalDrag);
        setTextBoxStyle(Slider::NoTextBox, true, 0, 0);

        setBufferedToImage(true);
    }
    ~Knob() override
    {
        setLookAndFeel(nullptr);
    }

    /**
     * Use this to set the default parameter value which the knob can report to its underlings
     */
    void setDefaultValue(float newDefault)
    {
        defaultValue = newDefault;
    }

    float getDefaultValue() { return defaultValue; }

    void mouseDrag(const MouseEvent &e) override
    {
        e.source.enableUnboundedMouseMovement(true);
        Slider::mouseDrag(e);
    }

    void setLabel(String newLabel)
    {
        label = newLabel;
        lnf.label = std::make_unique<String>(label);
    }

    void mouseDown(const MouseEvent &event) override
    {
        auto alt = event.mods.isAltDown();
        auto leftClick = event.mods.isLeftButtonDown();

        if (alt && leftClick)
        {
            if (onAltClick)
            {
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
    void setColor(Colour newBaseColor, Colour newAccentColor = Colours::grey, Colour newTextColor = Colours::antiquewhite)
    {
        lnf.baseColor = newBaseColor;
        lnf.accentColor = newAccentColor;
        lnf.textColor = newTextColor;
    }

    void setOffset(int xOffset, int yOffset)
    {
        lnf.xOffset = xOffset;
        lnf.yOffset = yOffset;
    }

    std::atomic<bool> autoGain = false;

private:
    struct KnobLookAndFeel : LookAndFeel_V4
    {
        Colour baseColor = Colours::antiquewhite, accentColor = Colours::grey, textColor = Colours::black;
        flags_t flags;

        int xOffset = 0, yOffset = 0;

        std::unique_ptr<String> label = nullptr;

        std::function<String(float)> valueToString = nullptr;

        std::atomic<bool> *autoGain = nullptr;

        KnobLookAndFeel(flags_t f) : flags(f)
        {
            label = std::make_unique<String>("");
        }

        void drawRotarySlider(Graphics &g, int x, int y, int width, int height, float sliderPos,
                              const float rotaryStartAngle, const float rotaryEndAngle, Slider &slider) override
        {
            g.setFont(14.f);

            auto drawCustomSlider = [&]
            {
                auto radius = (float)jmin(width / 2, height / 2) - 4.f;
                auto centerX = (float)slider.getLocalBounds().getCentreX();
                auto centerY = (float)slider.getLocalBounds().getCentreY() + yOffset;
                auto rx = centerX - radius;
                auto ry = centerY - radius;
                auto rw = radius * 2.f;
                auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

                if ((flags & DRAW_TICKS) > 0)
                {
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
                }
                if ((flags & DRAW_SHADOW) > 0)
                {
                    /* shadow & highlight */
                    Image shadow{Image::PixelFormat::ARGB, slider.getWidth(), slider.getHeight(), true};
                    Image highlight{Image::PixelFormat::ARGB, slider.getWidth(), slider.getHeight(), true};
                    Graphics sg(shadow);
                    Graphics hg(highlight);
                    sg.setColour(Colours::black.withAlpha(0.5f));
                    hg.setColour(Colours::white.withAlpha(0.33f));
                    sg.fillEllipse(rx, ry, rw, rw);
                    hg.fillEllipse(rx, ry, rw, rw);

                    gin::applyStackBlur(shadow, 12);
                    gin::applyStackBlur(highlight, 12);

                    g.drawImageAt(shadow, x - 3, y + 3);
                    g.drawImageAt(highlight, x + 3, y - 3);
                }

                if ((flags & DRAW_GRADIENT) == 0)
                {
                    g.setColour(slider.isMouseOverOrDragging() ? accentColor : baseColor);
                    g.drawEllipse(rx, ry, rw, rw, 3.f);

                    Path p;
                    auto pointerLength = radius * 0.8f;
                    auto pointerThickness = 2.5f;
                    p.addRectangle(-pointerThickness * 0.5f, -radius, pointerThickness, pointerLength);
                    p.applyTransform(AffineTransform::rotation(angle).translated(centerX, centerY));

                    g.fillPath(p);
                }
                else
                {
                    /* base knob */
                    g.setColour(baseColor);
                    g.fillEllipse(rx, ry, rw, rw);
                    g.setColour(accentColor);
                    g.drawEllipse(rx, ry, rw, rw, 3.f);

                    /* gradient overlay */
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
                }

                if ((flags & DRAW_ARC) > 0)
                {
                    auto defaultVal = static_cast<Knob *>(&slider)->getDefaultValue();
                    /* arc */
                    float arcStart = defaultVal == 0.f ? rotaryStartAngle : 0.f;
                    float arcAngle = defaultVal == 0.f ? angle : angle - 2.f * MathConstants<float>::pi;
                    g.setColour(accentColor.contrasting(0.3f).withMultipliedSaturation(1.2f));
                    Path arc;
                    arc.addArc(rx - 4, ry - 4, rw + 8, rw + 8, arcStart, arcAngle, true);
                    g.strokePath(arc, PathStrokeType(4.f, PathStrokeType::beveled, PathStrokeType::rounded));
                }
            };

            width *= 0.75;
            height *= 0.75;

            drawCustomSlider();

            /* label */
            g.setColour(accentColor);
            String text = "";
            if (slider.isMouseOverOrDragging() && valueToString)
                text = valueToString(slider.getValue());
            else if (label)
                text = *label;
            g.drawFittedText(text, slider.getLocalBounds().removeFromBottom(slider.getHeight() / 6), Justification::centred, 2);
        }
    };
    KnobLookAndFeel lnf;
    String label;
    float defaultValue = 0.f;
};
