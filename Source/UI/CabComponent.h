// CabComponent.h

#pragma once

class CabsComponent : public Component, private Timer
{
    struct Cab : DrawableButton
    {
        Cab(const String &buttonName, ButtonStyle buttonStyle)
            : DrawableButton(buttonName, buttonStyle)
        {
            setEdgeIndent(10);
            setClickingTogglesState(true);
        }

        void setDrawable(const void *svgData, size_t svgSize)
        {
            svg = Drawable::createFromImageData(svgData, svgSize);
        }

        void setColor(Colour newColor)
        {
            color = newColor.withAlpha(0.5f);
            svg->replaceColour(Colours::transparentBlack, Colours::grey);

            setImages();
        }

        /* call after drawables and color have been set */
        void setImages()
        {
            auto overSVG = svg->createCopy();
            auto downSVG = svg->createCopy();

            overSVG->replaceColour(Colours::black, Colours::white);
            downSVG->replaceColour(Colours::grey, color);

            auto overOnSVG = downSVG->createCopy();
            overOnSVG->replaceColour(color, color.withMultipliedBrightness(1.5));

            DrawableButton::setImages(svg.get(), overSVG.get(), nullptr, nullptr,
                                      downSVG.get(), overOnSVG.get());
        }

        void paint(Graphics &g) override
        {
            g.setColour(Colours::grey);
            g.drawRect(getLocalBounds());
        }

    private:
        /* internal svg that is passed in */
        std::unique_ptr<Drawable> svg;

        Colour color;
    };

    std::array<Cab, 3> cab{{{Cab("2x12", DrawableButton::ButtonStyle::ImageFitted)},
                            {Cab("4x12", DrawableButton::ButtonStyle::ImageFitted)},
                            {Cab("6x10", DrawableButton::ButtonStyle::ImageFitted)}}};

    std::atomic<float> *cabType;
    int lastType = 0;

public:
    /// @brief Constructor
    /// @param type pointer to cab type parameter
    CabsComponent(std::atomic<float>* type) : cabType(type)
    {
        addAndMakeVisible(cab[0]);
        cab[0].setDrawable(BinaryData::_2x12_svg, BinaryData::_2x12_svgSize);
        cab[0].setColor(Colours::olivedrab);
        cab[0].onClick = [&]
        {
            auto state = cab[0].getToggleState();
            setState(state ? 1 : 0);
            if (cabChanged)
                cabChanged(state);
        };

        addAndMakeVisible(cab[1]);
        cab[1].setDrawable(BinaryData::_4x12_svg, BinaryData::_4x12_svgSize);
        cab[1].setColor(Colours::cadetblue);
        cab[1].onClick = [&]
        { setState(cab[1].getToggleState() ? 2 : 0); if (cabChanged) cabChanged(cab[1].getToggleState() ? 2 : 0); };

        addAndMakeVisible(cab[2]);
        cab[2].setDrawable(BinaryData::_6x10_svg, BinaryData::_6x10_svgSize);
        cab[2].setColor(Colours::chocolate);
        cab[2].onClick = [&]
        { setState(cab[2].getToggleState() ? 3 : 0); if (cabChanged) cabChanged(cab[2].getToggleState() ? 3 : 0); };

        setBufferedToImage(true);

        startTimerHz(15);
    }

    ~CabsComponent()
    {
        stopTimer();
    }

    void paint(Graphics &g) override { g.fillAll(Colours::beige); }

    /// @brief function for setting new parameter pointers if need be
    /// @param cabType pointer to cab type param
    void setStatePointers(std::atomic<float>* cabType)
    {
        this->cabType = cabType;
    }

    void setState(int state)
    {
        for (int i = 0; i < 3; ++i)
        {
            if (i + 1 == state)
            {
                cab[i].setToggleState(true, NotificationType::dontSendNotification);
            }
            else
            {
                cab[i].setToggleState(false, NotificationType::dontSendNotification);
            }
        }

        lastType = *cabType;
    }

    void resized() override
    {
        auto bounds = getLocalBounds();
        auto third = (float)bounds.getWidth() / 3.f;

        for (int i = 0; i < 3; ++i)
        {
            if (i < 2)
                cab[i].setBounds(bounds.removeFromLeft(third));
            else
                cab[i].setBounds(bounds);
        }
    }

    void timerCallback() override
    {
        if (lastType != (int)*cabType)
            setState((int)*cabType);
    }

    std::function<void(int)> cabChanged;
};
