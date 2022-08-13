/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
GammaAudioProcessorEditor::GammaAudioProcessorEditor (GammaAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), ampControls(p.apvts), wave(p.audioSource), grMeter(p.getActiveGRSource(), p.apvts.getRawParameterValue("comp")), reverbComp(p.apvts), tooltip(this)
{
#if JUCE_WINDOWS
    opengl.attachTo(*this);
    opengl.setImageCacheSize((size_t)64 * 1024);
#endif

    mesh = Drawable::createFromImageData(BinaryData::amp_mesh_2_svg, BinaryData::amp_mesh_2_svgSize);
    logo = Drawable::createFromImageData(BinaryData::logo_svg, BinaryData::logo_svgSize);

    setSize (800, 450);

    auto bounds = getLocalBounds();
    auto topSection = bounds.removeFromTop(50);
    auto qtr = bounds.getWidth() / 4;

    auto ampSection = bounds.removeFromBottom(200).reduced(10, 0).translated(0, -2);
    auto topLeftQtr = bounds.removeFromLeft(qtr);
    auto topRightQtr = bounds.removeFromRight(qtr);

    addAndMakeVisible(pluginTitle);
    pluginTitle.setBounds(topSection.removeFromLeft(getWidth() * 0.66f).removeFromRight(getWidth() / 3));
    pluginTitle.setText("GAMMA", NotificationType::dontSendNotification);
    pluginTitle.setFont(Font(getCustomFont()).withHeight(20.f).withExtraKerningFactor(0.5f));
    pluginTitle.setColour(Label::textColourId, Colours::beige);
    pluginTitle.setJustificationType(Justification::centred);

    auto topSectionThird = topSection.getWidth() / 3;

    addAndMakeVisible(inGain);
    inGainAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "inputGain", inGain);
    inGain.setBounds(topSection.removeFromLeft(topSectionThird));
    inGain.setLabel("Input");

    addAndMakeVisible(outGain);
    outGainAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "outputGain", outGain);
    outGain.setBounds(topSection.removeFromLeft(topSectionThird));
    outGain.setLabel("Output");

    addAndMakeVisible(gate);
    gateAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "gate", gate);
    gate.setBounds(topSection);
    gate.setLabel("Noise Gate");

    addAndMakeVisible(wave);
    wave.setBounds(bounds.reduced(10).translated(0, -5));
    wave.setInterceptsMouseClicks(false, false);

    blur = std::make_unique<Image>(Image::PixelFormat::ARGB, wave.getWidth(), wave.getHeight(), true);

    addAndMakeVisible(ampControls);
    ampControls.setBounds(ampSection);

    addAndMakeVisible(lfEnhance);
    lfAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "lfEnhance", lfEnhance);
    lfEnhance.setBounds(topLeftQtr);
    lfEnhance.onAltClick = [&](bool state)
    {
        p.apvts.getParameterAsValue("lfEnhanceAuto") = state;
    };

    addAndMakeVisible(hfEnhance);
    hfAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "hfEnhance", hfEnhance);
    hfEnhance.setBounds(topRightQtr);
    hfEnhance.onAltClick = [&](bool state)
    {
        p.apvts.getParameterAsValue("hfEnhanceAuto") = state;
    };

    grMeter.setMeterType(strix::VolumeMeterComponent::Type::Reduction);
    grMeter.setMeterLayout(strix::VolumeMeterComponent::Layout::Horizontal);
    grMeter.setMeterColor(Colours::oldlace);
    grMeter.setBounds(ampSection.withTrimmedRight(ampSection.getWidth() * 0.66f)
                          .withTrimmedTop(ampSection.getHeight() * 0.82f)
                          .translated(10, -10));
    addAndMakeVisible(grMeter);

    setSize(800, 650);

    auto bottomSection = getLocalBounds().removeFromBottom(200);

    cabComponent.setBounds(bottomSection.removeFromLeft(getWidth() * 0.66f));
    addAndMakeVisible(cabComponent);

    cabComponent.setState(*p.apvts.getRawParameterValue("cabOn") + *p.apvts.getRawParameterValue("cabType"));

    cabComponent.cabChanged = [&](bool state, int newType)
    {
        auto type = p.apvts.getParameterAsValue("cabType");
        auto on = p.apvts.getParameterAsValue("cabOn");

        type = newType;
        on = state;
    };

    reverbComp.setBounds(bottomSection);
    
    addAndMakeVisible(reverbComp);

    setResizable(true, true);
    getConstrainer()->setMinimumSize(400, 325);
    getConstrainer()->setFixedAspectRatio(1.231);

    setBufferedToImage(true);
}

GammaAudioProcessorEditor::~GammaAudioProcessorEditor()
{
#if JUCE_WINDOWS
    opengl.detach();
#endif
}

//==============================================================================
void GammaAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll(Colour(TOP_TRIM));
    g.setColour(Colour(BACKGROUND_COLOR));
    auto top = getLocalBounds().withTrimmedBottom(getHeight() / 3);
    auto trimmedTop = top.removeFromTop(top.getHeight() / 10);

    logo->drawWithin(g, trimmedTop.removeFromLeft(trimmedTop.getWidth() / 12).reduced(5).toFloat(), RectanglePlacement::centred, 1.f);

    auto topsection = top.withTrimmedBottom(top.getHeight() / 2).reduced(10, 0).translated(0, 2).toFloat();

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

    for (auto& c : children)
        c->setTransform(AffineTransform::scale(scale));

    if (blur == nullptr)
        return;
    blur->clear(blur->getBounds());
    wave.setVisible(false);
    blur = std::make_unique<Image>(createComponentSnapshot(wave.getBoundsInParent()));
    wave.setVisible(true);

    gin::applyContrast(*blur, -35);
    gin::applyStackBlur(*blur, 10);
}