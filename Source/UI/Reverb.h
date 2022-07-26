// Reverb.h

#pragma once

struct ReverbButton : TextButton
{
    ReverbButton() = default;

    void paint(Graphics& g) override
    {
        auto bounds = getLocalBounds().reduced(15, 15);
        float centerY = bounds.getCentreY();
        float centerX = bounds.getCentreX();
        auto p1 = Point<float>{centerX - 20.f, centerY};
        auto p2 = Point<float>{centerX + 20.f, centerY};
        if (on)
        {
            ColourGradient gradient{Colours::seagreen, p1, Colours::grey, p2, false};
            if (index) {
                gradient.point1 = p2;
                gradient.point2 = p1;
            }
            else {
                gradient.point2 = p2;
                gradient.point1 = p1;
            }
            g.setGradientFill(gradient);
            g.fillRoundedRectangle(bounds.toFloat(), 5.f);
        }
        else
        {
            g.setColour(Colours::grey);
            g.fillRoundedRectangle(bounds.toFloat(), 5.f);
        }

        g.setColour(Colours::black);
        g.drawRoundedRectangle(bounds.toFloat(), 5.f, 1.f);
        g.fillRect(centerX, (float)bounds.getY() + 5, 1.f, (float)bounds.getHeight() - 10);

        String room = "Room";
        String hall = "Hall";

        g.setColour(Colours::black);

        g.drawFittedText(room, bounds.withTrimmedRight(bounds.getWidth() / 2), Justification::centred, 1);
        g.drawFittedText(hall, bounds.withTrimmedLeft(bounds.getWidth() / 2), Justification::centred, 1);
    }

    bool hitTest (int x, int y) override
    {
        auto w = getWidth();

        auto leftClick = ModifierKeys::currentModifiers.isLeftButtonDown();

        if (getBounds().withTrimmedRight(w / 2).contains(x, y) && leftClick)
        {
            if (on && index == 1)
                on = true;
            else if (!on)
                on = true;
            else
                on = false;
            
            index = 0;

            repaint();

            if (onChange != nullptr)
                onChange(on, index);
            
            return true;
        }
        else if (getBounds().withTrimmedLeft(w / 2).contains(x, y) && leftClick)
        {
            if (on && index == 0)
                on = true;
            else if (!on)
                on = true;
            else
                on = false;
            
            index = 1;

            repaint();

            if (onChange != nullptr)
                onChange(on, index);

            return true;
        }
        else
            return false;
    }

    bool isOn() { return on; }
    void setOn(bool newState) { on = newState; }
    int getIndex() { return index; }
    void setIndex(int newIndex) { index = newIndex; }

    std::function<void(bool, int)> onChange;

private:
    bool on = false;
    int index = 0;
};

class ReverbComponent : public Component
{
    Knob reverbAmount;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> amtAttach;

    ReverbButton reverb;

public:
    ReverbComponent(AudioProcessorValueTreeState& v) : reverbAmount(Knob::Type::Regular)
    {
        addAndMakeVisible(reverbAmount);
        amtAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(v, "roomAmt", reverbAmount);

        addAndMakeVisible(reverb);
        reverb.setOn(*v.getRawParameterValue("reverbType"));
        reverb.setIndex((int)*v.getRawParameterValue("reverbType") - (int)reverb.isOn());
        reverb.onChange = [&](bool isOn, int index)
        {
            if (!isOn)
                v.getParameterAsValue("reverbType") = 0;
            else
                v.getParameterAsValue("reverbType") = isOn + index;
        };
    }

    void paint(Graphics& g) override
    {
        g.fillAll(Colour(0xff256184).withAlpha(0.5f));
    }

    void resized() override
    {
        auto b = getLocalBounds();
        reverbAmount.setBounds(b.removeFromBottom(b.getHeight() / 2));
        reverb.setBounds(b);
    }
};