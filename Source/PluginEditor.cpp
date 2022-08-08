/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
GammaAudioProcessorEditor::GammaAudioProcessorEditor (GammaAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), ampControls(p.apvts), wave(p.audioSource), grMeter(p.getActiveGRSource()), reverbComp(p.apvts), tooltip(this)
{
#if JUCE_WINDOWS || JUCE_LINUX
    opengl.attachTo(*this);
    opengl.setImageCacheSize((size_t)64 * 1024);
#endif

    mesh = Drawable::createFromImageData(BinaryData::amp_mesh_2_svg, BinaryData::amp_mesh_2_svgSize);
    logo = Drawable::createFromImageData(BinaryData::logo_svg, BinaryData::logo_svgSize);

    setSize (800, 450);

    auto bounds = getLocalBounds();
    auto topSection = bounds.removeFromTop(50);
    auto qtr = bounds.getWidth() / 4;

    auto ampSection = bounds.removeFromBottom(200).reduced(10);
    auto topLeftQtr = bounds.removeFromLeft(qtr);
    auto topRightQtr = bounds.removeFromRight(qtr);

    addAndMakeVisible(wave);
    wave.setBounds(bounds.reduced(10));
    wave.setInterceptsMouseClicks(false, false);

    addAndMakeVisible(ampControls);
    ampControls.setBounds(ampSection);

    addAndMakeVisible(mode);
    modeAttach = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts, "mode", mode);
    mode.setSize(100, 30);
    mode.setCentrePosition(getLocalBounds().getCentreX(), 435);

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
                          .withTrimmedTop(ampSection.getHeight() * 0.8f)
                          .translated(10, -10));
    grMeter.setStatePointer(p.apvts.getRawParameterValue("comp"));
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
#if JUCE_WINDOWS || JUCE_LINUX
    opengl.detach();
#endif
}

//==============================================================================
void GammaAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.setColour(Colour(BACKGROUND_COLOR));
    auto top = getLocalBounds().withTrimmedBottom(getHeight() / 3);
    g.fillRect(top);

    mesh->drawWithin(g, top.withTrimmedBottom(top.getHeight() / 2).toFloat(), RectanglePlacement::fillDestination, 1.f);

    auto trimmedTop = top.removeFromTop(50);
    g.setColour(Colour(TOP_TRIM));
    g.fillRect(trimmedTop);
    logo->drawWithin(g, trimmedTop.removeFromLeft(45).reduced(5).toFloat(), RectanglePlacement::centred, 1.f);
}

void GammaAudioProcessorEditor::resized()
{
    auto children = getChildren();
    children.removeLast();
    auto scale = (float)getWidth() / 800.f;

    for (auto& c : children)
    {
    c->setTransform(AffineTransform::scale(scale));
    }
}
