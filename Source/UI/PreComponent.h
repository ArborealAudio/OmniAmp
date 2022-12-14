/**
 * PreComponent.h
 * A component container for the pre-amp features such as
 * compressor, distortion, and pre-emphasis filters
 */

struct PreComponent : Component,
                      Timer
{

    PreComponent(strix::VolumeMeterSource &vs, AudioProcessorValueTreeState &v) : vts(v),
                                                                                  grMeter(vs, strix::VolumeMeterComponent::Reduction, vts.getRawParameterValue("comp"))
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
        lfEmph.setDefaultValue(vts.getParameter("lfEmphasis")->getDefaultValue());
        lfEmph.setLabel("LF Emphasis");
        lfEmph.setValueToStringFunction([](float val)
                                        { auto str = String(val, 1);
                                        str.append("dB", 2);
                                        return str; });
        lfEmph.setTooltip("Adds or subtracts low-end before all saturation and compression, then an equal and opposite compensation after the saturation and compression. Useful for getting more saturation from the low-end without boosting it in volume.");

        lfFreqAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(vts, "lfEmphasisFreq", lfFreq);
        lfFreq.outlineColor = Colours::white;
        lfFreq.baseColor = Colour(BACKGROUND_COLOR).withMultipliedLightness(2.f).withMultipliedSaturation(1.25f);
        lfFreq.minValue = 30.0;
        lfFreq.maxValue = 1800.0;

        hfFreqAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(vts, "hfEmphasisFreq", hfFreq);
        hfFreq.outlineColor = Colours::white;
        hfFreq.baseColor = Colour(BACKGROUND_COLOR).withMultipliedLightness(2.f).withMultipliedSaturation(1.25f);
        hfFreq.minValue = 1800.0;
        hfFreq.maxValue = 18000.0;

        hfAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(vts, "hfEmphasis", hfEmph);
        hfEmph.setDefaultValue(vts.getParameter("hfEmphasis")->getDefaultValue());
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
        comp.setTooltip("An opto-style compressor, with program-dependent attack and release. Increasing values give more compression and sustain, and can also boost the volume going into the amp.");

        compLinkAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(vts, "compLink", compLink);
        compLink.setButtonText("Link");

        compPosAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(vts, "compPos", compPos);
        compPos.setButtonText(compPos.getToggleState() ? "Post" : "Pre");
        compPos.onStateChange = [&]
        {
            compPos.setButtonText(compPos.getToggleState() ? "Post" : "Pre");
        };

        // distAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(vts, "dist", dist);
        // dist.setDefaultValue(vts.getParameter("dist")->getDefaultValue());
        // dist.setLabel("Overdrive");
        // dist.setValueToStringFunction([](float val)
        //                               { return String(val * 10.0, 1); });
        // dist.setTooltip("A one-knob distortion pedal. Fully bypassed at 0.");

        grMeter.meterColor = Colours::oldlace;
        addAndMakeVisible(grMeter);

        startTimerHz(30);
    }

    ~PreComponent() override{ stopTimer(); }

    void timerCallback() override
    {
        if (*vts.getRawParameterValue("ampOn"))
            grMeter.setState(vts.getRawParameterValue("comp"));
        else
            grMeter.setState(vts.getRawParameterValue("ampOn"));
    }

    void paint(Graphics &g) override
    {
        g.setColour(Colours::antiquewhite);
        g.drawRoundedRectangle(getLocalBounds().reduced(1).toFloat(), 5.f, 3.f);

        float div = doubler.getRight() + (grMeter.getX() - doubler.getRight()) / 2;
        g.fillRoundedRectangle(div - 1.5f, 5.f, 3.f, getHeight() - 5.f, 3.f);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced(2);
        auto compSection = bounds.removeFromRight(bounds.getWidth() * 0.263f);
        auto compSectW = compSection.getWidth();
        grMeter.setBounds(compSection.removeFromLeft(compSectW / 3).reduced(compSectW * 0.1f, 0));
        auto w = bounds.getWidth();
        int chunk = w / 5;
        for (auto *c : getSelectComps(MidSide | StereoEmphasis | LFEmph | HFEmph | Doubler))
        {
            if (c == &lfEmph)
            {
                auto subBounds = bounds.removeFromLeft(chunk);
                layoutComponents(getSelectComps(LFEmph | LFFreq), subBounds, true, 0.75f);
                lfFreq.cornerRadius = lfFreq.getHeight() * 0.5f;
            }
            else if (c == &hfEmph)
            {
                auto subBounds = bounds.removeFromLeft(chunk);
                layoutComponents(getSelectComps(HFEmph | HFFreq), subBounds, true, 0.75f);
                hfFreq.cornerRadius = hfFreq.getHeight() * 0.5f;
            }
            else
                c->setBounds(bounds.removeFromLeft(chunk));
            // if (auto *k = dynamic_cast<Knob *>(c))
            // {
            //     if (k == &comp)
            //     {
            //         k->setBounds(compSection.removeFromTop(bounds.getHeight() * 0.75f));
            //         compPos.setBounds(compSection.removeFromLeft(compSectW * 0.33f).reduced(compSectW * 0.05f, 0));
            //         compLink.setBounds(compSection.removeFromRight(compSectW * 0.33f).reduced(compSectW * 0.05f, 0));
            //         compPos.lnf.cornerRadius = compPos.getHeight() * 0.5f;
            //         compLink.lnf.cornerRadius = compLink.getHeight() * 0.5f;
            //     }
            //     else
            //         k->setBounds(bounds.removeFromLeft(chunk).reduced(chunk * 0.1f));
            // }
            // else if (c == &midSide)
            // {
            //     c->setBounds(bounds.removeFromLeft(chunk * 0.75f).reduced(chunk * 0.1f, bounds.getHeight() * 0.25f));
            //     midSide.lnf.cornerRadius = c->getHeight() * 0.5f;
            // }
        }

        // lfFreq.setBounds(lfEmph.getBounds().translated(0, lfEmph.getHeight() * 0.33f).reduced(0, lfEmph.getHeight() * 0.35f));
        // lfEmph.setBounds(lfEmph.getBounds().translated(0, -lfEmph.getHeight() * 0.25f));
        // lfFreq.cornerRadius = lfFreq.getHeight() * 0.5f;
        // hfFreq.setBounds(hfEmph.getBounds().translated(0, hfEmph.getHeight() * 0.33f).reduced(0, hfEmph.getHeight() * 0.35f));
        // hfEmph.setBounds(hfEmph.getBounds().translated(0, -hfEmph.getHeight() * 0.25f));
        // hfFreq.cornerRadius = hfFreq.getHeight() * 0.5f;
    }

private:
    AudioProcessorValueTreeState &vts;

    Knob::flags_t knobFlags = Knob::DRAW_GRADIENT | Knob::DRAW_SHADOW | Knob::DRAW_ARC;

    Knob emphasis{knobFlags}, doubler{knobFlags}, lfEmph{knobFlags}, hfEmph{knobFlags}, comp{knobFlags};
    SimpleSlider lfFreq, hfFreq;

    LightButton midSide, compLink, compPos;

    std::unique_ptr<AudioProcessorValueTreeState::SliderAttachment> emphasisAttach, doublerAttach, lfAttach, lfFreqAttach, hfAttach, hfFreqAttach, compAttach;
    std::unique_ptr<AudioProcessorValueTreeState::ButtonAttachment> msAttach, compLinkAttach, compPosAttach;

    strix::VolumeMeterComponent grMeter;

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

    std::vector<Component *> getComps() // returns top knobs & buttons
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

    std::vector<Component *> getSelectComps(uint16_t mask) // get a vector of select components using a bitmask
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