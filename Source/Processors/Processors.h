/*
  ==============================================================================

    Processors.h
    Created: 11 Apr 2022 10:20:37am
    Author:  alexb

  ==============================================================================
*/

#pragma once

enum class ProcessorType
{
    Guitar,
    Bass,
    Channel
};

#include "SIMD.h"
#include "Tube.h"
#include "PreFilters.h"
#include "PostFilters.h"
#include "ToneStack.h"
#include "Comp.h"
#include "Enhancer.h"
#include "Cab.h"
#include "DistPlus.h"

struct Processor
{
    Processor(AudioProcessorValueTreeState& a, ProcessorType t, VolumeMeterSource& s) : apvts(a), comp(t, s)
    {
        inGain = apvts.getRawParameterValue("inputGain");
        outGain = apvts.getRawParameterValue("outputGain");
        p_comp = apvts.getRawParameterValue("comp");
        hiGain = apvts.getRawParameterValue("hiGain");
        dist = apvts.getRawParameterValue("dist");
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

    virtual VolumeMeterSource &getActiveGRSource() { return comp.getGRSource(); }

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

        simd.setInterleavedBlockSize(1, spec.maximumBlockSize);
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

    SIMD<double> simd;
};

struct Guitar : Processor
{
    Guitar(AudioProcessorValueTreeState& a, VolumeMeterSource& s) : Processor(a, ProcessorType::Guitar, s)
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

        triode[1].processBlock(processBlock, 0.5, 1.0);
        triode[2].processBlock(processBlock, 0.5, 1.0);

        toneStack->processBlock(processBlock);

        if (*hiGain) {
            triode[3].processBlock(processBlock, 0.5f, 1.f);
        }

        processBlock.multiplyBy(out_raw * 6.f);

        pentode.processBlockClassB(processBlock, 0.6f, 0.6f);

    #if USE_SIMD
        block = simd.deinterleaveBlock(processBlock);
    #endif
    }

private:
#if USE_SIMD
    GuitarPreFilter<vec> gtrPre;
#else
    GuitarPreFilter<double> gtrPre;
#endif
};

struct Bass : Processor
{
    Bass(AudioProcessorValueTreeState& a, VolumeMeterSource& s) : Processor(a, ProcessorType::Bass, s)
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

        processBlock.multiplyBy(out_raw);

        if (!*hiGain)
            pentode.processBlockClassB(processBlock, 1.f, 1.f);
        else
            pentode.processBlockClassB(processBlock, 1.5f, 1.5f);
        
    #if USE_SIMD
        block = simd.deinterleaveBlock(processBlock);
    #endif
    }
};

struct Channel : Processor
{
    Channel(AudioProcessorValueTreeState& a, VolumeMeterSource& s) : Processor(a, ProcessorType::Channel, s)
    {
        toneStack = nullptr;
        triode.resize(2);
    }

    void prepare(const dsp::ProcessSpec& spec) override
    {
        defaultPrepare(spec);
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

        if (*outGain > 0.f) {
            processBlock.multiplyBy(out_raw * 6.f);

            if (!*hiGain) {
                pentode.processBlockClassB(processBlock, 0.4f, 0.4f);
            }
            else {
                pentode.processBlockClassB(processBlock, 0.6f, 0.6f);
            }
        }
        
    #if USE_SIMD
        block = simd.deinterleaveBlock(processBlock);
    #endif
    }
};