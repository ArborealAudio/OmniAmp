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
    #else // make sure DBG builds are labeled non-production
        #ifndef PRODUCTION_BUILD
            #define PRODUCTION_BUILD 0
        #endif
    #endif
#endif
#ifndef PRODUCTION_BUILD // use this to control production build parameter
    #define PRODUCTION_BUILD 1
#endif

#include <JuceHeader.h>
#include <chowdsp_wdf/chowdsp_wdf.h>
#include "Processors/Processors.h"
#include "Presets/PresetManager.h"
#include "UI/UI.h"
#include "UI/SineWave.hpp"
#if PRODUCTION_BUILD
    #define BETA_BUILD 1
#endif
#include "Activation.hpp"

//==============================================================================
/**
 */
class GammaAudioProcessor : public juce::AudioProcessor,
                            public AudioProcessorValueTreeState::Listener
{
public:
    //==============================================================================
    GammaAudioProcessor();
    ~GammaAudioProcessor() override;

    //==============================================================================
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;

#ifndef JucePlugin_PreferredChannelConfigurations
    bool isBusesLayoutSupported(const BusesLayout &layouts) const override;
#endif

    void processBlock(juce::AudioBuffer<float> &, juce::MidiBuffer &) override;
    void processBlock(juce::AudioBuffer<double> &, juce::MidiBuffer &) override;

    //==============================================================================
    juce::AudioProcessorEditor *createEditor() override;
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
    void setCurrentProgram(int index) override;
    const juce::String getProgramName(int index) override;
    void changeProgramName(int index, const juce::String &newName) override;

    //==============================================================================
    void getStateInformation(juce::MemoryBlock &destData) override;
    void setStateInformation(const void *data, int sizeInBytes) override;

    void parameterChanged(const String &parameterID, float newValue) override;

    bool supportsDoublePrecisionProcessing() const override { return true; }

    AudioProcessorValueTreeState apvts;

    strix::AudioSource audioSource;

    double lastSampleRate = 0.0, SR = 0.0;
    int maxBlockSize = 0;

    strix::VolumeMeterSource &getActiveGRSource()
    {
        switch (currentMode)
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

    String currentPreset = "";

    // call this from the UI if activation fails and processing should suspend
    void lockProcessing(bool shouldBeLocked)
    {
        suspendProcessing(shouldBeLocked);
    }

private:
    AudioProcessorValueTreeState::ParameterLayout createParams();

    std::atomic<float> *inGain, *outGain, *gate, *autoGain, *hiGain, *hfEnhance, *lfEnhance;

    float lastInGain = 1.f, lastOutGain = 1.f;

    /*std::array<ToneStackNodal, 3> toneStack
    { {
            {0.25e-9f, 25e-9f, 22e-9f, 300e3f, 0.25e6f, 20e3f, 65e3f},
            {0.25e-9f, 22e-9f, 22e-9f, 300e3f, 0.5e6f, 30e3f, 56e3f}
    } };*/

    strix::VolumeMeterSource meterSource;

    dsp::NoiseGate<double> gateProc;

    Processors::Guitar guitar;
    Processors::Bass bass;
    Processors::Channel channel;
    std::array<dsp::Oversampling<double>, 2> oversample{dsp::Oversampling<double>(2),
    dsp::Oversampling<double>(2, 2, dsp::Oversampling<double>::FilterType::filterHalfBandPolyphaseIIR)};
    size_t os_index = 0;

    AudioBuffer<double> doubleBuffer;

#if USE_SIMD
    Processors::Enhancer<vec, Processors::EnhancerType::HF> hfEnhancer{apvts};
    Processors::Enhancer<vec, Processors::EnhancerType::LF> lfEnhancer{apvts};
    Processors::FDNCab<vec> cab;
#else
    Processors::Enhancer<double, Processors::EnhancerType::HF> hfEnhancer{apvts};
    Processors::Enhancer<double, Processors::EnhancerType::LF> lfEnhancer{apvts};
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

    void setOversampleIndex();

    // lastGain is a reference so it can be directly changed by this function
    inline void applySmoothedGain(AudioBuffer<double>& buffer, float currentGain, float& lastGain)
    {
        if (currentGain == lastGain)
        {
            buffer.applyGain(currentGain);
            lastGain = currentGain;
        }
        else
        {
            buffer.applyGainRamp(0, buffer.getNumSamples(), lastGain, currentGain);
            lastGain = currentGain;
        }
    }

    // expects stereo in and out
    void processDoubleBuffer(AudioBuffer<double> &buffer)
    {
        auto inGain_raw = std::pow(10.f, inGain->load() * 0.05f);
        auto outGain_raw = std::pow(10.f, outGain->load() * 0.05f);

        if (*apvts.getRawParameterValue("gainLink"))
            outGain_raw *= 1.0 / inGain_raw;

        dsp::AudioBlock<double> block(buffer);

        if (*gate > -95.0)
            gateProc.process(dsp::ProcessContextReplacing<double>(block));

        SmoothGain<float>::applySmoothGain(block, inGain_raw, lastInGain);

        bool ms = (bool)*apvts.getRawParameterValue("m/s");

        if (ms && block.getNumChannels() > 1)
            strix::MSMatrix::msEncode(block);

        float stereoEmph = *apvts.getRawParameterValue("stereoEmphasis");

        if (stereoEmph != 1.f) {
            if (stereoEmph == 0.f)
                stereoEmph = 0.01f;
            strix::Balance::processBalance(block, stereoEmph, ms);
        }

        auto osBlock = oversample[os_index].processSamplesUp(block);

        if (*apvts.getRawParameterValue("ampOn"))
        {
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
        }

        oversample[os_index].processSamplesDown(block);

        setLatencySamples(oversample[os_index].getLatencyInSamples());

#if USE_SIMD
        auto&& processBlock = simd.interleaveBlock(block);
#else
        auto &&processBlock = block;
#endif
        if (*lfEnhance)
            lfEnhancer.processBlock(processBlock, (double)*lfEnhance, *apvts.getRawParameterValue("lfEnhanceInvert"));

        if (*hfEnhance)
            hfEnhancer.processBlock(processBlock, (double)*hfEnhance, *apvts.getRawParameterValue("hfEnhanceInvert"));

        if (*apvts.getRawParameterValue("cabType"))
            cab.processBlock(processBlock);

#if USE_SIMD
        simd.deinterleaveBlock(processBlock);
#endif

        if (ms && block.getNumChannels() > 1) {
            strix::MSMatrix::msDecode(block);
            ms = false;
        }

        if (*apvts.getRawParameterValue("reverbType"))
            reverb.process(buffer, *apvts.getRawParameterValue("roomAmt"));

        // apply output gain
        SmoothGain<float>::applySmoothGain(block, outGain_raw, lastOutGain);

        if (stereoEmph != 1.f)
            strix::Balance::processBalance(block, 1.f / stereoEmph, ms);

        float width = *apvts.getRawParameterValue("width");

        if (block.getNumChannels() > 1 && width != 1.f)
            strix::Balance::processBalance(block, width, ms);
    }

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GammaAudioProcessor)
};