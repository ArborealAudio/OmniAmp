/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin editor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
GammaAudioProcessorEditor::GammaAudioProcessorEditor(GammaAudioProcessor &p)
    : AudioProcessorEditor(&p), main(p)
{
    addAndMakeVisible(main);
    
    // viewport.setViewedComponent(this, false);
    // viewport.setScrollBarsShown(true, false);
    // viewport.setScrollBarPosition(true, true);
    setSize(800, 610);
}

GammaAudioProcessorEditor::~GammaAudioProcessorEditor()
{
}

//==============================================================================
void GammaAudioProcessorEditor::paint(juce::Graphics &g)
{
}

void GammaAudioProcessorEditor::resized()
{
    main.setSize(800, 800);
}