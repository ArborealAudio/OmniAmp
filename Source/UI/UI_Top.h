/* UI_Top.h */

struct TopComponent : Component
{
    TopComponent(strix::AudioSource& s, AudioProcessorValueTreeState& apvts) : wave(s)
    {
        mesh = Drawable::createFromImageData(BinaryData::amp_mesh_2_svg, BinaryData::amp_mesh_2_svgSize);

        addAndMakeVisible(wave);
        wave.setInterceptsMouseClicks(false, false);

        addAndMakeVisible(lfEnhance);
        lfAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "lfEnhance", lfEnhance);
        lfEnhance.setTooltip("A saturating, low-end boost after the amp and before the cab and reverb. The frequency is calibrated depending on the amp's mode.\n\nAlt/Option-click to enable Auto Gain.");
        lfEnhance.setColor(Colours::black, Colours::whitesmoke);
        lfEnhance.autoGain.store(*apvts.getRawParameterValue("lfEnhanceAuto"));
        lfEnhance.onAltClick = [&](bool state)
        {
            apvts.getParameterAsValue("lfEnhanceAuto") = state;
        };

        addAndMakeVisible(lfInvert);
        lfInvAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(apvts, "lfEnhanceInvert", lfInvert);
        lfInvert.setButtonText("inv");
        lfInvert.setTooltip("Invert the low frequency enhancer. At low levels, this works more like a resonant high-pass, while at higher levels it adds more of a bell-shaped boost.");

        addAndMakeVisible(hfEnhance);
        hfAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "hfEnhance", hfEnhance);
        hfEnhance.setTooltip("A saturating, hi-end boost after the amp and before the cab and reverb.\n\nAlt/Option-click to enable Auto Gain.");
        hfEnhance.setColor(Colours::white, Colours::wheat);
        hfEnhance.autoGain.store(*apvts.getRawParameterValue("hfEnhanceAuto"));
        hfEnhance.onAltClick = [&](bool state)
        {
            apvts.getParameterAsValue("hfEnhanceAuto") = state;
        };

        addAndMakeVisible(hfInvert);
        hfInvAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(apvts, "hfEnhanceInvert", hfInvert);
        hfInvert.setButtonText("inv");
        hfInvert.setTooltip("Invert the high frequency enhancer. At low levels, this works more like a resonant high-pass, while at higher levels it adds a differently-voiced boost to the high-end.");
    }

    void paint(Graphics& g) override
    {
        auto bounds = getLocalBounds().reduced(1).toFloat();

        g.setColour(background.withMultipliedLightness(0.5));
        g.fillRoundedRectangle(bounds, 5.f);
        g.setColour(Colours::grey);
        g.drawRoundedRectangle(bounds, 5.f, 2.f);

        mesh->drawWithin(g, bounds.reduced(1.f), RectanglePlacement::fillDestination, 0.7f);

        g.setFont(getHeight() * 0.1f);
        g.setColour(Colours::white);

        auto LFLabel = bounds.withTrimmedRight(getWidth() * 0.8f).withTrimmedBottom(getHeight() * 0.5f).toNearestInt();
        g.drawFittedText("LF", LFLabel, Justification::centredTop, 1);

        auto HFLabel = bounds.withTrimmedLeft(getWidth() * 0.8f).withTrimmedBottom(getHeight() * 0.5f).toNearestInt();
        g.drawFittedText("HF", HFLabel, Justification::centredTop, 1);
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

private:
    std::unique_ptr<Drawable> mesh;

    Knob hfEnhance{KnobType::Enhancer}, lfEnhance{KnobType::Enhancer};
    LightButton hfInvert, lfInvert;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> hfAttach, lfAttach;
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> hfInvAttach, lfInvAttach;

    strix::SineWaveComponent wave;

    Colour background = Colour(DEEP_BLUE);
};