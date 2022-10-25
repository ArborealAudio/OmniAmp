/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
GammaAudioProcessorEditor::GammaAudioProcessorEditor(GammaAudioProcessor &p)
    : AudioProcessorEditor(&p), audioProcessor(p), top(p.audioSource, p.apvts), ampControls(p.getActiveGRSource(), p.apvts), cabComponent(p.apvts.getRawParameterValue("cabType")), reverbComp(p.apvts), menu(p.apvts, 200), presetMenu(p.apvts), dl(false), tooltip(this)
{
#if JUCE_WINDOWS || JUCE_LINUX
    opengl.setImageCacheSize((size_t)128 * 1024000);
    if (readConfigFile("openGL")) {
        opengl.detach();
        opengl.attachTo(*this);
        DBG("init opengl: " << 1 << ", w/ cache size: " << opengl.getImageCacheSize());
    }
    else
    {
        DBG("init openGL: " << 0);
    }
#endif

    logo = Drawable::createFromImageData(BinaryData::logo_svg, BinaryData::logo_svgSize);

    auto lastWidth = readConfigFile("size");
    setSize(lastWidth, (float)lastWidth / 1.143f);

    addAndMakeVisible(pluginTitle);
    String title = "GAMMA";
    Colour titleColor = Colours::beige;
#if !PRODUCTION_BUILD
    title.append("_DEV", 4);
    titleColor = Colours::red;
#endif
    pluginTitle.setText(title, NotificationType::dontSendNotification);
    pluginTitle.setFont(Font(getCustomFont()).withHeight(25.f).withExtraKerningFactor(0.5f));
    pluginTitle.setColour(Label::textColourId, titleColor);
    pluginTitle.setJustificationType(Justification::centred);

    addAndMakeVisible(menu);
    menu.windowResizeCallback = [&] { resetWindowSize(); };
    menu.checkUpdateCallback = [&]
    { dl.setVisible(dl.checkForUpdate()); };
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

    addAndMakeVisible(presetMenu);

    addAndMakeVisible(inGain);
    inGainAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "inputGain", inGain);
    inGain.setLabel("Input");
    inGain.setValueToStringFunction([](float val)
                                     { auto str = String(val, 1); str.append("dB", 2); return str; });

    addAndMakeVisible(outGain);
    outGainAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "outputGain", outGain);
    outGain.setLabel("Output");
    outGain.setValueToStringFunction([](float val)
                                     { auto str = String(val, 1); str.append("dB", 2); return str; });

    addAndMakeVisible(gate);
    gateAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "gate", gate);
    gate.setLabel("Gate");
    gate.setValueToStringFunction([](float val)
                                  { if (val < -95.f) return String("Off");
                                    if (val >= -95.f) return String(val, 1); });

    addAndMakeVisible(top);

    addAndMakeVisible(ampControls);

    addAndMakeVisible(cabComponent);
    cabComponent.setState(*p.apvts.getRawParameterValue("cabType"));
    cabComponent.cabChanged = [&](int newType)
    {
        p.apvts.getParameterAsValue("cabType") = newType;
    };

    addAndMakeVisible(reverbComp);

    setResizable(true, true);
    getConstrainer()->setMinimumSize(550, 481);
    getConstrainer()->setFixedAspectRatio(1.143);

    addChildComponent(dl);
    dl.centreWithSize(300, 200);
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
    auto trimmedTop = top.removeFromTop(getHeight() * 0.085f);

    logo->drawWithin(g, trimmedTop.removeFromLeft(trimmedTop.getWidth() / 12).reduced(5).toFloat(), RectanglePlacement::centred, 1.f);
}

void GammaAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    const auto w = getWidth();
    const auto h = getHeight();
    auto topSection = bounds.removeFromTop(h * 0.085f);
    auto bottomSection = bounds.removeFromBottom(h * 0.285f);
    auto ampSection = bounds.removeFromBottom(h * 0.285f).reduced(1);

    topSection.removeFromLeft(w / 12); // section where logo is drawn
    auto topSectionThird = topSection.getWidth() / 3;
    auto topKnobs = topSection.removeFromLeft(topSectionThird);
    auto titleSection = topSection.removeFromLeft(topSectionThird);
    auto presetSection = topSection.removeFromLeft(topSection.getWidth() * 0.66);

    pluginTitle.setBounds(titleSection);
    presetMenu.setBounds(presetSection.reduced(0, topSection.getHeight() * 0.25));
    menu.setBounds(w - (w * 0.25f), h * 0.03f, w * 0.25f, h / 2.5f);

    auto knobThird = topKnobs.getWidth() / 3;
    inGain.setBounds(topKnobs.removeFromLeft(knobThird).reduced(5, 0));
    outGain.setBounds(topKnobs.removeFromLeft(knobThird).reduced(5, 0));
    gate.setBounds(topKnobs.reduced(5, 0));

    top.setBounds(bounds);
    ampControls.setBounds(ampSection);
    cabComponent.setBounds(bottomSection.removeFromLeft(w * 0.66f));
    reverbComp.setBounds(bottomSection);

    dl.centreWithSize(300, 200);

    MessageManager::callAsync([&]{writeConfigFile("size", getWidth());});
}