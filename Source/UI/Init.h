// Init.h
// A component for prompting the mode on initial GUI load

#pragma once

class InitComponent : public Component
{
    struct InitButton : TextButton
    {
        InitButton(const String& buttonText) : TextButton(buttonText)
        {
            setColour(TextButton::ColourIds::buttonColourId, Colours::darkgrey);
        }
    };

public:
    InitComponent()
    {
        addAndMakeVisible(guitar);
        addAndMakeVisible(bass);
        addAndMakeVisible(channel);
    }

    void paint(Graphics& g) override
    {
        auto b = getLocalBounds().reduced(1).toFloat();
        g.drawImage(img, b, RectanglePlacement::doNotResize);
        g.setColour(Colour(TOP_TRIM).withAlpha(0.7f));
        g.fillRoundedRectangle(b, 5.f);
        g.setColour(Colour(TOP_TRIM));
        g.drawRoundedRectangle(b, 5.f, 2.f);

        g.setColour(Colours::white);
        g.setFont(20.f);
        g.drawFittedText("Select a mode:", getLocalBounds().removeFromTop(getHeight() * 0.5f), Justification::centred, 2);
    }

    void resized() override
    {
        auto b = getLocalBounds();
        auto third = getWidth() / 3;
        auto heightPadding = getHeight() * 0.4f;

        guitar.setBounds(b.removeFromLeft(third).reduced(third * 0.1f, heightPadding));
        bass.setBounds(b.removeFromLeft(third).reduced(third * 0.1f, heightPadding));
        channel.setBounds(b.reduced(third * 0.1f, heightPadding));
    }

    void setImage(const Image &newImage)
    {
        img = newImage;
        Blur::blurImage<4, true>(img);
    }

    InitButton guitar{"Guitar"}, bass{"Bass"}, channel{"Anything"};

private:
    Image img;
};