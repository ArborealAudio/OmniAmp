/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
GammaAudioProcessorEditor::GammaAudioProcessorEditor(GammaAudioProcessor &p)
    : AudioProcessorEditor(&p), audioProcessor(p), ampControls(p.getActiveGRSource(), p.apvts), wave(p.audioSource), reverbComp(p.apvts),
    menu(p.apvts, 200), tooltip(this)
{
#if JUCE_WINDOWS || JUCE_LINUX
    opengl.setImageCacheSize((size_t)64 * 1024);
    if (readConfigFile("openGL"))
        opengl.attachTo(*this);
    DBG("init opengl: " << opengl.isAttached());
#endif

    mesh = Drawable::createFromImageData(BinaryData::amp_mesh_2_svg, BinaryData::amp_mesh_2_svgSize);
    logo = Drawable::createFromImageData(BinaryData::logo_svg, BinaryData::logo_svgSize);

    setSize(800, 450);

    auto bounds = getLocalBounds();
    auto topSection = bounds.removeFromTop(55);
    auto qtr = bounds.getWidth() / 4;

    auto ampSection = bounds.removeFromBottom(200).reduced(10, 2);
    auto topLeftQtr = bounds.removeFromLeft(qtr);
    auto topRightQtr = bounds.removeFromRight(qtr);

    addAndMakeVisible(pluginTitle);
    pluginTitle.setBounds(topSection.removeFromRight(getWidth() * 0.66f).removeFromLeft(getWidth() / 3));
    pluginTitle.setText("GAMMA", NotificationType::dontSendNotification);
    pluginTitle.setFont(Font(getCustomFont()).withHeight(20.f).withExtraKerningFactor(0.5f));
    pluginTitle.setColour(Label::textColourId, Colours::beige);
    pluginTitle.setJustificationType(Justification::centred);

    addAndMakeVisible(menu);
    menu.setBounds(getWidth() - 205, 10, 200, 250);
    menu.menuClicked = [&] (bool state)
    {
        menu.setAlwaysOnTop(state);
        DBG("panel showing? " << state);
    };
    menu.windowResizeCallback = [&] { resetWindowSize(); };
    menu.openGLCallback = [&](bool state)
    {
        state ? opengl.attachTo(*this) : opengl.detach();
        DBG("OpenGL: " << opengl.isAttached());
        writeConfigFile("openGL", state);
    };

    topSection.removeFromLeft(getWidth() / 12);
    topSection.translate(0, -5);
    auto topSectionThird = topSection.getWidth() / 3;

    addAndMakeVisible(inGain);
    inGainAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "inputGain", inGain);
    inGain.setBounds(topSection.removeFromLeft(topSectionThird).reduced(5, 0));
    inGain.setLabel("Input");
    inGain.setValueToStringFunction([](float val)
                                     { auto str = String(val, 1); str.append("dB", 2); return str; });

    addAndMakeVisible(outGain);
    outGainAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "outputGain", outGain);
    outGain.setBounds(topSection.removeFromLeft(topSectionThird).reduced(5, 0));
    outGain.setLabel("Output");
    outGain.setValueToStringFunction([](float val)
                                     { auto str = String(val, 1); str.append("dB", 2); return str; });

    addAndMakeVisible(gate);
    gateAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "gate", gate);
    gate.setBounds(topSection.reduced(5, 0));
    gate.setLabel("Gate");
    gate.setValueToStringFunction([](float val)
                                  { if (val < -95.f) return String("Off");
                                    if (val >= -95.f) return String(val, 1); });

    addAndMakeVisible(wave);
    wave.setBounds(bounds.reduced(10).translated(0, -5));
    wave.setInterceptsMouseClicks(false, false);

    blur = std::make_unique<Image>(Image::PixelFormat::ARGB, wave.getWidth(), wave.getHeight(), true);

    addAndMakeVisible(ampControls);
    ampControls.setBounds(ampSection);

    addAndMakeVisible(lfEnhance);
    lfAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "lfEnhance", lfEnhance);
    lfEnhance.setBounds(topLeftQtr);
    lfEnhance.autoGain.store(*p.apvts.getRawParameterValue("lfEnhanceAuto"));
    lfEnhance.onAltClick = [&](bool state)
    {
        p.apvts.getParameterAsValue("lfEnhanceAuto") = state;
    };

    addAndMakeVisible(hfEnhance);
    hfAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "hfEnhance", hfEnhance);
    hfEnhance.setBounds(topRightQtr);
    hfEnhance.autoGain.store(*p.apvts.getRawParameterValue("hfEnhanceAuto"));
    hfEnhance.onAltClick = [&](bool state)
    {
        p.apvts.getParameterAsValue("hfEnhanceAuto") = state;
    };

    setSize(800, 650);

    auto bottomSection = getLocalBounds().removeFromBottom(200);

    cabComponent.setBounds(bottomSection.removeFromLeft(getWidth() * 0.66f));
    addAndMakeVisible(cabComponent);

    cabComponent.setState(*p.apvts.getRawParameterValue("cabOn") + *p.apvts.getRawParameterValue("cabType"));

    cabComponent.cabChanged = [&](bool state, int newType)
    {
        p.apvts.getParameterAsValue("cabType") = newType;
        p.apvts.getParameterAsValue("cabOn") = state;
    };

    reverbComp.setBounds(bottomSection);

    addAndMakeVisible(reverbComp);

    setResizable(true, true);
    getConstrainer()->setMinimumSize(400, 325);
    getConstrainer()->setFixedAspectRatio(1.231);

    // auto lastWidth = readConfigFile("size");

    // setSize(lastWidth, (float)lastWidth / 1.231f);
}

GammaAudioProcessorEditor::~GammaAudioProcessorEditor()
{
#if JUCE_WINDOWS || JUCE_LINUX
    opengl.detach();
#endif
}

void GammaAudioProcessorEditor::resetWindowSize() noexcept
{
    setSize(800, 650);
    writeConfigFile("size", 800);
}

//==============================================================================
void GammaAudioProcessorEditor::paint(juce::Graphics &g)
{
    g.fillAll(Colour(TOP_TRIM));
    g.setColour(Colour(BACKGROUND_COLOR));
    auto top = getLocalBounds().withTrimmedBottom(getHeight() / 3);
    auto trimmedTop = top.removeFromTop(top.getHeight() / 10);

    logo->drawWithin(g, trimmedTop.removeFromLeft(trimmedTop.getWidth() / 12).reduced(5).toFloat(), RectanglePlacement::centred, 1.f);

    auto topsection = top.withTrimmedBottom(top.getHeight() / 2).reduced(10, 0).translated(0, 10).toFloat();

    g.fillRoundedRectangle(topsection, 5.f);
    g.setColour(Colours::grey);
    g.drawRoundedRectangle(topsection, 5.f, 2.f);

    g.reduceClipRegion(topsection.toNearestInt());
    mesh->drawWithin(g, topsection, RectanglePlacement::fillDestination, 1.f);

    if (blur != nullptr)
        g.drawImage(*blur, wave.getBoundsInParent().toFloat(), RectanglePlacement::doNotResize);

    g.setColour(Colour(DEEP_BLUE));
    g.drawRoundedRectangle(wave.getBoundsInParent().toFloat(), 5.f, 2.f);
}

void GammaAudioProcessorEditor::resized()
{
    auto children = getChildren();
    children.removeLast();
    auto scale = (float)getWidth() / 800.f;

    for (auto &c : children)
        c->setTransform(AffineTransform::scale(scale));

    if (blur == nullptr)
        return;
    blur->clear(blur->getBounds());
    wave.setVisible(false);
    blur = std::make_unique<Image>(createComponentSnapshot(wave.getBoundsInParent()));
    wave.setVisible(true);

    gin::applyContrast(*blur, -35);
    gin::applyStackBlur(*blur, 10);

    writeConfigFile("size", getWidth());
}