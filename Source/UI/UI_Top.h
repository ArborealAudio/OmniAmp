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
        lfEnhance.setColor(Colours::black, Colours::whitesmoke);
        lfEnhance.autoGain.store(*apvts.getRawParameterValue("lfEnhanceAuto"));
        lfEnhance.onAltClick = [&](bool state)
        {
            apvts.getParameterAsValue("lfEnhanceAuto") = state;
        };

        addAndMakeVisible(hfEnhance);
        hfAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "hfEnhance", hfEnhance);
        hfEnhance.setColor(Colours::white, Colours::wheat);
        hfEnhance.autoGain.store(*apvts.getRawParameterValue("hfEnhanceAuto"));
        hfEnhance.onAltClick = [&](bool state)
        {
            apvts.getParameterAsValue("hfEnhanceAuto") = state;
        };
    }

    void paint(Graphics& g) override
    {
        auto bounds = getLocalBounds().reduced(1).toFloat();

        g.setColour(Colour(BACKGROUND_COLOR));
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

        wave.setBounds(bounds.reduced(10, (float)bounds.getHeight() * 0.2f));
        lfEnhance.setBounds(left);
        hfEnhance.setBounds(right);
    }

private:
    std::unique_ptr<Drawable> mesh;
    std::unique_ptr<Image> blur;

    Knob hfEnhance{KnobType::Enhancer}, lfEnhance{KnobType::Enhancer};
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> hfAttach, lfAttach;

    strix::SineWaveComponent wave;
};