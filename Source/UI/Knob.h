// Knob.h

#pragma once

struct SimpleSlider : Slider
{
    SimpleSlider() : lnf(*this)
    {
        setLookAndFeel(&lnf);
        setSliderStyle(Slider::SliderStyle::LinearHorizontal);
        setTextBoxStyle(Slider::NoTextBox, true, 0, 0);
        setSliderSnapsToMousePosition(false);
        setBufferedToImage(true);
    }
    ~SimpleSlider() override { setLookAndFeel(nullptr); }

    double proportionOfLengthToValue(double proportion) override
    {
        return mapToLog10(
            proportion, minValue,
            maxValue * 1.05); // 1.05 = fudge factor to get slider to fill up
    }

    double valueToProportionOfLength(double value) override
    {
        return mapFromLog10(value, minValue, maxValue * 1.05);
    }

    Colour outlineColor, baseColor;
    float cornerRadius = 0.f;
    double minValue = 0.0, maxValue = 0.0;

    struct LNF : LookAndFeel_V4
    {
        LNF(SimpleSlider &s) : owner(s) {}

        void drawLinearSlider(Graphics &g, int x, int y, int width, int height,
                              float sliderPos, float minSliderPos,
                              float maxSliderPos,
                              const Slider::SliderStyle style,
                              Slider &slider) override
        {
            auto bounds =
                slider.getLocalBounds().reduced(width * 0.07f, 5).toFloat();
            g.setColour(owner.outlineColor);
            g.drawRoundedRectangle(bounds, owner.cornerRadius, 2.f);
            auto fillBounds = bounds.reduced(1);
            Path outline;
            outline.addRoundedRectangle(bounds.reduced(2), owner.cornerRadius);
            g.reduceClipRegion(outline);
            g.setColour(owner.baseColor);
            g.fillRect(fillBounds.withRight(fillBounds.getX() + sliderPos));

            g.setColour(owner.outlineColor);
            String valStr;
            auto val = slider.getValue();
            if (val > 1000.0) {
                valStr = String(val / 1000, 2);
                valStr.append(" kHz", 4);
            } else {
                valStr = String((int)val);
                valStr.append(" Hz", 3);
            }

            g.drawFittedText(
                valStr,
                bounds.reduced(width * 0.33f, height * 0.1f).toNearestInt(),
                Justification::centred, 1);
        }

        SimpleSlider &owner;
    };

    LNF lnf;
};

struct Knob : Slider
{
    enum
    {
        DRAW_GRADIENT = 1,
        DRAW_SHADOW = 1 << 1,
        DRAW_ARC = 1 << 2,
        DRAW_TICKS = 1 << 3,
        LOG_KNOB = 1 << 4 // used for setting log mapping i.e. for freq. params
    };
    typedef uint8_t Flags;
    Flags flags;

    Knob(Flags f) : lnf(f), flags(f)
    {
        setLookAndFeel(&lnf);
        setSliderStyle(Slider::SliderStyle::RotaryVerticalDrag);
        setTextBoxStyle(Slider::NoTextBox, true, 0, 0);

        setBufferedToImage(true);
    }
    ~Knob() override { setLookAndFeel(nullptr); }

    /**
     * Use this to set the default parameter value which the knob can report to
     * its underlings
     */
    void setDefaultValue(float newDefault) { defaultValue = newDefault; }

    float getDefaultValue() { return defaultValue; }

    // void mouseDrag(const MouseEvent &e) override
    // {
    //     e.source.enableUnboundedMouseMovement(true, true);
    //     Slider::mouseDrag(e);
    // }

    double proportionOfLengthToValue(double proportion) override
    {
        if (flags & LOG_KNOB)
            return mapToLog10(proportion, getMinimum(), getMaximum());
        return Slider::proportionOfLengthToValue(proportion);
    }

    double valueToProportionOfLength(double value) override
    {
        if (flags & LOG_KNOB)
            return mapFromLog10(value, getMinimum(), getMaximum());
        return Slider::valueToProportionOfLength(value);
    }

    void setLabel(String newLabel)
    {
        label = newLabel;
        lnf.label = std::make_unique<String>(label);
    }

    String getLabel() { return label; }

    void setValueToStringFunction(std::function<String(float)> func)
    {
        lnf.valueToString = func;
    }

    /* sets the base & accent colors for Regular-style knobs */
    void setColor(Colour newBaseColor, Colour newAccentColor = Colours::grey,
                  Colour newTextColor = Colours::antiquewhite)
    {
        lnf.baseColor = newBaseColor;
        lnf.accentColor = newAccentColor;
        lnf.textColor = newTextColor;
    }

    // positional offset for knob within its bounds
    void setOffset(int xOffset, int yOffset)
    {
        lnf.xOffset = xOffset;
        lnf.yOffset = yOffset;
    }

    void setTextOffset(int xOffset, int yOffset)
    {
        lnf.textXOffset = xOffset;
        lnf.textYOffset = yOffset;
    }

  private:
    struct KnobLookAndFeel : LookAndFeel_V4
    {
        Colour baseColor = Colours::antiquewhite, accentColor = Colours::grey,
               textColor = Colours::antiquewhite;
        Flags flags;

        int xOffset = 0, yOffset = 0;
        int textXOffset = 0, textYOffset = 0;

        std::unique_ptr<String> label = nullptr;

        std::function<String(float)> valueToString = nullptr;

        KnobLookAndFeel(Flags f) : flags(f)
        {
            label = std::make_unique<String>("");
        }

        void drawRotarySlider(Graphics &g, int x, int y, int width, int height,
                              float sliderPos, const float rotaryStartAngle,
                              const float rotaryEndAngle,
                              Slider &slider) override
        {
            auto drawCustomSlider = [&] {
                auto radius = (float)jmin(width / 2, height / 2) - 4.f;
                auto centerX = (float)slider.getLocalBounds().getCentreX();
                auto centerY =
                    (float)slider.getLocalBounds().getCentreY() + yOffset;
                auto rx = centerX - radius;
                auto ry = centerY - radius;
                auto rw = radius * 2.f;
                auto angle = rotaryStartAngle +
                             sliderPos * (rotaryEndAngle - rotaryStartAngle);

                if ((flags & DRAW_TICKS) > 0) {
                    /* draw ticks around knob */
                    for (int i = 0; i < 11; ++i) {
                        Path p;
                        const auto tickRadius =
                            (float)jmin(slider.getWidth() / 2,
                                        slider.getHeight() / 1) -
                            4.f;
                        const auto pointerLength = tickRadius * 0.25f;
                        const auto pointerThickness = 2.5f;
                        const auto tickAngle =
                            rotaryStartAngle +
                            (i / 10.f) * (rotaryEndAngle - rotaryStartAngle);
                        p.addRectangle(-pointerThickness * 0.5f, -tickRadius,
                                       pointerThickness, pointerLength);
                        p.applyTransform(
                            AffineTransform::rotation(tickAngle).translated(
                                centerX, centerY));

                        g.setColour(accentColor);
                        g.fillPath(p);
                    }
                }
                if ((flags & DRAW_SHADOW) > 0) {
                    /* shadow & highlight */
                    Image shadow{Image::PixelFormat::ARGB, slider.getWidth(),
                                 slider.getHeight(), true};
                    Image highlight{Image::PixelFormat::ARGB, slider.getWidth(),
                                    slider.getHeight(), true};
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

                if ((flags & DRAW_GRADIENT) == 0) // if no gradient
                {
                    g.setColour(slider.isMouseOverOrDragging() ? accentColor
                                                               : baseColor);
                    g.drawEllipse(rx, ry, rw, rw, 3.f);

                    Path p;
                    auto pointerLength = radius * 0.8f;
                    auto pointerThickness = 2.5f;
                    p.addRectangle(-pointerThickness * 0.5f, -radius,
                                   pointerThickness, pointerLength);
                    p.applyTransform(
                        AffineTransform::rotation(angle).translated(centerX,
                                                                    centerY));

                    g.fillPath(p);
                } else // should draw gradient
                {
                    /* base knob */
                    g.setColour(baseColor);
                    g.fillEllipse(rx, ry, rw, rw);
                    g.setColour(accentColor);
                    g.drawEllipse(rx, ry, rw, rw, 3.f);

                    /* gradient overlay */
                    ColourGradient gradient{Colours::darkgrey.withAlpha(0.5f),
                                            (float)x,
                                            (float)y + height,
                                            Colour(0xe8e8e8).withAlpha(0.5f),
                                            (float)x + width,
                                            (float)y,
                                            false};
                    g.setGradientFill(gradient);
                    g.fillEllipse(rx, ry, rw, rw);

                    /* draw knob pointer */
                    Path p;
                    auto pointerLength = radius * 0.75f;
                    auto pointerThickness = 2.5f;
                    p.addRectangle(-pointerThickness * 0.5f, -radius * 0.8,
                                   pointerThickness, pointerLength);
                    p.applyTransform(
                        AffineTransform::rotation(angle).translated(centerX,
                                                                    centerY));

                    g.setColour(accentColor);
                    g.fillPath(p);
                }

                /* arc */
                if ((flags & DRAW_ARC) > 0) {
                    auto defaultVal =
                        static_cast<Knob *>(&slider)->getDefaultValue();
                    float arcStart =
                        defaultVal * (rotaryEndAngle - rotaryStartAngle) +
                        rotaryStartAngle;
                    g.setColour(
                        accentColor.contrasting(0.3f).withMultipliedSaturation(
                            1.2f));
                    Path arc;
                    arc.addArc(rx - 4, ry - 4, rw + 8, rw + 8, arcStart, angle,
                               true);
                    g.strokePath(arc,
                                 PathStrokeType(4.f, PathStrokeType::beveled,
                                                PathStrokeType::rounded));
                }
            };

            width *= 0.75;
            height *= 0.75;

            drawCustomSlider();

            /* label */
            g.setColour(textColor);
            String text = "";
            if (slider.isMouseOverOrDragging() && valueToString)
                text = valueToString(slider.getValue());
            else if (label)
                text = *label;

            float textBoxHeight = slider.getHeight() / 6.f;
            g.setFont(jlimit(14.f, 17.f, textBoxHeight));
            g.drawFittedText(text,
                             slider.getLocalBounds()
                                 .removeFromBottom(textBoxHeight)
                                 .translated(textXOffset, textYOffset),
                             Justification::centred, 2);
        }
    };
    KnobLookAndFeel lnf;
    String label;
    float defaultValue = 0.f;
};
