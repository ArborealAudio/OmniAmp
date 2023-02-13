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
#if !PRODUCTION_BUILD
#define DEV_BUILD 1
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

    bool checkedUpdate = false;

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

    std::array<dsp::Oversampling<double>, 2> oversample{dsp::Oversampling<double>(2),
                                                        dsp::Oversampling<double>(2, 2, dsp::Oversampling<double>::FilterType::filterHalfBandPolyphaseIIR)};
    size_t os_index = 0;

    AudioBuffer<double> doubleBuffer;

#if USE_SIMD
    Processors::Guitar<vec> guitar;
    Processors::Bass<vec> bass;
    Processors::Channel<vec> channel;
    Processors::FDNCab<vec> cab;
#else
    Processors::Guitar<double> guitar;
    Processors::Bass<double> bass;
    Processors::Channel<double> channel;
    Processors::FDNCab<double> cab;
#endif
    Processors::Enhancer<double, Processors::EnhancerType::HF> hfEnhancer;
    Processors::Enhancer<double, Processors::EnhancerType::LF> lfEnhancer;

    Processors::ReverbManager reverb;

    strix::Balance emphasisIn, emphasisOut;
    Processors::EmphasisFilter<double, Processors::EmphasisFilterType::Low> emphLow;
    Processors::EmphasisFilter<double, Processors::EmphasisFilterType::High> emphHigh;

    strix::MonoToStereo<double> doubler;

    Processors::CutFilters cutFilters;

    dsp::DelayLine<double, dsp::DelayLineInterpolationTypes::Thiran> mixDelay;
    dsp::DelayLine<double, dsp::DelayLineInterpolationTypes::Thiran> dryDelay;
    bool lastBypass = false;

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
        const size_t os_index_ = os_index;
        const bool isBypassed = (bool)*apvts.getRawParameterValue("bypass");

        if ((bool)*apvts.getRawParameterValue("gainLink"))
            outGain_raw *= 1.f / inGain_raw;

        dsp::AudioBlock<double> block(buffer);
        const size_t numChannels = mono ? 1 : block.getNumChannels();

        for (size_t ch = 0; ch < numChannels; ++ch)
        {
            const auto *in = block.getChannelPointer(ch);
            for (size_t i = 0; i < block.getNumSamples(); ++i)
                mixDelay.pushSample(ch, in[i]);
        }

        bool shouldBypass = processBypassIn(block, isBypassed);

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
        if (!mono)
        {
            stereoEmph = mapToLog10(stereoEmph, 0.1f, 10.f); /* create linear gain range btw ~0.1 - 10 */
            emphasisIn.process(block, stereoEmph, ms);
        }

        emphLow.processIn(block);
        emphHigh.processIn(block);

        auto osBlock = oversample[os_index_].processSamplesUp(block);

        if ((bool)*apvts.getRawParameterValue("ampOn"))
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

        oversample[os_index_].processSamplesDown(block);

        auto latency = oversample[os_index].getLatencyInSamples();
        setLatencySamples((int)latency);

        emphLow.processOut(block);
        emphHigh.processOut(block);

        if ((bool)*apvts.getRawParameterValue("cabType"))
        {
#if USE_SIMD
            auto &&processBlock = simd.interleaveBlock(block);
#else
            auto &&processBlock = block;
#endif
            cab.processBlock(processBlock);
#if USE_SIMD
            simd.deinterleaveBlock(processBlock);
#endif
        }

        /* Output Stereo Emphasis */
        if (!mono)
            emphasisOut.process(block, 1.f / stereoEmph, ms);

        if (ms && !mono)
            strix::MSMatrix::msDecode(block);

        double dubAmt = *apvts.getRawParameterValue("doubler");
        if ((bool)dubAmt && !mono)
            doubler.process(block, dubAmt);

        reverb.process(buffer, *apvts.getRawParameterValue("reverbAmt"));

        if ((bool)*lfEnhance)
            lfEnhancer.processBlock(block, (double)*lfEnhance, *apvts.getRawParameterValue("lfEnhanceInvert"));

        if ((bool)*hfEnhance)
            hfEnhancer.processBlock(block, (double)*hfEnhance, *apvts.getRawParameterValue("hfEnhanceInvert"));

        // final cut filters
        cutFilters.process(block);

        // apply output gain
        strix::SmoothGain<float>::applySmoothGain(block, outGain_raw, lastOutGain);

        float width = *apvts.getRawParameterValue("width");
        if (width != 1.f && !mono)
            strix::Balance::processBalance(block, width, false, lastWidth);

        mixDelay.setDelay(latency);
        dryDelay.setDelay((int)latency);
        float mixAmt = *apvts.getRawParameterValue("mix");
        for (size_t ch = 0; ch < numChannels; ++ch)
        {
            auto *out = block.getChannelPointer(ch);
            for (size_t i = 0; i < block.getNumSamples(); ++i)
                out[i] = ((1.f - mixAmt) * mixDelay.popSample(ch)) + mixAmt * out[i];
        }

        /* manage bypass */
        if (shouldBypass)
            processBypassOut(block, isBypassed);
        lastBypass = isBypassed;
    }

    inline float calcBassParam(float val)
    {
        return val * val * val;
    }

    inline bool processBypassIn(const dsp::AudioBlock<double> &block, const bool byp)
    {
        if (!byp && !lastBypass)
            return false;

        const size_t numChannels = block.getNumChannels();
        for (size_t ch = 0; ch < numChannels; ++ch)
        {
            const auto *in = block.getChannelPointer(ch);
            for (size_t i = 0; i < block.getNumSamples(); ++i)
                dryDelay.pushSample(ch, in[i]);
        }
        return true;
    }

    inline void processBypassOut(dsp::AudioBlock<double> &block, const bool byp)
    {
        const size_t numChannels = block.getNumChannels();

        if (lastBypass && byp) // bypass
        {
            for (size_t i = 0; i < block.getNumSamples(); ++i)
                for (size_t ch = 0; ch < numChannels; ++ch)
                    block.getChannelPointer(ch)[i] = dryDelay.popSample(ch);
        }
        else if (byp && !lastBypass) // fade-in dry
        {
            float inc = 1.f / (float)block.getNumSamples();
            float gain = 0.f;
            for (size_t i = 0; i < block.getNumSamples(); ++i)
            {
                for (size_t ch = 0; ch < numChannels; ++ch)
                    block.getChannelPointer(ch)[i] = (dryDelay.popSample(ch) * gain) + (block.getChannelPointer(ch)[i] * (1.f - gain));
                gain += inc;
            }
        }
        else if (!byp && lastBypass) // fade-out dry
        {
            float inc = 1.f / (float)block.getNumSamples();
            float gain = 0.f;
            for (size_t i = 0; i < block.getNumSamples(); ++i)
            {
                for (size_t ch = 0; ch < numChannels; ++ch)
                    block.getChannelPointer(ch)[i] = (dryDelay.popSample(ch) * (1.f - gain)) + (block.getChannelPointer(ch)[i] * gain);
                gain += inc;
            }
        }
    }

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(GammaAudioProcessor)
};