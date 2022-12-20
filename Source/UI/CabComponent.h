// CabComponent.h

#pragma once

struct CabsComponent : Component, AudioProcessorValueTreeState::Listener
{
private:
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

        // void paint(Graphics &g) override
        // {
        // }

    private:
        /* internal svg that is passed in */
        std::unique_ptr<Drawable> svg;

        Colour color;
    };

    struct MicComponent : Component
    {
        MicComponent(AudioProcessorValueTreeState &a) : apvts(a)
        {
            micPos = static_cast<strix::FloatParameter *>(a.getParameter("cabMicPosX"));
            micDepth = static_cast<strix::FloatParameter *>(a.getParameter("cabMicPosZ"));
        }

        void paint(Graphics &g) override
        {
            auto bounds = getLocalBounds().reduced(2);
            g.setColour(Colours::black.withAlpha(0.25f));
            g.fillRoundedRectangle(bounds.toFloat(), 5.f);

            const float pointWidth = 10.f;

            Rectangle<float> micPoint(posX, posY, pointWidth, pointWidth);
            g.setColour(Colours::white);
            g.fillEllipse(micPoint);
        }

        void mouseDrag(const MouseEvent &event) override
        {
            if (event.eventComponent == this)
            {
                auto x = event.position.getX();
                auto y = event.position.getY();

                auto bounds = getLocalBounds().reduced(2);

                posX = x;
                posY = y;

                apvts.getParameterAsValue("cabMicPosX") = posX / bounds.getWidth();
                apvts.getParameterAsValue("cabMicPosZ") = posY / bounds.getHeight();

                posX = jlimit(7.f, bounds.getWidth() - 12.f, posX);
                posY = jlimit(7.f, bounds.getHeight() - 12.f, posY);

                repaint();
            }
        }

        void mouseDoubleClick(const MouseEvent &event) override
        {
            if (event.eventComponent == this)
            {
                apvts.getParameterAsValue("cabMicPosX") = 0.5f;
                apvts.getParameterAsValue("cabMicPosZ") = 1.f;
                auto bounds = getLocalBounds().reduced(2);
                posX = bounds.getWidth() * micPos->get();
                posY = bounds.getHeight() * micDepth->get();
                repaint();
            }
        }

        void resized() override
        {
            auto bounds = getLocalBounds().reduced(2);
            posX = bounds.getWidth() * micPos->get();
            posY = bounds.getHeight() * micDepth->get();
        }

    private:

        strix::FloatParameter *micPos, *micDepth;
        AudioProcessorValueTreeState &apvts;

        float posX = 0.f, posY = 0.f;
    };

    ChoiceMenu menu;
    std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> menuAttach;

    std::unique_ptr<Drawable> cab_img;

    MicComponent micComp;

    strix::ChoiceParameter *cabType;

    AudioProcessorValueTreeState &apvts;
public:
    /**
     * @param type pointer to type parameter
     */
    CabsComponent(AudioProcessorValueTreeState &a) : menu(a.getParameter("cabType")->getAllValueStrings()), micComp(a), apvts(a)
    {
        apvts.addParameterListener("cabType", this);

        cabType = static_cast<strix::ChoiceParameter*>(apvts.getParameter("cabType"));

        addAndMakeVisible(menu);
        menuAttach = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "cabType", menu);

        addAndMakeVisible(micComp);

        setState();

        setBufferedToImage(true);
    }

    ~CabsComponent()
    {
        apvts.removeParameterListener("cabType", this);
        cabType = nullptr;
    }

    void parameterChanged(const String &paramID, float) override
    {
        if (paramID == "cabType")
        {
            setState();
            repaint();
        }
    }

    void paint(Graphics &g) override
    {
        auto bounds = getLocalBounds().reduced(2).toFloat();
        g.setColour(Colours::beige);
        g.drawRoundedRectangle(bounds, 5.f, 3.f);

        auto img_bounds = bounds.removeFromLeft(bounds.getWidth() * 0.33f).reduced(bounds.getWidth() * 0.1f);

        g.setColour(Colours::darkgrey);
        g.fillRoundedRectangle(img_bounds.toFloat().expanded(img_bounds.getWidth() * 0.2f), 5.f);

        if (cab_img)
            cab_img->drawWithin(g, img_bounds, RectanglePlacement::centred, 1.f);
    }

    /**
     * @param newState if -1 (default), will read from param value. Otherwise, will override param value and set it from this function
    */
    void setState(int newState = -1)
    {
        auto state = cabType->getIndex();
        if (newState != -1)
            state = newState;

        switch (state)
        {
        case 1:
            cab_img = Drawable::createFromImageData(BinaryData::_2x12_svg, BinaryData::_2x12_svgSize);
            cab_img->replaceColour(Colours::transparentBlack, Colours::olivedrab.withAlpha(0.5f));
            break;
        case 2:
            cab_img = Drawable::createFromImageData(BinaryData::_4x12_svg, BinaryData::_4x12_svgSize);
            cab_img->replaceColour(Colours::transparentBlack, Colours::cadetblue.withAlpha(0.5f));
            break;
        case 3:
            cab_img = Drawable::createFromImageData(BinaryData::_6x10_svg, BinaryData::_6x10_svgSize);
            cab_img->replaceColour(Colours::transparentBlack, Colours::chocolate.withAlpha(0.5f));
            break;
        default:
            cab_img = nullptr;
            break;
        }

        if (newState != -1)
            apvts.getParameterAsValue("cabType") = newState;
    }

    void resized() override
    {
        menu.setBounds(getLocalBounds().removeFromTop(getHeight() * 0.35f).removeFromRight(getWidth() * 0.5f).reduced(20, 5));

        micComp.setBounds(getLocalBounds().removeFromBottom(getHeight() * 0.65f).removeFromRight(getWidth() * 0.5f));
    }
};
