/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
GammaAudioProcessorEditor::GammaAudioProcessorEditor(GammaAudioProcessor &p)
    : AudioProcessorEditor(&p), audioProcessor(p), top(p.audioSource, p.apvts), ampControls(p.getActiveGRSource(), p.apvts), reverbComp(p.apvts), menu(p.apvts, 200), tooltip(this)
{
#if JUCE_WINDOWS || JUCE_LINUX
    opengl.setImageCacheSize((size_t)64 * 1024);
    if (readConfigFile("openGL"))
        opengl.attachTo(*this);
    DBG("init opengl: " << opengl.isAttached());
#endif

    logo = Drawable::createFromImageData(BinaryData::logo_svg, BinaryData::logo_svgSize);

    setSize(800, 700);

    auto bounds = getLocalBounds();
    auto third = bounds.getWidth() / 3;
    auto topSection = bounds.removeFromTop(60);
    auto bottomSection = bounds.removeFromBottom(200);
    auto ampSection = bounds.removeFromBottom(200).reduced(1);

    addAndMakeVisible(pluginTitle);
    pluginTitle.setBounds(topSection.removeFromRight(getWidth() * 0.66f).removeFromLeft(getWidth() / 3));
    String title = "GAMMA";
    Colour titleColor = Colours::beige;
#if !PRODUCTION_BUILD
    title.append("_DEV", 4);
    titleColor = Colours::red;
#endif
    pluginTitle.setText(title, NotificationType::dontSendNotification);
    pluginTitle.setFont(Font(getCustomFont()).withHeight(20.f).withExtraKerningFactor(0.5f));
    pluginTitle.setColour(Label::textColourId, titleColor);
    pluginTitle.setJustificationType(Justification::centred);

    addAndMakeVisible(menu);
    menu.setBounds(getWidth() - 205, 10, 200, 250);
    menu.menuClicked = [&] (bool state)
    {
        menu.setAlwaysOnTop(state);
        DBG("panel showing? " << state);
    };
    menu.windowResizeCallback = [&] { resetWindowSize(); };
#if JUCE_WINDOWS || JUCE_LINUX
    menu.openGLCallback = [&](bool state)
    {
        state ? opengl.attachTo(*this) : opengl.detach();
        DBG("OpenGL: " << opengl.isAttached());
        writeConfigFile("openGL", state);
    };
#endif

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

    addAndMakeVisible(top);
    top.setBounds(bounds);

    addAndMakeVisible(ampControls);
    ampControls.setBounds(ampSection);

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
    getConstrainer()->setMinimumSize(400, 350);
    getConstrainer()->setFixedAspectRatio(1.143);

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
    setSize(800, 700);
    writeConfigFile("size", 800);
}

//==============================================================================
void GammaAudioProcessorEditor::paint(juce::Graphics &g)
{
    g.fillAll(Colour(TOP_TRIM));
    
    auto top = getLocalBounds().withTrimmedBottom(getHeight() / 3);
    auto trimmedTop = top.removeFromTop(top.getHeight() / 10);

    logo->drawWithin(g, trimmedTop.removeFromLeft(trimmedTop.getWidth() / 12).reduced(5).toFloat(), RectanglePlacement::centred, 1.f);
}

void GammaAudioProcessorEditor::resized()
{
    auto children = getChildren();
    children.removeLast();
    auto scale = (float)getWidth() / 800.f;

    for (auto &c : children)
        c->setTransform(AffineTransform::scale(scale));

    MessageManager::callAsync([&]{writeConfigFile("size", getWidth());});
}