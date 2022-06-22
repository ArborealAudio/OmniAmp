// Knob.h

#pragma once
#include "gin_gui/gin_gui.h"

struct KnobLookAndFeel : LookAndFeel_V4
{
    enum Type
    {
        Regular,
        LF,
        HF
    };

    Type type;

    KnobLookAndFeel(Type newType) : type(newType) {}
    ~KnobLookAndFeel(){}

    void drawRotarySlider(Graphics& g, int x, int y, int width, int height, float sliderPos,
    const float rotaryStartAngle, const float rotaryEndAngle, Slider& slider) override
    {
        switch (type)
        {
        case Regular: {
            auto radius = (float)jmin(width / 2, height / 2) - 4.f;
            auto centerX = (float)x + (float)width * 0.5f;
            auto centerY = (float)y + (float)height * 0.5f;
            auto rx = centerX - radius;
            auto ry = centerY - radius;
            auto rw = radius * 2.f;
            auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

            Image shadow{Image::PixelFormat::ARGB, width, height, true};
            Graphics sg(shadow);
            sg.setColour(Colours::black);
            sg.fillEllipse(rx, ry, rw, rw);

            // gin::applyStackBlur(shadow, 5);

            g.drawImageAt(shadow, x-3, y+3);

            g.setColour(Colours::antiquewhite);
            g.fillEllipse(rx, ry, rw, rw);
            g.setColour(Colours::grey);
            g.drawEllipse(rx, ry, rw, rw, 3.f);

            ColourGradient gradient{Colours::grey.withAlpha(0.5f), (float)x + width, (float)y, Colour(0x00000000), (float)x + height, (float)y + height, false};
            g.setGradientFill(gradient);
            g.fillEllipse(rx, ry, rw, rw);

            Path p;
            auto pointerLength = radius * 0.75f;
            auto pointerThickness = 2.5f;
            p.addRectangle(-pointerThickness * 0.5f, -radius * 0.8, pointerThickness, pointerLength);
            p.applyTransform(AffineTransform::rotation(angle).translated(centerX, centerY));

            g.setColour(Colours::grey);
            g.fillPath(p);
            break;
            }
        case LF: {
            auto radius = (float)jmin(width / 2, height / 2) - 4.f;
            auto centerX = (float)x + (float)width * 0.5f;
            auto centerY = (float)y + (float)height * 0.5f;
            auto rx = centerX - radius;
            auto ry = centerY - radius;
            auto rw = radius * 2.f;
            auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

            Image glow{Image::PixelFormat::ARGB, width, height, true};
            Graphics gg(glow);
            gg.setColour(Colours::wheat);
            auto fac = 1.f + (5.f * sliderPos);
            gg.drawEllipse(rx, ry, rw, rw, 1.f * fac);

            // gin::applyStackBlur(glow, 5);

            g.drawImageAt(glow, x, y);

            ColourGradient gradient{Colours::transparentBlack, centerX, centerY, Colours::black, (float)x, (float)y, true};
            g.setGradientFill(gradient);
            g.fillEllipse(rx, ry, rw, rw);

            Path p;
            auto ellipseWidth = radius * 0.25f;
            p.addEllipse(rx, -radius * 0.9, ellipseWidth, ellipseWidth);
            p.applyTransform(AffineTransform::rotation(angle).translated(centerX, centerY));
            g.setColour(Colour(0xff968875));
            g.fillPath(p);
            break;
            }
        case HF: {
            // width *= 0.8;
            // height *= 0.8;
            auto radius = (float)jmin(width / 2, height / 2) - 4.f;
            auto centerX = (float)x + (float)width * 0.5f;
            auto centerY = (float)y + (float)height * 0.5f;
            auto rx = centerX - radius;
            auto ry = centerY - radius;
            auto rw = radius * 2.f;
            auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

            Image glow{Image::PixelFormat::ARGB, width, height, true};
            Graphics gg(glow);
            gg.setColour(Colours::wheat);
            auto fac = 1.f + (5.f * sliderPos);
            gg.drawEllipse(rx, ry, rw, rw, 1.f * fac);

            // gin::applyStackBlur(glow, 5);

            g.drawImageAt(glow, x, y);

            ColourGradient gradient{Colours::transparentWhite, centerX, centerY, Colours::white, (float)x, (float)y, true};
            g.setGradientFill(gradient);
            g.fillEllipse(rx, ry, rw, rw);

            Path p;
            auto ellipseWidth = radius * 0.25f;
            p.addEllipse(rx, -radius * 0.9, ellipseWidth, ellipseWidth);
            p.applyTransform(AffineTransform::rotation(angle).translated(centerX, centerY));
            g.setColour(Colour(0xff968875));
            g.fillPath(p);
            break;
            }
        }
    }
};

struct Knob : Slider
{
    enum Type
    {
        Regular,
        LF,
        HF
    };

    Knob(Type t) : lnf((KnobLookAndFeel::Type)t)
    {
        setLookAndFeel(&lnf);
        setSliderStyle(Slider::SliderStyle::RotaryVerticalDrag);
        setPopupDisplayEnabled(true, true, this);
        setTextBoxStyle(Slider::NoTextBox, true, 0, 0);
        setPaintingIsUnclipped(true);
    }
    ~Knob()
    {
        setLookAndFeel(nullptr);
    }

private:
    KnobLookAndFeel lnf;
};