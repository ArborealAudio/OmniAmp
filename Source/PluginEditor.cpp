#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
GammaAudioProcessorEditor::GammaAudioProcessorEditor(GammaAudioProcessor &p)
    : AudioProcessorEditor(&p),
      audioProcessor(p),
      ampControls(p.apvts),
      link(p.apvts),
      preComponent(p.getActiveGRSource(), p.apvts),
      cabComponent(p.apvts),
      reverbComp(p.apvts),
      enhancers(p.apvts),
      menu(p.apvts, p.isUnlocked),
      presetMenu(p.apvts),
      dl(SITE_URL
         "/downloads/"
         DL_BIN,
         "~/Downloads/"
         DL_BIN),
      activation(p.trialRemaining_ms)
{
#if JUCE_WINDOWS || JUCE_LINUX
    opengl.setImageCacheSize((size_t)64 * 1024000);
    if (strix::readConfigFile(CONFIG_PATH, "openGL"))
    {
        opengl.detach();
        opengl.attachTo(*this);
    }
#endif

    if (strix::readConfigFile(CONFIG_PATH, "tooltips"))
    {
        tooltip = std::make_unique<TooltipWindow>(this, 1000);
    }

    logo = Drawable::createFromImageData(BinaryData::logo_svg, BinaryData::logo_svgSize);
    logo->setInterceptsMouseClicks(true, false);

    auto lastWidth = strix::readConfigFile(CONFIG_PATH, "size");
    if (lastWidth <= 600)
        lastWidth = 800;
    setSize(lastWidth, lastWidth * 0.7625f);

    addAndMakeVisible(pluginTitle);
    String title = String(ProjectInfo::projectName).toUpperCase();
    Colour titleColor = Colours::antiquewhite;
#if !PRODUCTION_BUILD && DEV_BUILD
    title.append("\nDEV", 5);
    titleColor = Colours::red;
#elif BETA_BUILD
    title.append("\nbeta", 5);
#endif
    pluginTitle.setText(title, NotificationType::dontSendNotification);
    pluginTitle.setColour(Label::textColourId, titleColor);
    pluginTitle.setJustificationType(Justification::centred);
    pluginTitle.setInterceptsMouseClicks(false, false);

    addAndMakeVisible(menu);
    menu.windowResizeCallback = [&]
    { resetWindowSize(); };
    menu.checkUpdateCallback = [&]
    {
        dlResult = strix::DownloadManager::checkForUpdate(ProjectInfo::versionString,
                                                          SITE_URL
                                                          "/versions/"
#if !PRODUCTION_BUILD
                                                          "test/"
#endif
                                                          "Gamma-latest.json",
                                                          true);
        p.checkedUpdate = true;
        strix::writeConfigFileString(CONFIG_PATH, "updateCheck", String(Time::currentTimeMillis()));
        if (!dlResult.updateAvailable)
            NativeMessageBox::showMessageBoxAsync(MessageBoxIconType::NoIcon, "Update", "No new updates", &menu);
        else
        {
            dl.changes = dlResult.changes;
            DBG("Changes: " << dl.changes);
            dl.setVisible(true);
        }
    };
    menu.showTooltipCallback = [&](bool state)
    {
        if (state)
            tooltip = std::make_unique<TooltipWindow>(this, 1000);
        else
            tooltip = nullptr;
        strix::writeConfigFile(CONFIG_PATH, "tooltips", state);
    };
    menu.activateCallback = [&]
    {
        activation.setVisible(true);
    };
#if JUCE_WINDOWS || JUCE_LINUX
    menu.openGLCallback = [&](bool state)
    {
        if (state)
            opengl.attachTo(*this);
        else
            opengl.detach();
        DBG("OpenGL: " << (int)opengl.isAttached() << ", w/ cache size: " << opengl.getImageCacheSize());
        strix::writeConfigFile(CONFIG_PATH, "openGL", state);
    };
#endif

    for (auto *c : getTopComponents())
    {
        addAndMakeVisible(*c);
        if (auto k = dynamic_cast<Knob *>(c))
            k->setColor(Colours::antiquewhite, Colours::antiquewhite.withMultipliedLightness(1.5f));
    }

    addAndMakeVisible(presetMenu);
    presetMenu.setCurrentPreset(p.currentPreset);
    presetMenu.box.onChange = [&]
    {
        presetMenu.valueChanged();
        p.currentPreset = presetMenu.getCurrentPreset();
    };

    inGainAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "inputGain", inGain);
    inGain.setLabel("Input");
    inGain.setTooltip("Input gain before all processing, useful for increasing or decreasing headroom before the amp.");
    inGain.setValueToStringFunction([](float val)
                                    { String str(val, 1); str += "dB"; return str; });

    outGainAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "outputGain", outGain);
    outGain.setLabel("Output");
    outGain.setTooltip("Output gain after all processing");
    outGain.setValueToStringFunction([](float val)
                                     { String str(val, 1); str += "dB"; return str; });

    gateAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "gate", gate);
    gate.setLabel("Gate");
    gate.setTooltip("Simple noise gate before the amp.");
    gate.setValueToStringFunction([](float val)
                                  { if (val < -95.f) return String("Off");
                                    if (val >= -95.f) return String(val, 1); else return String(""); });

    widthAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "width", width);
    width.setLabel("Width");
    width.setTooltip("Standard stereo width control for widening the stereo image, after all processing.");
    width.setValueToStringFunction([](float val)
                                   { String s(val * 100, 0); return s + "%"; });

    mixAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "mix", mix);
    mix.setLabel("Mix");
    mix.setTooltip("Global dry/wet control.");
    mix.setValueToStringFunction([](float val)
                                 { String s(val * 100, 0); return s + "%"; });

    bypassAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(p.apvts, "bypass", bypass);
    bypass.setButtonText("Byp");
    bypass.setTooltip("Full, latency-compensated bypass");

    addAndMakeVisible(preComponent);
    addAndMakeVisible(ampControls);
    addAndMakeVisible(cabComponent);
    addAndMakeVisible(reverbComp);
    addAndMakeVisible(enhancers);

    setResizable(true, true);
    getConstrainer()->setMinimumSize(600, 600);
    getConstrainer()->setMaximumSize(1600, 1600);

    addChildComponent(dl);
    dl.changes = dlResult.changes;
    dl.centreWithSize(300, 200);

    addChildComponent(activation);
    if (!p.checkUnlock())
        activation.setVisible(true);
    activation.onActivationCheck = [&](bool result)
    {
        p.isUnlocked = result;
    };
    activation.centreWithSize(300, 200);

    if (!p.checkedUpdate)
    {
        lThread = std::make_unique<strix::LiteThread>(1);
        if (!p.checkedUpdate)
            lThread->addJob([&]
                            { dlResult = strix::DownloadManager::checkForUpdate(ProjectInfo::versionString, SITE_URL
        "/versions/"
#if !PRODUCTION_BUILD
        "test/"
#endif
        "Gamma-latest.json", false, strix::readConfigFileString(CONFIG_PATH, "updateCheck").getLargeIntValue());
                            p.checkedUpdate = true;
                            dl.changes = dlResult.changes;
                            DBG("Changes: " << dl.changes);
                            strix::writeConfigFileString(CONFIG_PATH, "updateCheck", String(Time::currentTimeMillis()));
                            MessageManager::callAsync([&]
                                                      { dl.setVisible(dlResult.updateAvailable); }); });
    }

    addChildComponent(splash);
    splash.centreWithSize(250, 350);
    splash.currentWrapper = p.getWrapperTypeString();
    splash.onLogoClick = [&]
    {
        if (!splash.isVisible())
        {
            splash.setImage(createComponentSnapshot(splash.getBounds()));
            splash.setVisible(true);
        }
    };

    preComponent.onResize = [&]
    {
        auto delta = (preComponent.minimized ? -95 : 95);
        setSize(getWidth(), getHeight() + delta);
    };
    enhancers.onResize = [&]
    {
        auto delta = (enhancers.minimized ? -95 : 95);
        setSize(getWidth(), getHeight() + delta);
    };

    addMouseListener(this, true);
}

GammaAudioProcessorEditor::~GammaAudioProcessorEditor()
{
#if JUCE_WINDOWS || JUCE_LINUX
    opengl.detach();
#endif
    removeMouseListener(this);
}

void GammaAudioProcessorEditor::resetWindowSize()
{
    preComponent.toggleMinimized(true);
    enhancers.toggleMinimized(true);
    setSize(800, 610);
    strix::writeConfigFile(CONFIG_PATH, "size", 800);
}

//==============================================================================
void GammaAudioProcessorEditor::paint(juce::Graphics &g)
{
    g.fillAll(Colour(BACKGROUND_COLOR));
    auto trimmedTop = getLocalBounds().removeFromTop(uiScale * 800 * 0.15f);
    logoBounds = trimmedTop.removeFromLeft(trimmedTop.getWidth() / 12).toFloat();
    logo->drawWithin(g, logoBounds.reduced(5.f), RectanglePlacement::centred, 1.f);
}

void GammaAudioProcessorEditor::resized()
{
    auto bounds = Rectangle(0, 0, 800, 800);
    uiScale = (float)getWidth() / (float)800;
    bounds *= uiScale;
    const auto w = bounds.getWidth();
    const auto h = bounds.getHeight();

    /**
     * HEIGHTS: express as absolute pixel amt added or subtracted from height
     * total height when both minimized = 610
     * total height when both maximized = 800
     * max delta: 190
     * lets say they both end up at the same size maximized:
     * each a delta of 95px / 11.875% of 800
     * Their bounds will be determined by either adding or subtracting 11.875%?
     */

    auto topSection = bounds.removeFromTop(h * 0.15f);
    // auto mainHeight = bounds.getHeight(); // height after top trimmed off
    float preSectMult = preComponent.minimized ? 0.06f : 0.17875f;
    auto preSection = bounds.removeFromTop((float)h * preSectMult);
    auto ampSection = bounds.removeFromTop(h * 0.255f);
    auto cabVerbSection = bounds.removeFromTop(h * 0.238f);
    float postSectMult = enhancers.minimized ? 0.06f : 0.17875f;
    auto enhancerSection = bounds.removeFromTop(h * postSectMult);

    /* set bounds of top controls */
    auto topSectionQtr = topSection.getWidth() / 4;
    auto topKnobs = topSection.removeFromLeft(w * 0.5f - topSectionQtr * 0.5);
    topKnobs.removeFromLeft(w / 12); // section where logo is drawn
    auto presetSection = topSection.removeFromRight(w * 0.5f - topSectionQtr * 0.5).withTrimmedRight(topSectionQtr * 0.5);
    auto titleSection = topSection;

    pluginTitle.setFont(Font(getCustomFont()).withHeight(h * 0.03f).withExtraKerningFactor(0.5f));
    pluginTitle.setBounds(titleSection);
    presetMenu.setBounds(presetSection.reduced(0, topSection.getHeight() * 0.35f));
    menu.setBounds(w - (w / 8.f), presetMenu.getY(), w / 14.f, w / 38.f);

    auto knobFrac = topKnobs.getWidth() / 4;
    auto knobHalf = topKnobs.getHeight() / 2;
    auto knobsBottom = topKnobs.removeFromBottom(knobHalf);
    auto topComponents = getTopComponents();
    for (size_t i = 0; i < topComponents.size(); ++i)
    {
        auto c = topComponents[i];
        if (c == &link)
        {
            c->setBounds(topKnobs.removeFromLeft(knobFrac * 0.4f));
            continue;
        }
        if (i < 4)
            c->setBounds(topKnobs.removeFromLeft(knobFrac));
        else
        {
            if (c == &bypass)
            {
                c->setBounds(knobsBottom.removeFromLeft(knobFrac).reduced(0, knobFrac * 0.2f));
                auto *b = static_cast<LightButton *>(c);
                b->lnf.cornerRadius = b->getHeight() * 0.25f;
                continue;
            }
            c->setBounds(knobsBottom.removeFromLeft(knobFrac));
        }

        if (auto *k = static_cast<Knob *>(c))
        {
            k->setTextOffset(0, -3);
            k->setOffset(0, -c->getHeight() * 0.1f);
        }
    }

    /* rest of UI */
    ampControls.setBounds(ampSection);
    preComponent.setBounds(preSection);
    cabComponent.setBounds(cabVerbSection.removeFromLeft(w * 0.66f));
    reverbComp.setBounds(cabVerbSection);
    enhancers.setBounds(enhancerSection);

    dl.centreWithSize(300, 200);
    splash.centreWithSize(250, 350);
    activation.centreWithSize(300, 200);

    MessageManager::callAsync([&]
                              { strix::writeConfigFile(CONFIG_PATH, "size", w); });
}