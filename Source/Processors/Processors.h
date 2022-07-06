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

    virtual void processBlock(dsp::AudioBlock<float> &block) = 0;

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
    }

    AudioProcessorValueTreeState &apvts;

    OptoComp comp;
    MXRDistWDF mxr;
    std::unique_ptr<ToneStackNodal> toneStack;
    std::vector<AVTriode> triode;
    Tube pentode;

    std::atomic<float>* inGain, *outGain, *hiGain, *p_comp, *dist;
};

struct Guitar : Processor
{
    Guitar(AudioProcessorValueTreeState& a, VolumeMeterSource& s) : Processor(a, ProcessorType::Guitar, s)
    {
        toneStack = std::make_unique<ToneStackNodal>(0.25e-9f, 25e-9f, 22e-9f, 300e3f, 0.5e6f, 20e3f, 65e3f);
        triode.resize(4);
    }

    void prepare(const dsp::ProcessSpec& spec) override
    {
        defaultPrepare(spec);
        gtrPre.prepare(spec);
    }

    void processBlock(dsp::AudioBlock<float>& block) override
    {
        float gain_raw = jmap(inGain->load(), 1.f, 8.f);
        float out_raw = jmap(outGain->load(), 1.f, 8.f);

        comp.processBlock(block, *p_comp);

        if (*dist > 0.f)
            mxr.processBlock(block);

        block.multiplyBy(gain_raw);

        triode[0].processBlock(block, 0.5f, 1.f);

        gtrPre.processBlock(block, *hiGain);

        triode[1].processBlock(block, 0.5f, 1.f);
        triode[2].processBlock(block, 0.5f, 1.f);

        toneStack->processBlock(block);

        if (*hiGain)
            triode[3].processBlock(block, 0.5f, 1.f);

        block.multiplyBy(out_raw * 6.f);

        // if (*hiGain) {
            pentode.processBlockClassB(block, 0.6f, 0.6f);
        // }
        // else {
        //     pentode.processBlockClassB(block, 0.4f, 0.4f);
        // }
    }

private:
    GuitarPreFilter gtrPre;
};

struct Bass : Processor
{
    Bass(AudioProcessorValueTreeState& a, VolumeMeterSource& s) : Processor(a, ProcessorType::Bass, s)
    {
        toneStack = std::make_unique<ToneStackNodal>(0.5e-9f, 10e-9f, 10e-9f, 250e3f, 0.5e6f, 30e3f, 100e3f);
        triode.resize(4);
    }

    void prepare(const dsp::ProcessSpec& spec) override
    {
        defaultPrepare(spec);
    }

    void processBlock(dsp::AudioBlock<float>& block) override
    {
        float gain_raw = jmap(inGain->load(), 1.f, 8.f);
        float out_raw = jmap(outGain->load(), 1.f, 8.f);

        comp.processBlock(block, *p_comp);

        triode[0].processBlock(block, 0.5, 1.0);

        block.multiplyBy(gain_raw);
        if (*hiGain)
            block.multiplyBy(2.f);

        triode[1].processBlock(block, 0.5, 1.0);
        if (*hiGain) {
            triode[2].processBlock(block, 1.0, 2.0);
            triode[3].processBlock(block, 1.0, 2.0);
        }

        toneStack->processBlock(block);

        block.multiplyBy(out_raw);

        if (!*hiGain)
            pentode.processBlockClassB(block, 1.f, 1.f);
        else
            pentode.processBlockClassB(block, 1.5f, 1.5f);
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

    void processBlock(dsp::AudioBlock<float>& block) override
    {
        float gain_raw = jmap(inGain->load(), 1.f, 4.f);
        float out_raw = jmap(outGain->load(), 1.f, 4.f);

        comp.processBlock(block, *p_comp);
        
        if (*dist > 0.f)
            mxr.processBlock(block);

        block.multiplyBy(gain_raw);

        if (*inGain > 0.f) {
            triode[0].processBlock(block, *inGain, 2.f * *inGain);
            if (*hiGain)
                triode[1].processBlock(block, *inGain, gain_raw);
        }

        if (*outGain > 0.f) {
            block.multiplyBy(out_raw * 6.f);
            if (!*hiGain) {
                pentode.processBlockClassB(block, 0.4f, 0.4f);
            }
            else {
                pentode.processBlockClassB(block, 0.6f, 0.6f);
            }
        }
    }
};