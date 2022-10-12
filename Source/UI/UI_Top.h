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
        // auto topsection = top.withTrimmedBottom(top.getHeight() / 2).reduced(10, 0).translated(0, 10).toFloat();

        auto bounds = getLocalBounds().reduced(1).toFloat();

        g.setColour(Colour(BACKGROUND_COLOR));
        g.fillRoundedRectangle(bounds, 5.f);
        g.setColour(Colours::grey);
        g.drawRoundedRectangle(bounds, 5.f, 2.f);

        // g.reduceClipRegion(topsection.toNearestInt());
        mesh->drawWithin(g, bounds.reduced(1.f), RectanglePlacement::fillDestination, 0.7f);

        // if (blur != nullptr)
        //     g.drawImage(*blur, wave.getBoundsInParent().toFloat(), RectanglePlacement::doNotResize);

        g.setColour(Colour(DEEP_BLUE));
        g.drawRoundedRectangle(wave.getBoundsInParent().toFloat(), 5.f, 2.f);
    }

    void resized() override
    {
        // if (blur == nullptr)
        //     return;
        // blur->clear(blur->getBounds());
        // wave.setVisible(false);
        // blur = std::make_unique<Image>(createComponentSnapshot(wave.getBoundsInParent()));
        // wave.setVisible(true);

        // gin::applyContrast(*blur, -35);
        // gin::applyStackBlur(*blur, 10);

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