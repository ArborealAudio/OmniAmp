/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
GammaAudioProcessorEditor::GammaAudioProcessorEditor (GammaAudioProcessor& p)
    : AudioProcessorEditor (&p), audioProcessor (p), ampControls(p.apvts)
{
    setSize (800, 400);

    addAndMakeVisible(bkgd);
    bkgd.setSize(400, 200);
    bkgd.setCentrePosition(getLocalBounds().getCentreX(), 100);

    addAndMakeVisible(ampControls);
    ampControls.setSize(490, 200);
    ampControls.setCentrePosition(getLocalBounds().getCentreX(), getLocalBounds().getCentreY() + 100);

    addAndMakeVisible(mode);
    modeAttach = std::make_unique<AudioProcessorValueTreeState::ComboBoxAttachment>(p.apvts, "mode", mode);
    mode.setSize(100, 30);
    mode.setCentrePosition(getLocalBounds().getCentreX(), 20);

    addAndMakeVisible(hfEnhance);
    hfAttach = std::make_unique<AudioProcessorValueTreeState::SliderAttachment>(p.apvts, "hfEnhance", hfEnhance);
    hfEnhance.setBounds(getLocalBounds().getCentreX(), 100, 100, 100);

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
