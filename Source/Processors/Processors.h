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
    MXRDistWDF<vec> mxr;
    std::unique_ptr<ToneStackNodal<vec>> toneStack;
    std::vector<AVTriode<vec>> triode;
    Tube<vec> pentode;

    std::atomic<float>* inGain, *outGain, *hiGain, *p_comp, *dist;

    SIMD<double> simd;
};

struct Guitar : Processor
{
    Guitar(AudioProcessorValueTreeState& a, VolumeMeterSource& s) : Processor(a, ProcessorType::Guitar, s)
    {
        toneStack = std::make_unique<ToneStackNodal<vec>>((vec)0.25e-9, (vec)25e-9, (vec)22e-9, (vec)300e3, (vec)0.5e6, (vec)20e3, (vec)65e3);
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

        auto simdBlock = simd.interleaveBlock(block);

        if (*dist > 0.f)
            mxr.processBlockSIMD(simdBlock);

        simdBlock.multiplyBy(gain_raw);
        triode[0].processBlockSIMD(simdBlock, (vec)0.5, (vec)1.0);

        gtrPre.processBlock(simdBlock, *hiGain);

        triode[1].processBlockSIMD(simdBlock, (vec)0.5, (vec)1.0);
        triode[2].processBlockSIMD(simdBlock, (vec)0.5, (vec)1.0);

        toneStack->processBlock(simdBlock);

        if (*hiGain) {
            triode[3].processBlockSIMD(simdBlock, (vec)0.5f, (vec)1.f);
        }

        simdBlock.multiplyBy(out_raw * 6.f);

        pentode.processBlockClassB(simdBlock, 0.6f, 0.6f);

        block = simd.deinterleaveBlock(simdBlock);
    }

private:
    GuitarPreFilter<vec> gtrPre;
};

struct Bass : Processor
{
    Bass(AudioProcessorValueTreeState& a, VolumeMeterSource& s) : Processor(a, ProcessorType::Bass, s)
    {
        toneStack = std::make_unique<ToneStackNodal<vec>>((vec)0.5e-9, (vec)10e-9, (vec)10e-9, (vec)250e3, (vec)0.5e6, (vec)30e3, (vec)100e3);
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

        auto simdBlock = simd.interleaveBlock(block);

        triode[0].processBlockSIMD(simdBlock, (vec)0.5, (vec)1.0);

        simdBlock.multiplyBy(gain_raw);
        if (*hiGain)
            simdBlock.multiplyBy(2.f);

        triode[1].processBlockSIMD(simdBlock, (vec)0.5, (vec)1.0);
        if (*hiGain) {
            triode[2].processBlockSIMD(simdBlock, (vec)1.0, (vec)2.0);
            triode[3].processBlockSIMD(simdBlock, (vec)1.0, (vec)2.0);
        }

        toneStack->processBlock(simdBlock);

        simdBlock.multiplyBy(out_raw);

        if (!*hiGain)
            pentode.processBlockClassB(simdBlock, 1.f, 1.f);
        else
            pentode.processBlockClassB(simdBlock, 1.5f, 1.5f);
        
        block = simd.deinterleaveBlock(simdBlock);
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
        
        auto simdBlock = simd.interleaveBlock(block);

        if (*dist > 0.f)
            mxr.processBlockSIMD(simdBlock);

        simdBlock.multiplyBy(gain_raw);

        if (*inGain > 0.f) {
            triode[0].processBlockSIMD(simdBlock, (vec)inGain->load(), (vec)2.f * inGain->load());
            if (*hiGain)
                triode[1].processBlockSIMD(simdBlock, (vec)inGain->load(), (vec)gain_raw);
        }

        if (*outGain > 0.f) {
            simdBlock.multiplyBy(out_raw * 6.f);

            if (!*hiGain) {
                pentode.processBlockClassB(simdBlock, 0.4f, 0.4f);
            }
            else {
                pentode.processBlockClassB(simdBlock, 0.6f, 0.6f);
            }
        }
        
        block = simd.deinterleaveBlock(simdBlock);
    }
};