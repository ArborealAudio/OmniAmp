/**
 * EnhancerComponent.h
 * Component for managing enhancers & associated UI, along with the visualizer
 */

struct EnhancerComponent : Component
{
    EnhancerComponent(strix::AudioSource &s, AudioProcessorValueTreeState &apvts) : wave(s)
    {
        mesh = Drawable::createFromImageData(BinaryData::amp_mesh_2_svg, BinaryData::amp_mesh_2_svgSize);

        // addAndMakeVisible(wave);
        wave.setInterceptsMouseClicks(false, false);

        auto percent = [](float val)
        {
            auto str = String(static_cast<int>(val * 100));
            str.append("%", 1);
            return str;
        };

        for (auto *k : getKnobs())
        {
            addAndMakeVisible(*k);
            k->setColor(Colours::black, Colours::antiquewhite);
        }

        lfAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "lfEnhance", lfEnhance);
        lfEnhance.setTooltip("A saturating, low-end boost after the amp and before the cab and reverb. The frequency is calibrated depending on the amp's mode.\n\nAlt/Option-click to enable Auto Gain.");
        lfEnhance.setLabel("LF Enhancer");
        lfEnhance.setDefaultValue(apvts.getParameter("lfEnhance")->getDefaultValue());
        lfEnhance.setValueToStringFunction(percent);
        lfEnhance.setColor(Colours::black, Colours::antiquewhite);
        lfEnhance.autoGain.store(*apvts.getRawParameterValue("lfEnhanceAuto"));
        lfEnhance.onAltClick = [&](bool state)
        {
            apvts.getParameterAsValue("lfEnhanceAuto") = state;
        };

        addAndMakeVisible(lfInvert);
        lfInvAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(apvts, "lfEnhanceInvert", lfInvert);
        lfInvert.setButtonText("inv");
        lfInvert.setTooltip("Invert the low frequency enhancer. At low levels, this works more like a resonant high-pass, while at higher levels it adds more of a bell-shaped boost.");

        hfAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "hfEnhance", hfEnhance);
        hfEnhance.setTooltip("A saturating, hi-end boost after the amp and before the cab and reverb.\n\nAlt/Option-click to enable Auto Gain.");
        hfEnhance.setLabel("HF Enhancer");
        hfEnhance.setDefaultValue(apvts.getParameter("hfEnhance")->getDefaultValue());
        hfEnhance.setValueToStringFunction(percent);
        hfEnhance.setColor(Colours::black, Colours::antiquewhite);
        hfEnhance.autoGain.store(*apvts.getRawParameterValue("hfEnhanceAuto"));
        hfEnhance.onAltClick = [&](bool state)
        {
            apvts.getParameterAsValue("hfEnhanceAuto") = state;
        };

        addAndMakeVisible(hfInvert);
        hfInvAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(apvts, "hfEnhanceInvert", hfInvert);
        hfInvert.setButtonText("inv");
        hfInvert.setTooltip("Invert the high frequency enhancer. At low levels, this works more like a resonant high-pass, while at higher levels it adds a differently-voiced boost to the high-end.");

        lfCutAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "lfCut", lfCut);
        lfCut.setLabel("LF Cut");
        lfCut.setDefaultValue(0.f);
        lfCut.setValueToStringFunction([](float val)
                                       { if (val == 5.f)
                return String("Off");
            String str((int)val);
            str.append(" Hz", 3); return str; });

         hfCutAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "hfCut", hfCut);
        hfCut.setLabel("HF Cut");
        hfCut.setDefaultValue(22000.f);
        hfCut.setValueToStringFunction([](float val)
                                       { if (val == 22000.f)
                return String("Off");
            String str((int)val);
            str.append(" Hz", 3); return str; });
    }

    void paint(Graphics &g) override
    {
        auto bounds = getLocalBounds().reduced(1).toFloat();

        g.setColour(Colours::grey);
        g.drawRoundedRectangle(bounds, 5.f, 2.f);

        g.setFont(getHeight() * 0.1f);
        g.setColour(Colours::white);
        auto LFLabel = bounds.withTrimmedRight(getWidth() * 0.8f).withTrimmedBottom(getHeight() * 0.5f).toNearestInt();
        g.drawFittedText("Low Freq", LFLabel, Justification::centredTop, 1);
        auto HFLabel = bounds.withTrimmedLeft(getWidth() * 0.8f).withTrimmedBottom(getHeight() * 0.5f).toNearestInt();
        g.drawFittedText("Hi Freq", HFLabel, Justification::centredTop, 1);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10, 2);
        auto div = bounds.getWidth() / 3;
        auto left = bounds.removeFromLeft(div);
        auto right = bounds.removeFromRight(div);
        const auto width = bounds.getWidth();

        auto invHeight = left.getHeight() * 0.25f;
        auto invWidth = left.getWidth() * 0.25f;

        // wave.setBounds(bounds.reduced(10, (float)bounds.getHeight() * 0.2f));
        lfCut.setBounds(bounds.removeFromLeft(width * 0.5f));
        hfCut.setBounds(bounds.removeFromLeft(width * 0.5f));
        lfInvert.setBounds(left.removeFromLeft(invWidth).withSizeKeepingCentre(invWidth, invHeight));
        lfInvert.lnf.cornerRadius = lfInvert.getHeight() * 0.25f;
        lfEnhance.setBounds(left);
        hfInvert.setBounds(right.removeFromRight(invWidth).withSizeKeepingCentre(invWidth, invHeight));
        hfInvert.lnf.cornerRadius = hfInvert.getHeight() * 0.25f;
        hfEnhance.setBounds(right);
    }

private:
    std::unique_ptr<Drawable> mesh;

    Knob::flags_t knobFlags = Knob::DRAW_GRADIENT | Knob::DRAW_SHADOW | Knob::DRAW_ARC;
    Knob hfEnhance{knobFlags}, lfEnhance{knobFlags}, lfCut{Knob::DRAW_GRADIENT | Knob::DRAW_SHADOW}, hfCut{Knob::DRAW_GRADIENT | Knob::DRAW_SHADOW | Knob::LOG_KNOB};

    std::vector<Knob*> getKnobs()
    {
        return {
            &lfEnhance,
            &lfCut,
            &hfCut,
            &hfEnhance};
    }

    LightButton hfInvert, lfInvert;
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> hfAttach, lfAttach, lfCutAttach, hfCutAttach;
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> hfInvAttach, lfInvAttach;

    strix::SineWaveComponent wave;

    Colour background = Colour(DEEP_BLUE);
};