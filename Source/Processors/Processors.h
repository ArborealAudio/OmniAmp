/*
  ==============================================================================

    Processors.h
    Created: 11 Apr 2022 10:20:37am
    Author:  alexb

  ==============================================================================
*/

#pragma once

#include "SIMD.h"

namespace Processors
{

enum class ProcessorType
{
    Guitar,
    Bass,
    Channel
};

#include "Tube.h"
#include "PreFilters.h"
#include "PostFilters.h"
#include "ToneStack.h"
#include "Comp.h"
#include "Enhancer.h"
#include "Cab.h"
#include "DistPlus.h"
#include "Room.h"

struct Processor : AudioProcessorValueTreeState::Listener
{
    Processor(AudioProcessorValueTreeState& a, ProcessorType t, strix::VolumeMeterSource& s) : apvts(a), comp(t, s)
    {
        inGain = apvts.getRawParameterValue("inputGain");
        outGain = apvts.getRawParameterValue("outputGain");
        p_comp = apvts.getRawParameterValue("comp");
        hiGain = apvts.getRawParameterValue("hiGain");
        dist = apvts.getRawParameterValue("dist");

        apvts.addParameterListener("comp", this);
    }

    ~Processor() { apvts.removeParameterListener("comp", this); }

    void parameterChanged(const String &parameterID, float newValue) override
    {
        if (parameterID.contains("comp"))
            comp.setThreshold(newValue);
    }

    virtual void prepare(const dsp::ProcessSpec &spec) = 0;

    virtual void reset()
    {
        comp.reset();

        for (auto& t : triode)
            t.reset();

        if (toneStack != nullptr)
            toneStack->reset();

        pentode.reset();
    }

    virtual void setDistParam(float newValue)
    {
        mxr.setParams(1.f - newValue);
    }

    /*0 = bass | 1 = mid | 2 = treble*/
    virtual void setToneControl(int control, float newValue)
    {
        if (toneStack == nullptr)
            return;
        
        switch (control)
        {
        case 0:
            toneStack->setBass(newValue);
            break;
        case 1:
            toneStack->setMid(newValue);
            break;
        case 2:
            toneStack->setTreble(newValue);
            break;
        }
    }

    virtual strix::VolumeMeterSource &getActiveGRSource() { return comp.getGRSource(); }

protected:
    void defaultPrepare(const dsp::ProcessSpec& spec)
    {
        comp.prepare(spec);

        mxr.prepare(spec.sampleRate);

        for (auto& t : triode)
            t.prepare(spec);

        if (toneStack != nullptr)
            toneStack->prepare(spec);

        pentode.prepare(spec);

        simd.setInterleavedBlockSize(spec.numChannels, spec.maximumBlockSize);
    }

    AudioProcessorValueTreeState &apvts;

    OptoComp<double> comp;
#if USE_SIMD
    MXRDistWDF<vec> mxr;
    std::unique_ptr<ToneStackNodal<vec>> toneStack;
    std::vector<AVTriode<vec>> triode;
    Tube<vec> pentode;
#else
    MXRDistWDF<double> mxr;
    std::unique_ptr<ToneStackNodal<double>> toneStack;
    std::vector<AVTriode<double>> triode;
    Tube<double> pentode;
#endif

    std::atomic<float>* inGain, *outGain, *hiGain, *p_comp, *dist;

    SIMD<double, dsp::AudioBlock<double>, chowdsp::AudioBlock<vec>> simd;
};

struct Guitar : Processor
{
    Guitar(AudioProcessorValueTreeState& a, strix::VolumeMeterSource& s) : Processor(a, ProcessorType::Guitar, s)
    {
    #if USE_SIMD
        toneStack = std::make_unique<ToneStackNodal<vec>>((vec)0.25e-9, (vec)25e-9, (vec)22e-9, (vec)300e3, (vec)0.5e6, (vec)20e3, (vec)65e3);
    #else
        toneStack = std::make_unique<ToneStackNodal<double>>(0.25e-9, 25e-9, 22e-9, 300e3, 0.5e6, 20e3, 65e3);
    #endif
        triode.resize(4);
    }

    void prepare(const dsp::ProcessSpec& spec) override
    {
        defaultPrepare(spec);
        gtrPre.prepare(spec);
    }

    template <typename T>
    void processBlock(dsp::AudioBlock<T>& block)
    {
        T gain_raw = jmap(inGain->load(), 1.f, 8.f);
        T out_raw = jmap(outGain->load(), 1.f, 8.f);

        comp.processBlock(block, *p_comp);

#if USE_SIMD
        auto simdBlock = simd.interleaveBlock(block);
        auto&& processBlock = simdBlock;
#else
        auto&& processBlock = block;
#endif

        if (*dist > 0.f)
            mxr.processBlock(processBlock);

        processBlock.multiplyBy(gain_raw);
        triode[0].processBlock(processBlock, 0.5, 1.0);

        gtrPre.processBlock(processBlock, *hiGain);

        triode[1].processBlock(processBlock, 0.7, 1.2);
        triode[2].processBlock(processBlock, 0.7, 1.2);

        toneStack->processBlock(processBlock);

        if (*hiGain)
            triode[3].processBlock(processBlock, 1.0, 2.0);

        processBlock.multiplyBy(out_raw * 6.0);

        pentode.processBlockClassB(processBlock, 0.6, 0.6);

    #if USE_SIMD
        simd.deinterleaveBlock(processBlock);
    #endif
    }

private:
#if USE_SIMD
    GuitarPreFilter<vec> gtrPre;
#endif
};

struct Bass : Processor
{
    Bass(AudioProcessorValueTreeState& a, strix::VolumeMeterSource& s) : Processor(a, ProcessorType::Bass, s)
    {
    #if USE_SIMD
        toneStack = std::make_unique<ToneStackNodal<vec>>((vec)0.5e-9, (vec)10e-9, (vec)10e-9, (vec)250e3, (vec)0.5e6, (vec)30e3, (vec)100e3);
    #else
        toneStack = std::make_unique<ToneStackNodal<double>>(0.5e-9, 10e-9, 10e-9, 250e3, 0.5e6, 30e3, 100e3);
    #endif
        triode.resize(4);
    }

    void prepare(const dsp::ProcessSpec& spec) override
    {
        defaultPrepare(spec);
    }

    template <typename T>
    void processBlock(dsp::AudioBlock<T>& block)
    {
        T gain_raw = jmap(inGain->load(), 1.f, 8.f);
        T out_raw = jmap(outGain->load(), 1.f, 8.f);

        comp.processBlock(block, *p_comp);

    #if USE_SIMD
        auto simdBlock = simd.interleaveBlock(block);
        auto&& processBlock = simdBlock;
    #else
        auto&& processBlock = block;
    #endif

        if (*dist > 0.f)
            mxr.processBlock(processBlock);

        triode[0].processBlock(processBlock, 0.5, 1.0);

        processBlock.multiplyBy(gain_raw);
        if (*hiGain)
            processBlock.multiplyBy(2.f);

        triode[1].processBlock(processBlock, 0.5, 1.0);
        if (*hiGain) {
            triode[2].processBlock(processBlock, 1.0, 2.0);
            triode[3].processBlock(processBlock, 1.0, 2.0);
        }

        toneStack->processBlock(processBlock);

        processBlock.multiplyBy(out_raw * 6.0);

        if (!*hiGain)
            pentode.processBlockClassB(processBlock, 0.6, 0.6);
        else
            pentode.processBlockClassB(processBlock, 0.7, 0.7);

    #if USE_SIMD
        simd.deinterleaveBlock(processBlock);
    #endif
    }
};

struct Channel : Processor
{
    Channel(AudioProcessorValueTreeState& a, strix::VolumeMeterSource& s) : Processor(a, ProcessorType::Channel, s)
    {
        toneStack = nullptr;
        triode.resize(2);
    }

    void prepare(const dsp::ProcessSpec& spec) override
    {
        defaultPrepare(spec);

        low.prepare(spec);
        low.setCutoffFreq(350.0);
        low.setType(strix::FilterType::firstOrderLowpass);

        mid.prepare(spec);
        mid.setCutoffFreq(900.0);
        mid.setType(strix::FilterType::bandpass);

        hi.prepare(spec);
        hi.setCutoffFreq(5000.0);
        hi.setType(strix::FilterType::firstOrderHighpass);
    }

    void setFilters(int index, float newValue)
    {
        switch (index)
        {
        case 0:
            low.setGain(newValue);
            break;
        case 1:
            mid.setGain(newValue);
            break;
        case 2:
            hi.setGain(newValue);
            break;
        default:
            break;
        }
    }

    template <typename T>
    void processBlock(dsp::AudioBlock<T>& block)
    {
        T gain_raw = jmap(inGain->load(), 1.f, 4.f);
        T out_raw = jmap(outGain->load(), 1.f, 4.f);
        
        comp.processBlock(block, *p_comp);

    #if USE_SIMD
        auto simdBlock = simd.interleaveBlock(block);
        auto&& processBlock = simdBlock;
    #else
        auto&& processBlock = block;
    #endif

        if (*dist > 0.f)
            mxr.processBlock(processBlock);

        processBlock.multiplyBy(gain_raw);

        if (*inGain > 0.f) {
            triode[0].processBlock(processBlock, inGain->load(), 2.f * inGain->load());
            if (*hiGain)
                triode[1].processBlock(processBlock, inGain->load(), gain_raw);
        }

        processFilters(processBlock);

        if (*outGain > 0.f) {
            processBlock.multiplyBy(out_raw * 6.f);

            if (!*hiGain)
                pentode.processBlockClassB(processBlock, 0.4f, 0.4f);
            else
                pentode.processBlockClassB(processBlock, 0.6f, 0.6f);
        }
        
    #if USE_SIMD
        simd.deinterleaveBlock(processBlock);
    #endif
    }

private:
#if USE_SIMD
    strix::SVTFilter<vec> low, mid, hi;
#else
    strix::SVTFilter<double> low, mid, hi;
#endif

    template <class Block>
    void processFilters(Block& block)
    {
        for (auto ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto in = block.getChannelPointer(ch);

            for (auto i = 0; i < block.getNumSamples(); ++i)
            {
                auto l = low.processSample(ch, in[i]);
                auto m = mid.processSample(ch, in[i]);
                auto h = hi.processSample(ch, in[i]);

                in[i] += + l + m + h;
            }
        }
    }
};

} // namespace Processors