// CabComponent.h

#pragma once

struct CabsComponent : Component, AudioProcessorValueTreeState::Listener
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

            clampPoints();
            Rectangle<float> micPoint(posX, posY, micWidth, micWidth);
            mic_img->replaceColour(Colours::black, Colours::white);
            mic_img->drawWithin(g, micPoint, RectanglePlacement::centred, 1.f);

            spkr_img->replaceColour(Colours::black, Colours::white);
            spkr_img->drawWithin(g, spkrBounds, RectanglePlacement::centred, 1.f);
        }

        void clampPoints()
        {
            posX = jlimit(micBounds.getX(), micBounds.getRight() - micWidth, posX);
            posY = jlimit(micBounds.getY(), micBounds.getBottom() - micWidth, posY);
        }

        void mouseDrag(const MouseEvent &event) override
        {
            if (event.eventComponent == this)
            {
                auto adjBounds = micBounds.reduced(micWidth * 0.5f);
                posX = event.position.x - (micWidth * 0.5f); // UI x pos
                float pX = event.position.x; // param x pos
                posY = event.position.y - (micWidth * 0.5f);
                float pY = event.position.y; 
                auto xFrac = (posX - micBounds.getX()) / adjBounds.getWidth();
                auto yFrac = (posY - (getHeight() - micBounds.getHeight())) / adjBounds.getHeight();

                apvts.getParameterAsValue("cabMicPosX") = xFrac;
                apvts.getParameterAsValue("cabMicPosZ") = yFrac;

                repaint();
            }
        }

        void mouseDoubleClick(const MouseEvent &event) override
        {
            if (event.eventComponent == this)
            {
                auto adjBounds = micBounds.reduced(micWidth * 0.5f);
                apvts.getParameterAsValue("cabMicPosX") = 0.5f;
                apvts.getParameterAsValue("cabMicPosZ") = 1.f;
                posX = micBounds.getWidth() * micPos->get();
                posY = micBounds.getHeight() * micDepth->get();
                clampPoints();
                repaint();
            }
        }

        void resized() override
        {
            auto bounds = getLocalBounds().reduced(10);
            auto width = bounds.getWidth();
            auto height = bounds.getHeight();
            micBounds = bounds.removeFromBottom(height * 0.65f).withWidth(width * 0.5f + (micWidth * 0.5f)).withTrimmedLeft(width * 0.1f).toFloat();
            spkrBounds = bounds.withTrimmedBottom(height * 0.1f).toFloat();
            posX = micBounds.getWidth() * micPos->get();
            posY = micBounds.getHeight() * micDepth->get();
            clampPoints();
        }

    private:

        strix::FloatParameter *micPos, *micDepth;
        AudioProcessorValueTreeState &apvts;

        std::unique_ptr<Drawable> mic_img, spkr_img;

        float posX = 0.f, posY = 0.f;
        const float micWidth = 35.f;
        Rectangle<float> micBounds, spkrBounds;
    };

    ChoiceMenu menu;
    std::unique_ptr<AudioProcessorValueTreeState::ComboBoxAttachment> menuAttach;

    Knob::flags_t flags {Knob::DRAW_GRADIENT | Knob::DRAW_ARC | Knob::DRAW_SHADOW};
    Knob resoLo{flags}, resoHi{flags};
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> resoLoAttach, resoHiAttach;

    std::unique_ptr<Drawable> cab_img;
    Rectangle<float> cabBounds;

    MicComponent micComp;

    Label title;

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
            const MessageManagerLock lock;
            setState();
            repaint(cabBounds.toNearestInt());
        }
    }

    void paint(Graphics &g) override
    {
        auto bounds = getLocalBounds().reduced(2).toFloat();
        g.setColour(Colours::beige);
        g.drawRoundedRectangle(bounds, 5.f, 3.f);

        g.setColour(Colours::darkgrey);
        // g.fillRoundedRectangle(img_bounds.toFloat().expanded(img_bounds.getWidth() * 0.2f), 5.f);

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
