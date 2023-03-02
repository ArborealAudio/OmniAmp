/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
GammaAudioProcessorEditor::GammaAudioProcessorEditor(GammaAudioProcessor &p)
    : AudioProcessorEditor(&p), audioProcessor(p),
      ampControls(p.apvts),
      link(p.apvts),
      preComponent(p.getActiveGRSource(), p.apvts),
      cabComponent(p.apvts),
      reverbComp(p.apvts),
      enhancers(p.audioSource, p.apvts),
      menu(p.apvts),
      presetMenu(p.apvts),
      dl(SITE_URL
        "/downloads/"
        DL_BIN,
        "~/Downloads/"
        DL_BIN
      )
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
    if (lastWidth <= 0)
        lastWidth = 800;
    setSize(lastWidth, lastWidth);

    addAndMakeVisible(pluginTitle);
    String title = "GAMMA";
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
    getConstrainer()->setMinimumSize(550, 550);
    getConstrainer()->setMaximumSize(1600, 1600);
    getConstrainer()->setFixedAspectRatio(1.0);

    addChildComponent(dl);
    dl.changes = dlResult.changes;
    dl.centreWithSize(300, 200);

    addChildComponent(activation);
    activation.onActivationCheck = [&](bool result)
    {
        activation.setVisible(!result);
        p.lockProcessing(!result);
    };
    activation.centreWithSize(300, 200);

    if (!p.checkedUpdate || !p.checkedActivation)
    {
        lThread = std::make_unique<strix::LiteThread>(2);
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
                            // strix::writeConfigFileString(CONFIG_PATH, "updateCheck", String(Time::currentTimeMillis()));
                            MessageManager::callAsync([&]
                                                      { dl.setVisible(dlResult.updateAvailable); });
                            });
        if (!p.checkedActivation)
            lThread->addJob([&]
                            { activation.checkSite(); p.checkedActivation = true; });
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
    setSize(800, 800);
    strix::writeConfigFile(CONFIG_PATH, "size", 800);
}

//==============================================================================
void GammaAudioProcessorEditor::paint(juce::Graphics &g)
{
    DBG("DL visible: " << (int)dl.isVisible());
    g.fillAll(Colour(BACKGROUND_COLOR));
    auto trimmedTop = getLocalBounds().removeFromTop(getHeight() * 0.15f);
    logoBounds = trimmedTop.removeFromLeft(trimmedTop.getWidth() / 12).toFloat();
    logo->drawWithin(g, logoBounds.reduced(5.f), RectanglePlacement::centred, 1.f);
}

void GammaAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto w = getWidth();
    auto h = getHeight();
    auto topSection = bounds.removeFromTop(h * 0.15f);
    auto mainHeight = bounds.getHeight();
    auto enhancerSection = bounds.removeFromBottom(mainHeight * 0.25f);
    auto cabVerbSection = bounds.removeFromBottom(mainHeight * 0.3f);
    auto ampSection = bounds.removeFromBottom(bounds.getHeight() * 0.65f);
    auto preSection = bounds;

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
                              { strix::writeConfigFile(CONFIG_PATH, "size", getWidth()); });
}