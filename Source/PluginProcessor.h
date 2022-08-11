/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#pragma once

// define for SIMD-specific declarations & functions
#ifndef USE_SIMD
    #if NDEBUG
        #define USE_SIMD 1
    #endif
#endif

#include <JuceHeader.h>
#include "../modules/chowdsp_wdf/include/chowdsp_wdf/chowdsp_wdf.h"
#include "Processors/Processors.h"
#include "UI/UI.h"
#include "UI/SineWave.hpp"

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
    void processBlock (juce::AudioBuffer<double>&, juce::MidiBuffer&) override;

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

    bool supportsDoublePrecisionProcessing() const override { return true; }

    AudioProcessorValueTreeState apvts;

    strix::AudioSource audioSource;

    double lastSampleRate = 0.0;

    strix::VolumeMeterSource& getActiveGRSource()
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

    ValueTree gtrState, bassState, channelState;

    std::atomic<float>* gain, *outGain, *autoGain, *hiGain, *hfEnhance, *lfEnhance;

    /*std::array<ToneStackNodal, 3> toneStack
    { {
            {0.25e-9f, 25e-9f, 22e-9f, 300e3f, 0.25e6f, 20e3f, 65e3f},
            {0.25e-9f, 22e-9f, 22e-9f, 300e3f, 0.5e6f, 30e3f, 56e3f},
            {0.5e-9f, 22e-9f, 20e-9f, 270e3f, 1e6f, 125e3f, 33e3f}
    } };*/

    strix::VolumeMeterSource meterSource;

    Processors::Guitar guitar;
    Processors::Bass bass;
    Processors::Channel channel;
    dsp::Oversampling<double> oversample{2, 2, dsp::Oversampling<double>::FilterType::filterHalfBandPolyphaseIIR};

    AudioBuffer<double> doubleBuffer;

    Processors::Enhancer hfEnhancer {apvts, Processors::Enhancer::Type::HF};
    Processors::Enhancer lfEnhancer {apvts, Processors::Enhancer::Type::LF};

    Processors::CabType currentCab = Processors::CabType::small;
#if USE_SIMD
    Processors::FDNCab<vec> cab;
#else
    Processors::FDNCab<double> cab;
#endif

    Processors::ReverbManager reverb;

    strix::SIMD<double, dsp::AudioBlock<double>, strix::AudioBlock<vec>> simd;

    enum Mode
    {
        Guitar,
        Bass,
        Channel
    };

    Mode currentMode = Mode::Channel;

    // expects stereo in and out
    void processDoubleBuffer(AudioBuffer<double>& buffer, bool mono)
    {
        dsp::AudioBlock<double> block(buffer);

        auto osBlock = oversample.processSamplesUp(block);

        switch (currentMode)
        {
        case Guitar:
            guitar.processBlock(osBlock);
            break;
        case Bass:
            bass.processBlock(osBlock);
            break;
        case Channel:
            channel.processBlock(osBlock);
            break;
        }

        if (*lfEnhance)
            lfEnhancer.processBlock(osBlock, (double)*lfEnhance, mono);

        if (*hfEnhance)
            hfEnhancer.processBlock(osBlock, (double)*hfEnhance, mono);

        oversample.processSamplesDown(block);

        setLatencySamples(oversample.getLatencyInSamples());

    #if USE_SIMD
        auto simdBlock = simd.interleaveBlock(block);
        auto &&processBlock = simdBlock;
    #else
        auto &&processBlock = block;
    #endif

        if (*apvts.getRawParameterValue("cabOn"))
            cab.processBlock(processBlock);

    #if USE_SIMD
        simd.deinterleaveBlock(processBlock);
    #endif

        if (*apvts.getRawParameterValue("reverbType"))
            reverb.process(buffer, *apvts.getRawParameterValue("roomAmt"));
    }

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (GammaAudioProcessor)
};
