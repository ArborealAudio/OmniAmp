// Knob.h

#pragma once
#include "gin_gui/gin_gui.h"

enum KnobType
{
    Regular,
    LF,
    HF,
    Simple
};

struct KnobLookAndFeel : LookAndFeel_V4
{
    KnobType type;

    std::unique_ptr<String> label;

    KnobLookAndFeel(KnobType newType) : type(newType)
    {
        label = std::make_unique<String>("");
        setDefaultSansSerifTypeface(getCustomFont().getTypeface());
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

            Image shadow{Image::PixelFormat::ARGB, slider.getWidth(), slider.getHeight(), true};
            Graphics sg(shadow);
            sg.setColour(Colours::black);
            sg.fillEllipse(rx, ry, rw, rw);

            gin::applyStackBlur(shadow, 5);

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

            g.setColour(Colours::black);
            if (slider.isMouseOverOrDragging())
                g.drawText(String(slider.getValue()), slider.getLocalBounds().removeFromBottom(height * 0.2), Justification::centred, false);
            else
                g.drawText(*label, slider.getLocalBounds().removeFromBottom(height * 0.2), Justification::centred, false);
            break;
            }
        case LF: {
            width *= 0.8;
            height *= 0.8;

            auto radius = (float)jmin(width / 2, height / 2) - 4.f;
            auto centerX = (float)slider.getLocalBounds().getCentreX();
            auto centerY = (float)slider.getLocalBounds().getCentreY();
            auto rx = centerX - radius;
            auto ry = centerY - radius;
            auto rw = radius * 2.f;
            auto angle = rotaryStartAngle * 0.8 + sliderPos * (rotaryEndAngle - rotaryStartAngle);

            Image glow{Image::PixelFormat::ARGB, slider.getWidth(), slider.getHeight(), true};
            Graphics gg(glow);
            gg.setColour(Colours::whitesmoke);
            auto fac = 1.f + (15.f * sliderPos);
            gg.drawEllipse(rx, ry, rw, rw, 1.f * fac);

            gin::applyStackBlur(glow, 5);

            g.setColour(Colour(BACKGROUND_COLOR));
            g.fillEllipse(rx, ry, rw, rw);

            g.drawImageAt(glow, 0, 0);

            ColourGradient gradient{Colours::transparentBlack, centerX, centerY, Colours::black, (float)x, (float)y, true};
            g.setGradientFill(gradient);
            g.fillEllipse(rx, ry, rw, rw);

            Path p;
            auto ellipseWidth = radius * 0.25f;
            p.addEllipse(rx, -radius * 0.9, ellipseWidth, ellipseWidth);
            p.applyTransform(AffineTransform::rotation(angle).translated(centerX, centerY));
            g.setColour(Colour(BACKGROUND_COLOR));
            g.fillPath(p);
            break;
            }
        case HF: {
            width *= 0.8;
            height *= 0.8;

            auto radius = (float)jmin(width * 0.5f, height * 0.5f) - 4.f;
            auto centerX = (float)slider.getLocalBounds().getCentreX();
            auto centerY = (float)slider.getLocalBounds().getCentreY();
            auto rx = centerX - radius;
            auto ry = centerY - radius;
            auto rw = radius * 2.f;
            auto angle = rotaryStartAngle * 0.8 + sliderPos * (rotaryEndAngle - rotaryStartAngle);

            Image glow{Image::PixelFormat::ARGB, slider.getWidth(), slider.getHeight(), true};
            
            Graphics gg(glow);
            gg.setColour(Colours::wheat);
            auto fac = 1.f + (15.f * sliderPos);
            gg.drawEllipse(rx, ry, rw, rw, 1.f * fac);

            gin::applyStackBlur(glow, 5);

            g.setColour(Colour(BACKGROUND_COLOR));
            g.fillEllipse(rx, ry, rw, rw);

            g.drawImageAt(glow, 0, 0);

            ColourGradient gradient{Colours::transparentWhite, centerX, centerY, Colours::white, (float)x, (float)y, true};
            g.setGradientFill(gradient);
            g.fillEllipse(rx, ry, rw, rw);

            Path p;
            auto ellipseWidth = radius * 0.25f;
            p.addEllipse(rx, -radius * 0.9, ellipseWidth, ellipseWidth);
            p.applyTransform(AffineTransform::rotation(angle).translated(centerX, centerY));
            g.setColour(Colour(BACKGROUND_COLOR));
            g.fillPath(p);
            break;
            }
        case Simple: {
            width *= 0.75;
            height *= 0.75;

            auto radius = (float)jmin(width / 2, height / 2) - 4.f;
            auto centerX = (float)slider.getLocalBounds().getCentreX();
            auto centerY = (float)slider.getLocalBounds().getCentreY();
            auto rx = centerX - radius;
            auto ry = centerY - radius;
            auto rw = radius * 2.f;
            auto angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

            g.setColour(Colours::antiquewhite);
            g.drawEllipse(rx, ry, rw, rw, 3.f);

            Path p;
            auto pointerLength = radius * 0.8f;
            auto pointerThickness = 2.5f;
            p.addRectangle(-pointerThickness * 0.5f, -radius, pointerThickness, pointerLength);
            p.applyTransform(AffineTransform::rotation(angle).translated(centerX, centerY));

            g.setColour(Colours::antiquewhite);
            g.fillPath(p);

            if (slider.isMouseOverOrDragging())
                g.drawText(String(slider.getValue()), slider.getLocalBounds().removeFromBottom(height * 0.2), Justification::centred, false);
            else
                g.drawText(*label, slider.getLocalBounds().removeFromBottom(height * 0.2), Justification::centred, false);
            break;
            }
        }
    }
};

struct Knob : Slider
{
    Knob(KnobType t) : lnf(t)
    {
        setLookAndFeel(&lnf);
        setSliderStyle(Slider::SliderStyle::RotaryVerticalDrag);
        setTextBoxStyle(Slider::NoTextBox, true, 0, 0);
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
            if (onAltClick != nullptr) {
                autogain.store(autogain.load() ? false : true);
                onAltClick(autogain.load());
            }
        }
        else
            Slider::mouseDown(event);
    }

    String getLabel() { return label; }

    std::function<void(bool)> onAltClick;
    std::atomic<bool> autogain = false;

private:
    KnobLookAndFeel lnf;
    String label;
};