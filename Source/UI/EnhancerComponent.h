/**
 * EnhancerComponent.h
 * Component for managing enhancers & associated UI, along with the visualizer
 */

struct EnhancerComponent : Component
{
    EnhancerComponent(strix::AudioSource &s, AudioProcessorValueTreeState &apvts)
    {
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
        lfEnhance.setTooltip("A saturating, low-end boost after the amp and before the cab and reverb. The frequency is calibrated depending on the amp's mode.");
        lfEnhance.setLabel("LF Enhancer");
        lfEnhance.setDefaultValue(0.f);
        lfEnhance.setValueToStringFunction(percent);
        lfEnhance.setColor(Colours::black, Colours::antiquewhite);

        addAndMakeVisible(lfAutoGain);
        lfAutoGainAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(apvts, "lfEnhanceAuto", lfAutoGain);
        lfAutoGain.setButtonText("Auto");
        lfAutoGain.setTooltip("Enable automatic gain compensation for the enhancer filter.");

        addAndMakeVisible(lfInvert);
        lfInvAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(apvts, "lfEnhanceInvert", lfInvert);
        lfInvert.setButtonText("Inv");
        lfInvert.setTooltip("Invert the low frequency enhancer. At low levels, this works more like a resonant high-pass, while at higher levels it adds more of a bell-shaped boost.");

        hfAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "hfEnhance", hfEnhance);
        hfEnhance.setTooltip("A saturating, hi-end boost after the amp and before the cab and reverb.");
        hfEnhance.setLabel("HF Enhancer");
        hfEnhance.setDefaultValue(0.f);
        hfEnhance.setValueToStringFunction(percent);
        hfEnhance.setColor(Colours::black, Colours::antiquewhite);

        addAndMakeVisible(hfAutoGain);
        hfAutoGainAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(apvts, "hfEnhanceAuto", hfAutoGain);
        hfAutoGain.setButtonText("Auto");
        hfAutoGain.setTooltip("Enable automatic gain compensation for the enhancer filter.");

        addAndMakeVisible(hfInvert);
        hfInvAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(apvts, "hfEnhanceInvert", hfInvert);
        hfInvert.setButtonText("Inv");
        hfInvert.setTooltip("Invert the high frequency enhancer. At low levels, this works more like a resonant high-pass, while at higher levels it adds a differently-voiced boost to the high-end.");

        lfCutAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "lfCut", lfCut);
        lfCut.setLabel("LF Cut");
        lfCut.setColor(Colour(LIGHT_BLUE), Colours::antiquewhite);
        lfCut.setDefaultValue(0.f);
        lfCut.setValueToStringFunction([](float val)
                                       { if (val == 5.f)
                return String("Off");
            String str((int)val);
            str.append(" Hz", 3); return str; });

        hfCutAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(apvts, "hfCut", hfCut);
        hfCut.setLabel("HF Cut");
        hfCut.setColor(Colour(LIGHT_BLUE), Colours::antiquewhite);
        hfCut.setDefaultValue(1.f);
        hfCut.setValueToStringFunction([](float val)
                                       { if (val == 22000.f)
                return String("Off");
            String str((int)val);
            str.append(" Hz", 3); return str; });

        addAndMakeVisible(title);
        title.setText("Post", NotificationType::dontSendNotification);
        title.setJustificationType(Justification::centred);

        addAndMakeVisible(resizeButton);
        resizeButton.onClick = [&]
        {
            minimized = !resizeButton.getToggleState();
            for (auto *c : getKnobs()) c->setVisible(!minimized);
            for (auto *b : getButtons()) b->setVisible(!minimized);
            if (onResize)
                onResize();
        };
    }

    void paint(Graphics &g) override
    {
        auto bounds = getLocalBounds().reduced(3).toFloat();

        g.setColour(Colours::grey);
        g.drawRoundedRectangle(bounds, 5.f, 3.f);

        g.setFont(getHeight() * 0.085f);
        g.setColour(Colours::white);
        if (minimized) return;
        auto LFLabel = bounds.withTrimmedRight(getWidth() * 0.8f).withTrimmedBottom(getHeight() * 0.5f).translated(0, 5).toNearestInt();
        g.drawFittedText("Low Freq", LFLabel, Justification::centredTop, 1);
        auto HFLabel = bounds.withTrimmedLeft(getWidth() * 0.8f).withTrimmedBottom(getHeight() * 0.5f).translated(0, 5).toNearestInt();
        g.drawFittedText("Hi Freq", HFLabel, Justification::centredTop, 1);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(10, 2);
        auto div = bounds.getWidth() / 3;
        auto left = bounds.removeFromLeft(div);
        auto right = bounds.removeFromRight(div);
        const auto width = bounds.getWidth();
        const auto height = bounds.getHeight();

        auto titleBounds = minimized ? bounds.reduced(width * 0.33f, 0) : bounds.removeFromTop(height * 0.2f).reduced(width * 0.33f, 0);

        title.setBounds(titleBounds);
        title.setFont(Font(title.getHeight() * 0.65f).withExtraKerningFactor(0.25f));
        resizeButton.setBounds(Rectangle(title.getRight(), title.getY(), title.getHeight(), title.getHeight()).reduced(3));

        if (minimized) return;

        const auto buttonHeight = left.getHeight() * 0.25f;
        const auto buttonWidth = left.getWidth() * 0.25f;
        auto leftButtons = left.removeFromLeft(buttonWidth).reduced(0, height * 0.1f);
        auto rightButtons = right.removeFromRight(buttonWidth).reduced(0, height * 0.1f);

        lfCut.setBounds(bounds.removeFromLeft(width * 0.5f));
        hfCut.setBounds(bounds.removeFromLeft(width * 0.5f));
        lfInvert.setBounds(leftButtons.removeFromTop(height * 0.5f).withSizeKeepingCentre(buttonWidth, buttonHeight));
        lfAutoGain.setBounds(leftButtons.withSizeKeepingCentre(buttonWidth, buttonHeight));
        lfInvert.lnf.cornerRadius = lfInvert.getHeight() * 0.25f;
        lfAutoGain.lnf.cornerRadius = lfAutoGain.getHeight() * 0.25f;
        lfEnhance.setBounds(left);
        hfInvert.setBounds(rightButtons.removeFromTop(height * 0.5f).withSizeKeepingCentre(buttonWidth, buttonHeight));
        hfAutoGain.setBounds(rightButtons.withSizeKeepingCentre(buttonWidth, buttonHeight));
        hfInvert.lnf.cornerRadius = hfInvert.getHeight() * 0.25f;
        hfAutoGain.lnf.cornerRadius = hfAutoGain.getHeight() * 0.25f;
        hfEnhance.setBounds(right);
    }

    std::function<void()> onResize;
    std::atomic<bool> minimized = true;
    void toggleMinimized(bool state)
    {
        minimized = state;
        resizeButton.setToggleState(state, NotificationType::dontSendNotification);
        for (auto *c : getKnobs()) c->setVisible(!state);
        for (auto *c : getButtons()) c->setVisible(!state);
        repaint();
    }

private:
    Knob::flags_t knobFlags = Knob::DRAW_GRADIENT | Knob::DRAW_SHADOW | Knob::DRAW_ARC;
    Knob::flags_t hfFlags = knobFlags | Knob::LOG_KNOB;
    Knob hfEnhance{knobFlags}, lfEnhance{knobFlags}, lfCut{knobFlags}, hfCut{hfFlags};

    std::vector<Knob*> getKnobs()
    {
        return {
            &lfEnhance,
            &lfCut,
            &hfCut,
            &hfEnhance};
    }

    LightButton hfAutoGain, lfAutoGain, hfInvert, lfInvert;
    std::vector<LightButton*> getButtons()
    {
        return {
            &hfAutoGain,
            &lfAutoGain,
            &hfInvert,
            &lfInvert
        };
    }
    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> hfAttach, lfAttach, lfCutAttach, hfCutAttach;
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> hfAutoGainAttach, lfAutoGainAttach, hfInvAttach, lfInvAttach;

    Label title;

    ResizeButton resizeButton;

    Colour background = Colour(DEEP_BLUE);
};