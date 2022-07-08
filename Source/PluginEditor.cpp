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

    // addAndMakeVisible(wave);
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

    grMeter.setMeterType(VolumeMeterComponent::Type::Reduction);
    grMeter.setBounds(72, 200, 32, 100);
    addAndMakeVisible(grMeter);

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
