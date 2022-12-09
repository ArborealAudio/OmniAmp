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
      preComponent(p.getActiveGRSource(), p.apvts),
      ampControls(p.apvts),
      cabComponent(p.apvts.getRawParameterValue("cabType")),
      reverbComp(p.apvts),
      enhancers(p.audioSource, p.apvts),
      menu(p.apvts, 200),
      presetMenu(p.apvts),
      dl(false), tooltip(this, 1000)
{
#if JUCE_WINDOWS || JUCE_LINUX
    opengl.setImageCacheSize((size_t)64 * 1024000);
    if (readConfigFile("openGL"))
    {
        opengl.detach();
        opengl.attachTo(*this);
    }
#endif

    logo = Drawable::createFromImageData(BinaryData::logo_svg, BinaryData::logo_svgSize);
    logo->setInterceptsMouseClicks(true, false);

    auto lastWidth = readConfigFile("size");
    if (lastWidth <= 0)
        lastWidth = 800;
    setSize(lastWidth, lastWidth);

    addAndMakeVisible(pluginTitle);
    String title = "GAMMA";
    Colour titleColor = Colours::antiquewhite;
#if !PRODUCTION_BUILD
    title.append("\nDEV", 5);
    titleColor = Colours::red;
#elif BETA_BUILD
    title.append("\nbeta", 5);
#endif
    pluginTitle.setText(title, NotificationType::dontSendNotification);
    pluginTitle.setFont(Font(getCustomFont()).withHeight(25.f).withExtraKerningFactor(0.5f));
    pluginTitle.setColour(Label::textColourId, titleColor);
    pluginTitle.setJustificationType(Justification::centred);

    addAndMakeVisible(menu);
    menu.windowResizeCallback = [&]
    { resetWindowSize(); };
    menu.checkUpdateCallback = [&]
    {
        dl.setVisible(dl.checkForUpdate());
        if (!dl.isVisible())
            NativeMessageBox::showMessageBoxAsync(MessageBoxIconType::NoIcon, "Update", "No new updates", &menu);
    };
#if JUCE_WINDOWS || JUCE_LINUX
    menu.openGLCallback = [&](bool state)
    {
        if (state)
            opengl.attachTo(*this);
        else
            opengl.detach();
        DBG("OpenGL: " << (int)opengl.isAttached() << ", w/ cache size: " << opengl.getImageCacheSize());
        writeConfigFile("openGL", state);
    };
#endif

    for (auto c : getTopComponents())
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

    linkAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(p.apvts, "gainLink", link);
    link.setButtonText("Link");
    link.setTooltip("Link Input and Output gains. Output gain can still be adjusted with linking on.");

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

    addAndMakeVisible(preComponent);
    addAndMakeVisible(ampControls);
    addAndMakeVisible(cabComponent);
    cabComponent.setState(*p.apvts.getRawParameterValue("cabType"));
    cabComponent.cabChanged = [&](int newType)
    {
        p.apvts.getParameterAsValue("cabType") = newType;
    };
    addAndMakeVisible(reverbComp);
    addAndMakeVisible(enhancers);

    setResizable(true, true);
    getConstrainer()->setMinimumSize(550, 550);
    getConstrainer()->setFixedAspectRatio(1.0);

    addChildComponent(dl);
    dl.centreWithSize(300, 200);

    addChildComponent(activation);
    activation.onActivationCheck = [&](bool result)
    {
        activation.setVisible(!result);
        p.lockProcessing(!result);
    };
    activation.onSiteCheck = [&](bool result)
    {
        activation.m_betaLive = result;
        activation.setVisible(!result);
        activation.editor.setVisible(result);
        activation.submit.setVisible(result);
        activation.repaint();
        if (result)
            activation.readFile();
        p.lockProcessing(!result);
    };
    activation.centreWithSize(300, 200);

    addChildComponent(splash);
    splash.centreWithSize(250, 350);
    splash.setPluginWrapperType(p.getWrapperTypeString());
    splash.onLogoClick = [&]
    {
        if (!splash.isVisible())
        {
            splash.setImage(createComponentSnapshot(splash.getBounds()));
            splash.setVisible(true);
        }
    };
}

GammaAudioProcessorEditor::~GammaAudioProcessorEditor()
{
#if JUCE_WINDOWS || JUCE_LINUX
    opengl.detach();
#endif
}

void GammaAudioProcessorEditor::resetWindowSize() noexcept
{
    setSize(800, 800);
    writeConfigFile("size", 800);
}

//==============================================================================
void GammaAudioProcessorEditor::paint(juce::Graphics &g)
{
    g.fillAll(Colour(TOP_TRIM));
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
    auto enhancerSection = bounds.removeFromBottom(mainHeight * 0.25f).reduced(1);
    auto cabVerbSection = bounds.removeFromBottom(mainHeight * 0.25f).reduced(1);
    auto ampSection = bounds.removeFromBottom(bounds.getHeight() * 0.66f).reduced(1);
    auto preSection = bounds.reduced(1);

    /* set bounds of top controls */
    auto topSectionQtr = topSection.getWidth() / 4;
    auto topKnobs = topSection.removeFromLeft(w * 0.5f - topSectionQtr * 0.5);
    topKnobs.removeFromLeft(w / 12); // section where logo is drawn
    auto presetSection = topSection.removeFromRight(w * 0.5f - topSectionQtr * 0.5).withTrimmedRight(topSectionQtr * 0.5);
    auto titleSection = topSection;

    pluginTitle.setBounds(titleSection);
    presetMenu.setBounds(presetSection.reduced(0, topSection.getHeight() * 0.35f));
    menu.setBounds(w - (w * 0.25f), presetMenu.getY(), w * 0.25f, h / 3.f); /*...sickening*/

    auto knobFrac = topKnobs.getWidth() / 4;
    auto knobHalf = topKnobs.getHeight() / 2;
    auto knobsBottom = topKnobs.removeFromBottom(knobHalf);
    auto topComponents = getTopComponents();
    for (size_t i = 0; i < topComponents.size(); ++i)
    {
        auto c = topComponents[i];
        if (i < 4)
            c->setBounds(topKnobs.removeFromLeft(knobFrac));
        else
            c->setBounds(knobsBottom.removeFromLeft(knobFrac));
        if (auto *k = dynamic_cast<Knob*>(c))
            k->setOffset(0, -c->getHeight() * 0.1f);
    }

    /* rest of UI */
    ampControls.setBounds(ampSection);
    preComponent.setBounds(preSection);
    cabComponent.setBounds(cabVerbSection.removeFromLeft(w * 0.66f).reduced(2));
    reverbComp.setBounds(cabVerbSection.reduced(2));
    enhancers.setBounds(enhancerSection);

    dl.centreWithSize(300, 200);
    splash.centreWithSize(250, 350);
    activation.centreWithSize(300, 200);

    MessageManager::callAsync([&]
                              { writeConfigFile("size", getWidth()); });
}