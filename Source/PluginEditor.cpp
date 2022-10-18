/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
GammaAudioProcessorEditor::GammaAudioProcessorEditor(GammaAudioProcessor &p)
    : AudioProcessorEditor(&p), audioProcessor(p), top(p.audioSource, p.apvts), ampControls(p.getActiveGRSource(), p.apvts), reverbComp(p.apvts), menu(p.apvts, 200), dl(false), tooltip(this)
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

    cabComponent.setState(*p.apvts.getRawParameterValue("cabOn") + *p.apvts.getRawParameterValue("cabType"));

    cabComponent.cabChanged = [&](bool state, int newType)
    {
        p.apvts.getParameterAsValue("cabType") = newType;
        p.apvts.getParameterAsValue("cabOn") = state;
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
    auto trimmedTop = top.removeFromTop(top.getHeight() / 10);

    logo->drawWithin(g, trimmedTop.removeFromLeft(trimmedTop.getWidth() / 12).reduced(5).toFloat(), RectanglePlacement::centred, 1.f);
}

void GammaAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    auto w = getWidth();
    auto h = getHeight();
    auto topSection = bounds.removeFromTop(h * 0.085f);
    auto bottomSection = bounds.removeFromBottom(h * 0.285f);
    auto ampSection = bounds.removeFromBottom(h * 0.285f).reduced(1);

    pluginTitle.setBounds(topSection.removeFromRight(w * 0.66f).removeFromLeft(w / 3));
    menu.setBounds(w - (w * 0.25f), h * 0.03f, w * 0.25f, h / 2.5f);
    // dl.setBounds(w - (w * 0.3f), h * 0.03f, w * 0.1f, h * 0.1f);

    topSection.removeFromLeft(w / 12);
    topSection.translate(0, -5);
    auto topSectionThird = topSection.getWidth() / 3;
    inGain.setBounds(topSection.removeFromLeft(topSectionThird).reduced(5, 0));
    outGain.setBounds(topSection.removeFromLeft(topSectionThird).reduced(5, 0));
    gate.setBounds(topSection.reduced(5, 0));

    top.setBounds(bounds);
    ampControls.setBounds(ampSection);
    cabComponent.setBounds(bottomSection.removeFromLeft(w * 0.66f));
    reverbComp.setBounds(bottomSection);
    // auto children = getChildren();
    // children.removeLast();
    // auto scale = (float)getWidth() / 800.f;

    // for (auto &c : children)
    //     c->setTransform(AffineTransform::scale(scale));

    MessageManager::callAsync([&]{writeConfigFile("size", getWidth());});
}