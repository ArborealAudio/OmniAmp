/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
GammaAudioProcessorEditor::GammaAudioProcessorEditor(GammaAudioProcessor &p)
    : AudioProcessorEditor(&p), audioProcessor(p), top(p.audioSource, p.apvts), ampControls(p.getActiveGRSource(), p.apvts), cabComponent(p.apvts.getRawParameterValue("cabType")), reverbComp(p.apvts), menu(p.apvts, 200), presetMenu(p.apvts), dl(false), tooltip(this, 1000)
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
    if (lastWidth <= 0)
        lastWidth = 800;
    setSize(lastWidth, lastWidth);

    addAndMakeVisible(pluginTitle);
    String title = "GAMMA";
    Colour titleColor = Colours::beige;
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
    inGain.setTooltip("Input gain before all processing, useful for increasing or decreasing headroom before the amp.");
    inGain.setValueToStringFunction([](float val)
                                    { String str(val, 1); str += "dB"; return str; });

    addAndMakeVisible(outGain);
    outGainAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "outputGain", outGain);
    outGain.setLabel("Output");
    outGain.setTooltip("Output gain after all processing");
    outGain.setValueToStringFunction([](float val)
                                     { String str(val, 1); str += "dB"; return str; });

    addAndMakeVisible(gate);
    gateAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "gate", gate);
    gate.setLabel("Gate");
    gate.setTooltip("Simple noise gate before the amp.");
    gate.setValueToStringFunction([](float val)
                                  { if (val < -95.f) return String("Off");
                                    if (val >= -95.f) return String(val, 1); });

    addAndMakeVisible(width);
    widthAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "width", width);
    width.setLabel("Width");
    width.setTooltip("Standard stereo width control for widening the stereo image. This happens *before* the amp.");
    width.setValueToStringFunction([](float val)
                                   { String s(val * 100, 0); return s + "%"; });

    addAndMakeVisible(midSide);
    msAttach = std::make_unique<AudioProcessorValueTreeState::ButtonAttachment>(p.apvts, "m/s", midSide);
    midSide.setButtonText(midSide.getToggleState() ? "M/S" : "Stereo");
    midSide.setTooltip("Toggle for Stereo and Mid/Side processing.\n\nIn Stereo mode, Left and Right channels will be processed independently in the amp. In Mid/Side mode, the middle of the stereo image will be separated from the sides and then processed independently.\n\nThis is useful if you want to do Mid/Side compression or saturation, which can create a more spacious stereo image.");
    midSide.onStateChange = [&]
    { midSide.setButtonText(midSide.getToggleState() ? "M/S" : "Stereo"); };

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
    getConstrainer()->setMinimumSize(550, 550);
    getConstrainer()->setFixedAspectRatio(1.0);

    addChildComponent(dl);
    dl.centreWithSize(300, 200);

    addChildComponent(activation);
    activation.centreWithSize(300, 200);
    auto activated = activation.readFile();
    activation.setVisible(!activated);
    p.lockProcessing(!activated);
    activation.onActivationCheck = [&](bool result)
    {
        activation.setVisible(!result);
        p.lockProcessing(!result);
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
    
    auto top = getLocalBounds().withTrimmedBottom(getHeight() / 3);
    auto trimmedTop = top.removeFromTop(getHeight() * 0.12f);

    logo->drawWithin(g, trimmedTop.removeFromLeft(trimmedTop.getWidth() / 12).reduced(5).toFloat(), RectanglePlacement::centred, 1.f);
}

void GammaAudioProcessorEditor::resized()
{
    auto bounds = getLocalBounds();
    const auto w = getWidth();
    const auto h = getHeight();
    auto topSection = bounds.removeFromTop(h * 0.12f);
    auto bottomSection = bounds.removeFromBottom(h * 0.285f).reduced(1);
    auto ampSection = bounds.removeFromBottom(h * 0.285f).reduced(1);

    topSection.removeFromLeft(w / 12); // section where logo is drawn
    auto topSectionThird = topSection.getWidth() / 3;
    auto topKnobs = topSection.removeFromLeft(topSectionThird);
    auto titleSection = topSection.removeFromLeft(topSectionThird);
    auto presetSection = topSection.removeFromLeft(topSection.getWidth() * 0.66);

    pluginTitle.setBounds(titleSection);
    presetMenu.setBounds(presetSection.reduced(0, topSection.getHeight() * 0.25));
    menu.setBounds(w - (w * 0.25f), h * 0.03f, w * 0.25f, h / 3.f);

    auto knobThird = topKnobs.getWidth() / 3;
    auto knobHalf = topKnobs.getHeight() / 2;
    auto knobsBottom = topKnobs.removeFromBottom(knobHalf).withSizeKeepingCentre(knobThird * 2.f, knobHalf);
    inGain.setBounds(topKnobs.removeFromLeft(knobThird).reduced(5, 0));
    outGain.setBounds(topKnobs.removeFromLeft(knobThird).reduced(5, 0));
    gate.setBounds(topKnobs.reduced(5, 0));
    midSide.setBounds(knobsBottom.removeFromLeft(knobsBottom.getWidth() * 0.5).reduced(0, knobHalf * 0.15f));
    width.setBounds(knobsBottom);

    top.setBounds(bounds);
    ampControls.setBounds(ampSection);
    cabComponent.setBounds(bottomSection.removeFromLeft(w * 0.66f));
    reverbComp.setBounds(bottomSection);

    dl.centreWithSize(300, 200);

    activation.centreWithSize(300, 200);

    MessageManager::callAsync([&]{writeConfigFile("size", getWidth());});
}