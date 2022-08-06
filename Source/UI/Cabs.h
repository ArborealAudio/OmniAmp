// Cabs.h

#pragma once

class CabsComponent : public Component
{
    struct Cab : Component
    {
        Cab(const void *svgData, size_t svgSize)
        {
            svg = Drawable::createFromImageData(svgData, svgSize);
        }

        void setColor(Colour newColor)
        {
            color = newColor.withAlpha(0.5f);
            svg->replaceColour(Colours::transparentBlack, Colours::grey);
        }

        void paint(Graphics &g) override
        {
            g.setColour(Colours::grey);
            g.drawRect(getBounds());

            if (on)
                svg->replaceColour(Colours::grey, color);
            else
                svg->replaceColour(color, Colours::grey);

            svg->drawWithin(g, getBounds().reduced(15).toFloat(), RectanglePlacement::centred, 1.f);
        }

        bool isOn()
        {
            return on;
        }

        void setIsOn(bool isOn)
        {
            on = isOn;
        }

    private:
        std::unique_ptr<Drawable> svg;

        bool on = false;

        Colour color;
    };

    Cab small, med, large;

public:
    CabsComponent() : small(BinaryData::_2x12_svg, BinaryData::_2x12_svgSize),
                      med(BinaryData::_4x12_svg, BinaryData::_4x12_svgSize),
                      large(BinaryData::_6x10_svg, BinaryData::_6x10_svgSize)
    {
        small.setColor(Colours::olivedrab);
        med.setColor(Colours::cadetblue);
        large.setColor(Colours::chocolate);
    }

    void paint(Graphics &g) override
    {
        g.fillAll(Colours::oldlace);
        small.paint(g);
        med.paint(g);
        large.paint(g);
    }

    // 0 = none on
    void setState(int state)
    {
        switch (state)
        {
        case 0:
            small.setIsOn(false);
            med.setIsOn(false);
            large.setIsOn(false);
            break;
        case 1:
            small.setIsOn(true);
            med.setIsOn(false);
            large.setIsOn(false);
            break;
        case 2:
            small.setIsOn(false);
            med.setIsOn(true);
            large.setIsOn(false);
            break;
        case 3:
            small.setIsOn(false);
            med.setIsOn(false);
            large.setIsOn(true);
            break;
        };
    }

    bool hitTest(int x, int y) override
    {
        auto leftClick = ModifierKeys::currentModifiers.isLeftButtonDown();

        if (small.getBounds().contains(x, y) && leftClick)
        {
            if (small.isOn()) {
                small.setIsOn(false);
                cabChanged(false, 0);
            }
            else {
                small.setIsOn(true);
                med.setIsOn(false);
                large.setIsOn(false);
                if (cabChanged != nullptr)
                    cabChanged(true, 0);
            }
            repaint();
            return true;
        }
        else if (med.getBounds().contains(x, y) && leftClick)
        {
            if (med.isOn()) {
                med.setIsOn(false);
                cabChanged(false, 1);
            }
            else {
                med.setIsOn(true);
                small.setIsOn(false);
                large.setIsOn(false);
                if (cabChanged != nullptr)
                    cabChanged(true, 1);
            }
            repaint();
            return true;
        }
        else if (large.getBounds().contains(x, y) && leftClick)
        {
            if (large.isOn()) {
                large.setIsOn(false);
                cabChanged(false, 2);
            }
            else {
                large.setIsOn(true);
                small.setIsOn(false);
                med.setIsOn(false);
                if (cabChanged != nullptr)
                    cabChanged(true, 2);
            }
            repaint();
            return true;
        }
        else
            return false;
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        auto third = (float)bounds.getWidth() / 3.f;

        small.setBounds(bounds.removeFromLeft(third));
        med.setBounds(bounds.removeFromLeft(third));
        large.setBounds(bounds);
    }

    std::function<void(bool, int)> cabChanged;
};