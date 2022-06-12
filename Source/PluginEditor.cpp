/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
GammaAudioProcessorEditor::GammaAudioProcessorEditor (GammaAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), ampControls(p.apvts), wave(p.audioSource), grMeter(p.getActiveGRSource())
{
    setSize (800, 400);

    addAndMakeVisible(wave);
    wave.setSize(400, 200);
    wave.setCentrePosition(getLocalBounds().getCentreX(), 100);
    wave.setInterceptsMouseClicks(false, false);

    addAndMakeVisible(ampControls);
    ampControls.setSize(490, 200);
    ampControls.setCentrePosition(getLocalBounds().getCentreX(), getLocalBounds().getCentreY() + 100);

    addAndMakeVisible(mode);
    modeAttach = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts, "mode", mode);
    mode.setSize(100, 30);
    mode.setCentrePosition(getLocalBounds().getCentreX(), 20);

    addAndMakeVisible(lfEnhance);
    lfAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "lfEnhance", lfEnhance);
    lfEnhance.setBounds(getLocalBounds().getCentreX() - 300, 100, 100, 100);

    addAndMakeVisible(hfEnhance);
    hfAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "hfEnhance", hfEnhance);
    hfEnhance.setBounds(getLocalBounds().getCentreX() + 200, 100, 100, 100);

    addAndMakeVisible(grMeter);
    grMeter.setMeterType(VolumeMeterComponent::Type::Reduction);
    grMeter.setBounds(100, 200, 35, 150);

    setResizable(true, true);
    getConstrainer()->setMinimumSize(200, 150);
    getConstrainer()->setFixedAspectRatio(2.0);
}

GammaAudioProcessorEditor::~GammaAudioProcessorEditor()
{
}

//==============================================================================
void GammaAudioProcessorEditor::paint (juce::Graphics& g)
{
  g.fillAll(Colour(0xff968875));

  // Rectangle<int> testbounds{JUCE_LIVE_CONSTANT(20),
  //                             JUCE_LIVE_CONSTANT((getHeight() / 2) - 35),
  //                             JUCE_LIVE_CONSTANT(70),
  //                             JUCE_LIVE_CONSTANT(70)};
  grMeter.paint(g);
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
