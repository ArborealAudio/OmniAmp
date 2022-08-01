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
#if JUCE_WINDOWS
    opengl.attachTo(*this);
    opengl.setImageCacheSize((size_t)64 * 1024);
#endif

    setSize (800, 400);

    addAndMakeVisible(wave);
    wave.setSize(400, 200);
    wave.setCentrePosition(getLocalBounds().getCentreX(), 100);
    wave.setInterceptsMouseClicks(false, false);

    addAndMakeVisible(ampControls);
    ampControls.setSize(585, 130);
    ampControls.setCentrePosition(getLocalBounds().getCentreX(), getLocalBounds().getCentreY() + 65);

    addAndMakeVisible(mode);
    modeAttach = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts, "mode", mode);
    mode.setSize(100, 30);
    mode.setCentrePosition(getLocalBounds().getCentreX(), 360);

    addAndMakeVisible(lfEnhance);
    lfAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "lfEnhance", lfEnhance);
    lfEnhance.setBounds(getLocalBounds().getCentreX() - 350, 100, 100, 100);

    addAndMakeVisible(hfEnhance);
    hfAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "hfEnhance", hfEnhance);
    hfEnhance.setBounds(getLocalBounds().getCentreX() + 250, 100, 100, 100);

    grMeter.setMeterType(strix::VolumeMeterComponent::Type::Reduction);
    grMeter.setBounds(72, 200, 32, 100);
    addAndMakeVisible(grMeter);

    setSize(800, 600);

    auto bottomSection = getLocalBounds().removeFromBottom(200);

    cabComponent.setBounds(bottomSection.removeFromLeft(getWidth() * 0.66));
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
    getConstrainer()->setMinimumSize(200, 150);
    getConstrainer()->setFixedAspectRatio(1.333);

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
  // g.fillAll(Colour(0xff968875));
  g.setColour(Colour(0xffaa8875));
  g.fillRect(getLocalBounds().withTrimmedBottom(getHeight() / 3)); // make this adapt to size!!
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
