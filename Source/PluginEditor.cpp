#include "PluginEditor.h"
#include "PluginProcessor.h"

//==============================================================================
GammaAudioProcessorEditor::GammaAudioProcessorEditor(GammaAudioProcessor &p)
    : AudioProcessorEditor(&p), audioProcessor(p), ampControls(p.apvts),
      link(p.apvts), preComponent(p.getActiveGRSource(), p.apvts),
      cabComponent(p.apvts), reverbComp(p.apvts), enhancers(p.apvts),
      menu(p.apvts, p.isUnlocked), presetMenu(p.apvts), dl(DL_BIN),
      activation(p.trialRemaining_ms)
{
#if JUCE_WINDOWS || JUCE_LINUX
    opengl.setImageCacheSize((size_t)64 * 1024000);
    if (strix::readConfigFile(CONFIG_PATH, "openGL")) {
        opengl.detach();
        opengl.attachTo(*this);
    }
#endif

    if (strix::readConfigFile(CONFIG_PATH, "tooltips")) {
        tooltip = std::make_unique<TooltipWindow>(this, 1000);
    }

    logo = Drawable::createFromImageData(BinaryData::logo_svg,
                                         BinaryData::logo_svgSize);
    logo->setInterceptsMouseClicks(true, false);

    auto lastWidth = strix::readConfigFile(CONFIG_PATH, "size");
    if (lastWidth <= MIN_WIDTH)
        lastWidth = UI_WIDTH;
    setSize(lastWidth, lastWidth * 0.7625f);
    getConstrainer()->setFixedAspectRatio((double)getWidth() /
                                          (double)getHeight());

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
    menu.windowResizeCallback = [&] { resetWindowSize(); };
    menu.checkUpdateCallback = [&] {
        dlResult = strix::DownloadManager::checkForUpdate(
            ProjectInfo::projectName, ProjectInfo::versionString,
            SITE_URL "/versions/index.json", true,
            strix::readConfigFile(CONFIG_PATH, "beta_update"));
        p.checkedUpdate = true;
        strix::writeConfigFileString(CONFIG_PATH, "updateCheck",
                                     String(Time::currentTimeMillis()));
        if (!dlResult.updateAvailable)
            NativeMessageBox::showMessageBoxAsync(
                MessageBoxIconType::NoIcon, "Update", "No new updates", &menu);
        else {
            dl.changes = dlResult.changes;
            DBG("Changes: " << dl.changes);
            dl.setVisible(true);
        }
    };
    menu.showTooltipCallback = [&](bool state) {
        if (state)
            tooltip = std::make_unique<TooltipWindow>(this, 1000);
        else
            tooltip = nullptr;
        strix::writeConfigFile(CONFIG_PATH, "tooltips", state);
    };
    menu.activateCallback = [&] { activation.setVisible(true); };
#if JUCE_WINDOWS || JUCE_LINUX
    menu.openGLCallback = [&](bool state) {
        if (state)
            opengl.attachTo(*this);
        else
            opengl.detach();
        DBG("OpenGL: " << (int)opengl.isAttached()
                       << ", w/ cache size: " << opengl.getImageCacheSize());
        strix::writeConfigFile(CONFIG_PATH, "openGL", state);
    };
#endif

    for (auto *c : getTopComponents()) {
        addAndMakeVisible(*c);
        if (auto k = dynamic_cast<Knob *>(c))
            k->setColor(Colours::antiquewhite,
                        Colours::antiquewhite.withMultipliedLightness(1.5f));
    }

    addAndMakeVisible(presetMenu);
    presetMenu.setCurrentPreset(p.currentPreset);
    presetMenu.box.onChange = [&] {
        presetMenu.valueChanged();
        p.currentPreset = presetMenu.getCurrentPreset();
    };

    inGainAttach =
        std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
            p.apvts, "inputGain", inGain);
    inGain.setLabel("Input");
    inGain.setTooltip("Input gain before all processing, useful for increasing "
                      "or decreasing headroom before the amp.");
    inGain.setValueToStringFunction([](float val) {
        String str(val, 1);
        str += "dB";
        return str;
    });

    outGainAttach =
        std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
            p.apvts, "outputGain", outGain);
    outGain.setLabel("Output");
    outGain.setTooltip("Output gain after all processing");
    outGain.setValueToStringFunction([](float val) {
        String str(val, 1);
        str += "dB";
        return str;
    });

    // gateAttach =
    // std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(p.apvts,
    // "gate", gate); gate.setLabel("Gate"); gate.setTooltip("Simple noise gate
    // before the amp."); gate.setValueToStringFunction([](float val)
    //                               { if (val < -95.f) return String("Off");
    //                                 if (val >= -95.f) return String(val, 1);
    //                                 else return String(""); });

    widthAttach =
        std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
            p.apvts, "width", width);
    width.setLabel("Width");
    width.setTooltip("Standard stereo width control for widening the stereo "
                     "image, after all processing.");
    width.setValueToStringFunction([](float val) {
        String s(val * 100, 0);
        return s + "%";
    });

    mixAttach =
        std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(
            p.apvts, "mix", mix);
    mix.setLabel("Mix");
    mix.setTooltip("Global dry/wet control.");
    mix.setValueToStringFunction([](float val) {
        String s(val * 100, 0);
        return s + "%";
    });

    bypassAttach =
        std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(
            p.apvts, "bypass", bypass);
    bypass.setButtonText("Byp");
    bypass.setTooltip("Latency-compensated bypass of entire plugin");

    addAndMakeVisible(preComponent);
    addAndMakeVisible(ampControls);
    addAndMakeVisible(cabComponent);
    addAndMakeVisible(reverbComp);
    addAndMakeVisible(enhancers);

    setResizable(true, true);
    getConstrainer()->setMinimumWidth(MIN_WIDTH);
    getConstrainer()->setMaximumWidth(MAX_WIDTH);

    /* extra components (download, activation, splash, thread initialization) */
    addChildComponent(dl);
    dl.changes = dlResult.changes;
    dl.centreWithSize(300, 200);

    addChildComponent(activation);
    if (!p.checkUnlock())
        activation.setVisible(true);
    activation.onActivationCheck = [&](bool result) { p.isUnlocked = result; };
    activation.centreWithSize(300, 200);

    if (!p.checkedUpdate) {
        lThread = std::make_unique<strix::LiteThread>(1);
        lThread->addJob([&] {
            dlResult = strix::DownloadManager::checkForUpdate(
                ProjectInfo::projectName, ProjectInfo::versionString,
                SITE_URL
                "/versions/index.json", false,
                strix::readConfigFile(CONFIG_PATH, "beta_update"),
                strix::readConfigFileString(CONFIG_PATH, "updateCheck")
                    .getLargeIntValue());
            p.checkedUpdate = true;
            dl.changes = dlResult.changes;
            dl.shouldBeHidden = false;
            strix::writeConfigFileString(CONFIG_PATH, "updateCheck",
                                         String(Time::currentTimeMillis()));
        });
    }

    startTimerHz(1);

    addChildComponent(splash);
    splash.centreWithSize(250, 350);
    splash.currentWrapper = p.getWrapperTypeString();
    splash.onLogoClick = [&] {
        if (!splash.isVisible()) {
            splash.setImage(createComponentSnapshot(splash.getBounds()));
            splash.setVisible(true);
        }
    };

    preComponent.onResize = [&] {
        auto delta =
            (preComponent.minimized ? -95 : 95); // 95px = (800-610) / 2
        setSize(getWidth(), getHeight() + delta);
        getConstrainer()->setFixedAspectRatio((double)getWidth() /
                                              (double)getHeight());
    };
    enhancers.onResize = [&] {
        auto delta = (enhancers.minimized ? -95 : 95);
        setSize(getWidth(), getHeight() + delta);
        getConstrainer()->setFixedAspectRatio((double)getWidth() /
                                              (double)getHeight());
    };

    addMouseListener(this, true);
}

GammaAudioProcessorEditor::~GammaAudioProcessorEditor()
{
#if JUCE_WINDOWS || JUCE_LINUX
    opengl.detach();
#endif
    removeMouseListener(this);
    stopTimer();
}

void GammaAudioProcessorEditor::resetWindowSize()
{
    preComponent.toggleMinimized(true);
    enhancers.toggleMinimized(true);
    setSize(UI_WIDTH, 610);
    getConstrainer()->setFixedAspectRatio((double)UI_WIDTH / (double)610);
    strix::writeConfigFile(CONFIG_PATH, "size", UI_WIDTH);
}

//==============================================================================
void GammaAudioProcessorEditor::paint(juce::Graphics &g)
{
    g.fillAll(Colour(BACKGROUND_COLOR));
    auto trimmedTop =
        getLocalBounds().removeFromTop(uiScale * UI_WIDTH * 0.15f);
    logoBounds =
        trimmedTop.removeFromLeft(trimmedTop.getWidth() / 12).toFloat();
    logo->drawWithin(g, logoBounds.reduced(5.f), RectanglePlacement::centred,
                     1.f);
}

void GammaAudioProcessorEditor::resized()
{
    auto bounds = Rectangle(0, 0, UI_WIDTH, UI_WIDTH);
    uiScale = (float)getWidth() / (float)UI_WIDTH;
    bounds *= uiScale;
    const auto w = bounds.getWidth();
    const auto h = bounds.getHeight();

    auto topSection = bounds.removeFromTop(h * 0.15f);
    auto topSectionHeight = topSection.getHeight();
    topSection.removeFromLeft(w / 12); // section where logo is drawn
    float preSectMult = preComponent.minimized ? 0.06f : 0.17875f;
    auto preSection = bounds.removeFromTop((float)h * preSectMult);
    auto ampSection = bounds.removeFromTop(h * 0.255f);
    auto cabVerbSection = bounds.removeFromTop(h * 0.238f);
    float postSectMult = enhancers.minimized ? 0.06f : 0.17875f;
    auto enhancerSection = bounds.removeFromTop(h * postSectMult);

    /* flex box setup */
    FlexBox topControlsFlex, uiTopFlex;
    topControlsFlex.flexWrap = FlexBox::Wrap::wrap;
    topControlsFlex.justifyContent = FlexBox::JustifyContent::center;
    for (auto *c : getTopComponents()) {
        if (c == &link) {
            topControlsFlex.items.add(FlexItem(25 * uiScale, 50, *c));
            continue;
        } else if (auto *b = dynamic_cast<LightButton *>(c)) {
            topControlsFlex.items.add(
                FlexItem(50 * uiScale, 35, *c)
                    .withMargin(FlexItem::Margin(15.f * uiScale, 0,
                                                 15.f * uiScale, 0)));
            b->lnf.cornerRadius = 12.f;
            continue;
        } else if (auto *k = dynamic_cast<Knob *>(c)) {
            k->setTextOffset(0, -3);
            k->setOffset(0, -5);
        }
        auto flexComp = FlexItem(*c).withMinWidth(50 * uiScale);
        topControlsFlex.items.add(flexComp);
    }

    /* set bounds of top controls */
    uiTopFlex.justifyContent = FlexBox::JustifyContent::spaceAround;
    auto topSectionQtr = topSection.getWidth() / 4;
    float topKnobs = (w * 0.5f - topSectionQtr);
    float presetSection = topSectionQtr;
    float titleSection = topSection.getWidth() * 0.22f;
    uiTopFlex.items.add(FlexItem(titleSection, topSectionHeight, pluginTitle));
    uiTopFlex.items.add(FlexItem(topKnobs, topSectionHeight, topControlsFlex));
    uiTopFlex.items.add(
        FlexItem(presetSection, 0, presetMenu)
            .withMargin(FlexItem::Margin(topSectionHeight * 0.35f, 0,
                                         topSectionHeight * 0.35f, 0)));
    uiTopFlex.items.add(FlexItem(presetSection / 6.f, topSectionHeight, menu));

    uiTopFlex.performLayout(topSection);

    pluginTitle.setFont(
        Font(pluginTitle.getHeight() * 0.25f).withExtraKerningFactor(0.25f));

    /* rest of UI */
    ampControls.setBounds(ampSection);
    preComponent.setBounds(preSection);
    cabComponent.setBounds(cabVerbSection.removeFromLeft(w * 0.66f));
    reverbComp.setBounds(cabVerbSection);
    enhancers.setBounds(enhancerSection);

    dl.centreWithSize(300, 200);
    splash.centreWithSize(250, 350);
    activation.centreWithSize(300, 200);

    assert(w <= 1200 && w >= 600);

    strix::writeConfigFile(CONFIG_PATH, "size", w > MAX_WIDTH ? MAX_WIDTH : w);
}
