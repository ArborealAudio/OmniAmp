/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

#include <JuceHeader.h>
#include "Processors/Processors.h"
#include "UI/SineWave.hpp"
#include "VolumeMeter/VolumeMeter.h"

//==============================================================================
/**
*/
class GammaAudioProcessor  : public juce::AudioProcessor,
    public AudioProcessorValueTreeState::Listener
{
public:
    //==============================================================================
    GammaAudioProcessor();
    ~GammaAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

   #ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
   #endif

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    const juce::String getName() const override;

    bool acceptsMidi() const override;
    bool producesMidi() const override;
    bool isMidiEffect() const override;
    double getTailLengthSeconds() const override;

    //==============================================================================
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int index, const juce::String& newName) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    void parameterChanged(const String& parameterID, float newValue) override;

    AudioProcessorValueTreeState apvts;

    strix::AudioSource audioSource;

    double lastSampleRate = 0.0;

    VolumeMeterSource& getActiveGRSource()
    {
      switch(currentMode)
      {
        case Guitar:
          return guitar.getActiveGRSource();
          break;
        case Bass:
          return bass.getActiveGRSource();
          break;
        case Channel:
          return channel.getActiveGRSource();
          break;
      }
    }

private:

    AudioProcessorValueTreeState::ParameterLayout createParams();

    std::atomic<float>* gain, *outGain, *autoGain, *hiGain, *hfEnhance, *lfEnhance;

    /*std::array<ToneStackNodal, 3> toneStack
    { {
            {0.25e-9f, 25e-9f, 22e-9f, 300e3f, 0.25e6f, 20e3f, 65e3f},
            {0.25e-9f, 22e-9f, 22e-9f, 300e3f, 0.5e6f, 30e3f, 56e3f},
            {0.5e-9f, 22e-9f, 20e-9f, 270e3f, 1e6f, 125e3f, 33e3f}
    } };*/

    VolumeMeterSource meterSource;

    Guitar guitar;
    Bass bass;
    Channel channel;
    HFEnhancer<float> hfEnhancer;
    LFEnhancer lfEnhancer;
    dsp::Oversampling<float> oversample{2, 0, dsp::Oversampling<float>::FilterType::filterHalfBandFIREquiripple};

    ConvoCab cab;

    enum Mode
    {
        Guitar,
        Bass,
        Channel
    };

    Mode currentMode = Mode::Guitar;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GammaAudioProcessor)
};
