// CabComponent.h

#pragma once

struct CabsComponent : Component, AudioProcessorValueTreeState::Listener, Timer
{
private:
    struct MicComponent : Component
    {
        MicComponent(AudioProcessorValueTreeState &a) : apvts(a)
        {
            micPos = static_cast<strix::FloatParameter *>(a.getParameter("cabMicPosX"));
            micDepth = static_cast<strix::FloatParameter *>(a.getParameter("cabMicPosZ"));

            mic_img = Drawable::createFromImageData(BinaryData::mic_svg, BinaryData::mic_svgSize);
            spkr_img = Drawable::createFromImageData(BinaryData::speaker_svg, BinaryData::speaker_svgSize);
        }

        void paint(Graphics &g) override
        {
            auto bounds = getLocalBounds().reduced(5);
            g.setColour(Colours::black.withAlpha(0.25f));
            g.fillRoundedRectangle(bounds.toFloat(), 5.f);

            pos = micBounds.getRelativePoint(micPos->get(), micDepth->get());

            Rectangle<float> micPoint(pos.x - (micWidth * 0.5f), pos.y - (micWidth * 0.5f), micWidth, micWidth);
            g.setColour(Colours::white);
            g.fillEllipse(micPoint);

            spkr_img->replaceColour(Colours::black, Colours::white);
            spkr_img->drawWithin(g, spkrBounds, RectanglePlacement::centred, 1.f);
        }

        void clampPoints()
        {
            pos.x = jlimit(micBounds.getX(), micBounds.getRight() - micWidth * 0.5f, pos.x);
            pos.y = jlimit(micBounds.getY(), micBounds.getBottom() - micWidth * 0.5f, pos.y);
        }

        void mouseDrag(const MouseEvent &event) override
        {
            auto adjBounds = micBounds.reduced(micWidth * 0.5f);
            pos.x = event.position.x - (micWidth * 0.5f);
            pos.y = event.position.y + (micWidth * 0.5f);
            auto xFrac = (pos.x - micBounds.getX()) / adjBounds.getWidth();
            auto yFrac = (pos.y - (getHeight() - micBounds.getHeight())) / adjBounds.getHeight();

            apvts.getParameterAsValue("cabMicPosX").setValue(xFrac);
            apvts.getParameterAsValue("cabMicPosZ").setValue(yFrac);

            clampPoints();
            repaint();
        }

        void mouseDoubleClick(const MouseEvent &event) override
        {
            auto adjBounds = micBounds.reduced(micWidth * 0.5f);
            apvts.getParameterAsValue("cabMicPosX").setValue(apvts.getParameter("cabMicPosX")->getDefaultValue());
            apvts.getParameterAsValue("cabMicPosZ").setValue(apvts.getParameter("cabMicPosZ")->getDefaultValue());
            pos = adjBounds.getRelativePoint(micPos->get(), micDepth->get());
            clampPoints();
            repaint();
        }

        void resized() override
        {
            auto bounds = getLocalBounds().reduced(10);
            auto width = bounds.getWidth();
            auto height = bounds.getHeight();
            auto micBoundsXInset = width * 0.2f;
            auto micBoundsYInset = height * 0.65f;
            micBounds = bounds.removeFromBottom(micBoundsYInset).withWidth(width * 0.5f + (micWidth * 0.5f)).withTrimmedLeft(micBoundsXInset).toFloat();
            spkrBounds = bounds.withTrimmedBottom(height * 0.1f).toFloat();
            clampPoints();
        }

    private:
        strix::FloatParameter *micPos, *micDepth;
        AudioProcessorValueTreeState &apvts;

        std::unique_ptr<Drawable> mic_img, spkr_img;

        // center of mic point
        Point<float> pos;
        const float micWidth = 8.f;
        Rectangle<float> micBounds, spkrBounds;
    };

    ChoiceMenu menu;
    std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> menuAttach;

    Knob::Flags flags{Knob::DRAW_GRADIENT | Knob::DRAW_ARC | Knob::DRAW_SHADOW};
    Knob resoLo{flags}, resoHi{flags};
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> resoLoAttach, resoHiAttach;

    std::unique_ptr<Drawable> cab_img;
    Rectangle<float> cabBounds;

    MicComponent micComp;

    Label title;

    strix::ChoiceParameter *cabType;
    std::atomic<bool> needUpdate = false;

    AudioProcessorValueTreeState &apvts;

public:
    /**
     * @param type pointer to type parameter
     */
    CabsComponent(AudioProcessorValueTreeState &a) : menu(a.getParameter("cabType")->getAllValueStrings()), micComp(a), apvts(a)
    {
        apvts.addParameterListener("cabType", this);

        cabType = static_cast<strix::ChoiceParameter *>(apvts.getParameter("cabType"));

        addAndMakeVisible(menu);
        menuAttach = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(apvts, "cabType", menu);

        addAndMakeVisible(resoLo);
        resoLo.setDefaultValue(0.5f);
        resoLo.setLabel("Reso Lo");
        resoLo.setColor(Colour(BACKGROUND_COLOR), Colours::antiquewhite);
        resoLo.setValueToStringFunction([](float val)
                                        { String s(int(val * 100)); return s + "%"; });
        resoLoAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "cabResoLo", resoLo);

        addAndMakeVisible(resoHi);
        resoHi.setDefaultValue(0.5f);
        resoHi.setLabel("Reso Hi");
        resoHi.setColor(Colour(BACKGROUND_COLOR), Colours::antiquewhite);
        resoHi.setValueToStringFunction([](float val)
                                        { String s(int(val * 100)); return s + "%"; });
        resoHiAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "cabResoHi", resoHi);

        addAndMakeVisible(micComp);

        addAndMakeVisible(title);
        title.setText("Cab", NotificationType::dontSendNotification);
        title.setJustificationType(Justification::centred);

        setState();

        setBufferedToImage(true);

        startTimer(10);
    }

    ~CabsComponent() override
    {
        apvts.removeParameterListener("cabType", this);
        stopTimer();
        cabType = nullptr;
    }

    void parameterChanged(const String &paramID, float) override
    {
        if (paramID == "cabType")
        {
            needUpdate = true;
        }
    }

    void timerCallback() override
    {
        if (needUpdate)
        {
            setState();
            repaint();
            needUpdate = false;
        }
    }

    void paint(Graphics &g) override
    {
        auto bounds = getLocalBounds().reduced(3).toFloat();
        g.setColour(Colours::beige);
        g.drawRoundedRectangle(bounds, 5.f, 3.f);

        g.setColour(Colours::darkgrey);

        if (cab_img)
            cab_img->drawWithin(g, cabBounds, RectanglePlacement::centred, 1.f);
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
            // cab_img->replaceColour(Colours::transparentBlack, Colours::olivedrab.withAlpha(0.5f));
            cab_img->replaceColour(Colours::transparentBlack, Colour(LIGHT_GREEN));
            break;
        case 2:
            cab_img = Drawable::createFromImageData(BinaryData::_4x12_svg, BinaryData::_4x12_svgSize);
            // cab_img->replaceColour(Colours::transparentBlack, Colours::cadetblue.withAlpha(0.5f));
            cab_img->replaceColour(Colours::transparentBlack, Colour(LIGHT_BLUE));
            break;
        case 3:
            cab_img = Drawable::createFromImageData(BinaryData::_6x10_svg, BinaryData::_6x10_svgSize);
            // cab_img->replaceColour(Colours::transparentBlack, Colours::chocolate.withAlpha(0.5f));
            cab_img->replaceColour(Colours::transparentBlack, Colour(DULL_RED));
            break;
        default:
            cab_img = nullptr;
            break;
        }

        if (newState != -1)
            apvts.getParameterAsValue("cabType").setValue(newState);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(5);
        title.setBounds(bounds.removeFromTop(bounds.getHeight() * 0.15f));
        title.setFont(Font(title.getHeight() * 0.75f).withExtraKerningFactor(0.5f));
        micComp.setBounds(bounds.removeFromRight(bounds.getHeight()).reduced(2));
        auto resoBounds = bounds.removeFromRight(bounds.getWidth() * 0.25f);
        resoLo.setBounds(resoBounds.removeFromTop(bounds.getHeight() * 0.5f));
        resoHi.setBounds(resoBounds.removeFromTop(bounds.getHeight() * 0.5f));
        menu.setBounds(bounds.removeFromTop(bounds.getHeight() * 0.3f).removeFromLeft(getWidth() * 0.5f).reduced(20, 5));
        cabBounds = bounds.reduced(5).toFloat();
    }
};
