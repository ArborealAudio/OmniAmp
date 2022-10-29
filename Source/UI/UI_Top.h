/* UI_Top.h */

struct TopComponent : Component, private Timer
{
    TopComponent(strix::AudioSource& s, AudioProcessorValueTreeState& apvts) : wave(s)
    {
        mesh = Drawable::createFromImageData(BinaryData::amp_mesh_2_svg, BinaryData::amp_mesh_2_svgSize);

        currentMode = apvts.getRawParameterValue("mode");

        addAndMakeVisible(wave);
        wave.setInterceptsMouseClicks(false, false);

        addAndMakeVisible(lfEnhance);
        lfAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "lfEnhance", lfEnhance);
        lfEnhance.setColor(Colours::black, Colours::whitesmoke);
        lfEnhance.autoGain.store(*apvts.getRawParameterValue("lfEnhanceAuto"));
        lfEnhance.onAltClick = [&](bool state)
        {
            apvts.getParameterAsValue("lfEnhanceAuto") = state;
        };

        addAndMakeVisible(lfInvert);
        lfInvAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(apvts, "lfEnhanceInvert", lfInvert);
        lfInvert.setButtonText(String::fromUTF8("inv"));

        addAndMakeVisible(hfEnhance);
        hfAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "hfEnhance", hfEnhance);
        hfEnhance.setColor(Colours::white, Colours::wheat);
        hfEnhance.autoGain.store(*apvts.getRawParameterValue("hfEnhanceAuto"));
        hfEnhance.onAltClick = [&](bool state)
        {
            apvts.getParameterAsValue("hfEnhanceAuto") = state;
        };

        addAndMakeVisible(hfInvert);
        hfInvAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(apvts, "hfEnhanceInvert", hfInvert);
        hfInvert.setButtonText("inv");

        updateBackgroundColor();

        startTimerHz(15);
    }

    ~TopComponent() { stopTimer(); }

    void paint(Graphics& g) override
    {
        auto bounds = getLocalBounds().reduced(1).toFloat();

        g.setColour(background.withMultipliedLightness(0.33));
        g.fillRoundedRectangle(bounds, 5.f);
        g.setColour(Colours::grey);
        g.drawRoundedRectangle(bounds, 5.f, 2.f);

        mesh->drawWithin(g, bounds.reduced(1.f), RectanglePlacement::fillDestination, 0.7f);

        g.setColour(Colour(DEEP_BLUE));
        g.drawRoundedRectangle(wave.getBoundsInParent().toFloat(), 5.f, 2.f);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10, 2);
        auto div = bounds.getWidth() / 3;
        auto left = bounds.removeFromLeft(div);
        auto right = bounds.removeFromRight(div);

        auto invHeight = left.getHeight() * 0.45;

        wave.setBounds(bounds.reduced(10, (float)bounds.getHeight() * 0.2f));
        lfInvert.setBounds(left.removeFromLeft(left.getWidth() * 0.2).reduced(0, invHeight));
        lfEnhance.setBounds(left);
        hfInvert.setBounds(right.removeFromRight(right.getWidth() * 0.2).reduced(0, invHeight));
        hfEnhance.setBounds(right);
    }

    void timerCallback() override
    {
        if ((int)*currentMode == lastMode)
            return;

        updateBackgroundColor();

        repaint(getLocalBounds());

        lastMode = *currentMode;
    }

    inline void updateBackgroundColor()
    {
        switch (int(currentMode->load()))
        {
        case 0:
            background = Colour(AMP_COLOR);
            break;
        case 1:
            background = Colours::slategrey;
            break;
        case 2:
            background = Colours::darkgrey;
            break;
        default:
            background = Colour(AMP_COLOR);
            break;
        }
    }

private:
    std::unique_ptr<Drawable> mesh;
    std::unique_ptr<Image> blur;

    Knob hfEnhance{KnobType::Enhancer}, lfEnhance{KnobType::Enhancer};
    LightButton hfInvert, lfInvert;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> hfAttach, lfAttach;
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> hfInvAttach, lfInvAttach;

    strix::SineWaveComponent wave;

    Colour background;

    int lastMode = 0;
    std::atomic<float> *currentMode;
};