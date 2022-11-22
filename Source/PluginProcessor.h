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
                            public AudioProcessorValueTreeState::Listener,
                            public clap_juce_extensions::clap_properties
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

    String getWrapperTypeString()
    {
        if (wrapperType == wrapperType_Undefined && is_clap)
            return "CLAP";

        return juce::AudioProcessor::getWrapperTypeDescription(wrapperType);
    }

private:
    AudioProcessorValueTreeState::ParameterLayout createParams();

    std::atomic<float> *inGain, *outGain, *gate, *autoGain, *hiGain, *hfEnhance, *lfEnhance;

    float lastInGain = 1.f, lastOutGain = 1.f, lastWidth = 1.f, lastEmph = 0.f;

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

    strix::Balance emphasisIn, emphasisOut;

    strix::MonoToStereo<double> doubler;

    dsp::DryWetMixer<double> mixer{8};

    strix::SIMD<double, dsp::AudioBlock<double>, strix::AudioBlock<vec>> simd;

    enum Mode
    {
        Guitar,
        Bass,
        Channel
    };

    Mode currentMode = Mode::Channel;

    void setOversampleIndex();

    // expects stereo in and out
    void processDoubleBuffer(AudioBuffer<double> &buffer, bool mono)
    {
        auto inGain_raw = std::pow(10.f, inGain->load() * 0.05f);
        auto outGain_raw = std::pow(10.f, outGain->load() * 0.05f);

        if (*apvts.getRawParameterValue("gainLink"))
            outGain_raw *= 1.0 / inGain_raw;

        dsp::AudioBlock<double> block(buffer);

        mixer.pushDrySamples(block); /* causin' segfaults in auval. LORD we need a better solution! */

        if (*gate > -95.0)
            gateProc.process(dsp::ProcessContextReplacing<double>(block));

        /*apply input gain*/
        strix::SmoothGain<float>::applySmoothGain(block, inGain_raw, lastInGain);

        /* M/S encode if necessary */
        bool ms = (bool)*apvts.getRawParameterValue("m/s");

        if (ms && !mono)
            strix::MSMatrix::msEncode(block);

        /* Input Stereo Emphasis */
        float stereoEmph = *apvts.getRawParameterValue("stereoEmphasis");
        if ((bool)stereoEmph && !mono)
        {
            stereoEmph = jmap(stereoEmph, 0.1f, 10.f); /*create a range from -10 to 10*/
            emphasisIn.process(block, stereoEmph, ms);
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

        auto latency = oversample[os_index].getLatencyInSamples();

        setLatencySamples(latency);

#if USE_SIMD
        auto &&processBlock = simd.interleaveBlock(block);
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

        /* Output Stereo Emphasis */
        if ((bool)stereoEmph && !mono)
            emphasisOut.process(block, 1.f / stereoEmph, ms);

        if (ms && !mono)
            strix::MSMatrix::msDecode(block);

        double dubAmt = *apvts.getRawParameterValue("doubler");
        if (dubAmt && !mono)
            doubler.process(block, dubAmt);

        if (*apvts.getRawParameterValue("reverbType"))
            reverb.process(buffer, *apvts.getRawParameterValue("roomAmt"));

        // apply output gain
        strix::SmoothGain<float>::applySmoothGain(block, outGain_raw, lastOutGain);

        if (float width = *apvts.getRawParameterValue("width") != 1.f && !mono)
            strix::Balance::processBalance(block, width, false, lastWidth);

        mixer.setWetLatency(latency);
        mixer.setWetMixProportion(*apvts.getRawParameterValue("mix"));
        mixer.mixWetSamples(block);
    }

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GammaAudioProcessor)
};