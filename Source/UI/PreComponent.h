/**
 * PreComponent.h
 * A component container for the pre-amp features such as
 * compressor, distortion, and pre-emphasis filters
 */

struct PreComponent : Component
{

    PreComponent(strix::VolumeMeterSource &vs, AudioProcessorValueTreeState &v) : vts(v),
                                                                                  grMeter(vs, strix::VolumeMeterComponent::Horizontal | strix::VolumeMeterComponent::Reduction | strix::VolumeMeterComponent::Background, vts.getRawParameterValue("comp"))
    {
        for (auto c : getComps())
        {
            addAndMakeVisible(*c);
            if (auto k = dynamic_cast<Knob *>(c))
                k->setColor(Colour(BACKGROUND_COLOR), Colours::antiquewhite);
        }

        emphasisAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(vts, "stereoEmphasis", emphasis);
        emphasis.setDefaultValue(vts.getParameter("stereoEmphasis")->getDefaultValue());
        emphasis.setLabel("Stereo Emphasis");
        emphasis.setTooltip("Stereo emphasis control--a compensated drive control for the stereo width going *into* the processing.\n\nUse this for fine-tuning the compression or saturation of the stereo sides.");
        emphasis.setValueToStringFunction([](float val)
                                          { val = jmap(val, -100.f, 100.f);
                                        String s((int)val); return s + "%"; });

        doublerAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(vts, "doubler", doubler);
        doubler.setDefaultValue(vts.getParameter("doubler")->getDefaultValue());
        doubler.setLabel("Doubler");
        doubler.setTooltip("Synthesize a stereo signal from a mono input.");
        doubler.setValueToStringFunction([](float val)
                                         { String s(int(val * 100)); return s + "%"; });

        msAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(vts, "m/s", midSide);
        midSide.setButtonText(midSide.getToggleState() ? "M/S" : "Stereo");
        midSide.setTooltip("Toggle for Stereo and Mid/Side processing.\n\nIn Stereo mode, Left and Right channels will be processed independently in the amp. In Mid/Side mode, the middle of the stereo image will be separated from the sides and then processed independently.\n\nThis is useful if you want to do Mid/Side compression or saturation, which can create a more spacious stereo image.");
        midSide.onStateChange = [&]
        { midSide.setButtonText(midSide.getToggleState() ? "M/S" : "Stereo"); };

        lfAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(vts, "lfEmphasis", lfEmph);
        lfEmph.setDefaultValue(0.5f);
        lfEmph.setLabel("LF Emphasis");
        lfEmph.setValueToStringFunction([](float val)
                                        { auto str = String(val, 1);
                                        str.append("dB", 2);
                                        return str; });
        lfEmph.setTooltip("Adds or subtracts low-end before all saturation and compression, then an equal and opposite compensation after the saturation and compression. Useful for getting more saturation from the low-end without boosting it in volume.");

        lfFreqAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(vts, "lfEmphasisFreq", lfFreq);
        lfFreq.outlineColor = Colours::antiquewhite;
        lfFreq.baseColor = Colour(LIGHT_BLUE);
        lfFreq.minValue = 30.0;
        lfFreq.maxValue = 1800.0;

        hfFreqAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(vts, "hfEmphasisFreq", hfFreq);
        hfFreq.outlineColor = Colours::antiquewhite;
        hfFreq.baseColor = Colour(LIGHT_BLUE);
        hfFreq.minValue = 1800.0;
        hfFreq.maxValue = 18000.0;

        hfAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(vts, "hfEmphasis", hfEmph);
        hfEmph.setDefaultValue(0.5f);
        hfEmph.setLabel("HF Emphasis");
        hfEmph.setValueToStringFunction([](float val)
                                        { auto str = String(val, 1);
                                        str.append("dB", 2);
                                        return str; });
        hfEmph.setTooltip("Adds or subtracts high-end before all saturation and compression, then an equal and opposite compensation after the saturation and compression. Useful for getting more saturation from the high-end without boosting it in volume.");

        compAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(vts, "comp", comp);
        comp.setDefaultValue(vts.getParameter("comp")->getDefaultValue());
        comp.setLabel("Opto Comp");
        comp.setValueToStringFunction([](float val)
                                      { auto str = String(int(val * 100.0));
                                        str.append("%", 1);
                                        return str; });
        comp.setTooltip("An opto-style compressor, with signal-dependent attack and release. Increasing gives more compression and sustain, and can also boost the volume going into the amp.");

        compLinkAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(vts, "compLink", compLink);
        compLink.setButtonText("Link");

        compPosAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(vts, "compPos", compPos);
        compPos.lnf.shouldFillOnToggle = false;
        compPos.setButtonText(compPos.getToggleState() ? "Post" : "Pre");
        compPos.onStateChange = [&]
        {
            compPos.setButtonText(compPos.getToggleState() ? "Post" : "Pre");
        };

        grMeter.meterColor = Colours::oldlace;
        grMeter.backgroundColor = Colour(BACKGROUND_COLOR).contrasting(0.15f);
        addAndMakeVisible(grMeter);

        addAndMakeVisible(resizeButton);
        resizeButton.onClick = [&]
        {
            minimized = !resizeButton.getToggleState();
            for (auto *c : getComps()) c->setVisible(!minimized);
            grMeter.setVisible(!minimized);
            if (onResize)
                onResize();
        };

        addAndMakeVisible(title);
        title.setText("Pre", NotificationType::dontSendNotification);
        title.setJustificationType(Justification::centred);
    }

    std::function<void()> onResize;
    std::atomic<bool> minimized = true;
    void toggleMinimized(bool state)
    {
        minimized = state;
        resizeButton.setToggleState(!state, NotificationType::sendNotificationSync);
        for (auto *c : getComps()) c->setVisible(!state);
        grMeter.setVisible(!state);
        // repaint();
    }

    void paint(Graphics &g) override
    {
        auto bounds = getLocalBounds().reduced(3).toFloat();
        g.setColour(Colours::antiquewhite);
        g.drawRoundedRectangle(bounds, 5.f, 3.f);

        if (!minimized)
        {
            float div = doubler.getRight() + (grMeter.getX() - doubler.getRight()) / 2;
            g.fillRoundedRectangle(div - 1.5f, 5.f, 3.f, getHeight() - 10.f, 3.f);
        }
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(2);
        auto compSection = bounds.removeFromRight(bounds.getWidth() * 0.3f);
        const auto compSectW = compSection.getWidth();
        grMeter.setBounds(compSection.removeFromBottom(compSection.getHeight() * 0.33f).reduced(compSectW * 0.03f));
        const auto compSectH = compSection.getHeight(); // compSect height after GR meter
        auto w = bounds.getWidth();
        int chunk = w / 5;

        auto msBounds = minimized ? Rectangle(0, 0, 0, 0) : bounds.removeFromLeft(chunk).withSizeKeepingCentre(chunk * 0.66f, bounds.getHeight() * 0.33f);
        auto titleBounds = minimized ? bounds.removeFromLeft(chunk).withSizeKeepingCentre(chunk * 0.66f, bounds.getHeight())
                      : msBounds.translated(0, -bounds.getHeight() / 3);
        midSide.setBounds(msBounds);
        midSide.lnf.cornerRadius = midSide.getHeight() * 0.25f;

        title.setBounds(titleBounds);
        title.setFont(Font(title.getHeight() * 0.65f).withExtraKerningFactor(0.25f));

        resizeButton.setBounds(Rectangle(title.getRight() - 10, title.getY(), title.getHeight(), title.getHeight()).reduced(12));

        if (minimized) return;

        for (auto *c : getSelectComps(StereoEmphasis | LFEmph | HFEmph | Doubler))
        {
            if (c == &lfEmph)
            {
                auto subBounds = bounds.removeFromLeft(chunk);
                layoutComponents(getSelectComps(LFEmph | LFFreq), subBounds, true, 0.7f, 0.025f);
                lfFreq.cornerRadius = lfFreq.getHeight() * 0.5f;
            }
            else if (c == &hfEmph)
            {
                auto subBounds = bounds.removeFromLeft(chunk);
                layoutComponents(getSelectComps(HFEmph | HFFreq), subBounds, true, 0.7f, 0.025f);
                hfFreq.cornerRadius = hfFreq.getHeight() * 0.5f;
            }
            else
                c->setBounds(bounds.removeFromLeft(chunk));
            auto *knob = static_cast<Knob *>(c);
            knob->setOffset(0, -3);
            knob->setTextOffset(0, -2);
        }

        auto compKnobBounds = compSection.removeFromLeft(compSectW * 0.5f).reduced(5, 0);
        comp.setBounds(compKnobBounds);
        comp.setOffset(0, -5);
        comp.setTextOffset(0, -2);
        compPos.setBounds(compSection.removeFromTop(compSectH * 0.5f - 2).reduced(compSectW * 0.125f, compSectH * 0.07f));
        compLink.setBounds(compSection.removeFromTop(compSectH * 0.5f - 2).reduced(compSectW * 0.125f, compSectH * 0.07f));
        compPos.lnf.cornerRadius = compPos.getHeight() * 0.25f;
        compLink.lnf.cornerRadius = compLink.getHeight() * 0.25f;
    }

private:
    AudioProcessorValueTreeState &vts;

    Knob::Flags knobFlags = Knob::DRAW_GRADIENT | Knob::DRAW_SHADOW | Knob::DRAW_ARC;

    Knob emphasis{knobFlags}, doubler{knobFlags}, lfEmph{knobFlags}, hfEmph{knobFlags}, comp{knobFlags};
    SimpleSlider lfFreq, hfFreq;

    LightButton midSide, compLink, compPos;

    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> emphasisAttach, doublerAttach, lfAttach, lfFreqAttach, hfAttach, hfFreqAttach, compAttach;
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> msAttach, compLinkAttach, compPosAttach;

    strix::VolumeMeterComponent grMeter;

    ResizeButton resizeButton;

    Label title;

    enum
    {
        MidSide = 1,
        StereoEmphasis = 1 << 1,
        LFEmph = 1 << 2,
        LFFreq = 1 << 3,
        HFEmph = 1 << 4,
        HFFreq = 1 << 5,
        Doubler = 1 << 6,
        Comp = 1 << 7,
        CompPos = 1 << 8,
        CompLink = 1 << 9
    };

    // returns top knobs & buttons (everything but GR meter)
    std::vector<Component *> getComps()
    {
        return {
            &midSide,
            &emphasis,
            &lfEmph,
            &lfFreq,
            &hfEmph,
            &hfFreq,
            &doubler,
            &comp,
            &compPos,
            &compLink};
    }
    // get a vector of select components using a bitmask
    std::vector<Component *> getSelectComps(uint16_t mask)
    {
        auto c = getComps();
        std::vector<Component *> out;
        for (size_t i = 0; i < c.size(); ++i)
        {
            if ((1 << i) & mask)
                out.emplace_back(c[i]);
        }
        return out;
    }
};