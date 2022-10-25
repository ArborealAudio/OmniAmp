/*
  ==============================================================================

    Processors.h
    Created: 11 Apr 2022 10:20:37am
    Author:  alexb

  ==============================================================================
*/

#pragma once

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
        inGain = apvts.getRawParameterValue("preampGain");
        inGainAuto = apvts.getRawParameterValue("preampAutoGain");
        outGain = apvts.getRawParameterValue("powerampGain");
        outGainAuto = apvts.getRawParameterValue("powerampAutoGain");
        eqAutoGain = apvts.getRawParameterValue("eqAutoGain");
        p_comp = apvts.getRawParameterValue("comp");
        linked = apvts.getRawParameterValue("compLink");
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
        SR = spec.sampleRate;
        numSamples = spec.maximumBlockSize;
        numChannels = spec.numChannels;

        comp.prepare(spec);

        mxr.prepare(spec.sampleRate);

        for (auto& t : triode)
            t.prepare(spec);

        if (toneStack != nullptr)
            toneStack->prepare(spec);

        pentode.prepare(spec);

        // updateSIMD = true;
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

    std::atomic<float> *inGain, *inGainAuto, *outGain, *outGainAuto, *hiGain, *p_comp, *linked, *dist, *eqAutoGain;

    strix::SIMD<double, dsp::AudioBlock<double>, strix::AudioBlock<vec>> simd;

    std::atomic<bool> updateSIMD = false;

    double SR = 44100.0;
    int numSamples = 0, numChannels = 0;
};

struct Guitar : Processor
{
    Guitar(AudioProcessorValueTreeState& a, strix::VolumeMeterSource& s) : Processor(a, ProcessorType::Guitar, s)
    {
    #if USE_SIMD
        toneStack = std::make_unique<ToneStackNodal<vec>>((vec)0.25e-9, (vec)25e-9, (vec)22e-9, (vec)300e3, (vec)1e6, (vec)20e3, (vec)65e3);
    #else
        toneStack = std::make_unique<ToneStackNodal<double>>(0.25e-9, 25e-9, 22e-9, 300e3, 1e6, 20e3, 65e3);
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

        T autoGain = 1.0;

        if (!*apvts.getRawParameterValue("compPos"))
            comp.processBlock(block, *p_comp, *linked);

        // if (updateSIMD) {
        //     simd.setInterleavedBlockSize(numChannels, numSamples);
        //     updateSIMD = false;
        // }

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

        triode[1].processBlock(processBlock, 1.4, 2.4);
        triode[2].processBlock(processBlock, 1.4, 2.4);

        if (*inGainAuto)
            autoGain *= 1.0 / gain_raw;

        toneStack->processBlock(processBlock);

        if (*hiGain)
            triode[3].processBlock(processBlock, 2.0, 4.0);

        processBlock.multiplyBy(out_raw * 6.0);

        pentode.processBlockClassB(processBlock, 0.6, 0.6);

        if (*outGainAuto)
            autoGain *= 1.0 / out_raw;

        processBlock.multiplyBy(autoGain);

#if USE_SIMD
        simd.deinterleaveBlock(processBlock);
#endif

        if (*apvts.getRawParameterValue("compPos"))
            comp.processBlock(block, *p_comp, *linked);
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
    Bass(AudioProcessorValueTreeState& a, strix::VolumeMeterSource& s) : Processor(a, ProcessorType::Bass, s)
    {
    #if USE_SIMD
        toneStack = std::make_unique<ToneStackNodal<vec>>((vec)0.5e-9, (vec)10e-9, (vec)10e-9, (vec)250e3, (vec)0.5e6, (vec)100e3, (vec)200e3);
    #else
        toneStack = std::make_unique<ToneStackNodal<double>>(0.5e-9, 10e-9, 10e-9, 250e3, 0.5e6, 100e3, 200e3);
    #endif
        triode.resize(4);
    }

    void prepare(const dsp::ProcessSpec& spec) override
    {
        defaultPrepare(spec);

        scoop.prepare(spec);
        scoop.coefficients = dsp::IIR::Coefficients<double>::makePeakFilter(spec.sampleRate, 650.0, 1.0, 0.5);
    }

    template <typename T>
    void processBlock(dsp::AudioBlock<T>& block)
    {
        T gain_raw = jmap(inGain->load(), 1.f, 8.f);
        T out_raw = jmap(outGain->load(), 1.f, 8.f);

        T autoGain = 1.0;

        if (!*apvts.getRawParameterValue("compPos"))
            comp.processBlock(block, *p_comp, *linked);

        // if (updateSIMD) {
        //     simd.setInterleavedBlockSize(numChannels, numSamples);
        //     updateSIMD = false;
        // }

#if USE_SIMD
        auto simdBlock = simd.interleaveBlock(block);
        auto&& processBlock = simdBlock;
#else
        auto&& processBlock = block;
#endif

        if (*dist > 0.f)
            mxr.processBlock(processBlock);

        triode[0].processBlock(processBlock, 1.0, 2.0);

        processBlock.multiplyBy(gain_raw);

        if (*inGainAuto)
            autoGain *= 1.0 / gain_raw;

        if (*hiGain)
        {
            processBlock.multiplyBy(2.f);
            if (*inGainAuto)
                autoGain *= 0.5;
        }

        triode[1].processBlock(processBlock, 1.0, 2.0);

        for (int ch = 0; ch < processBlock.getNumChannels(); ++ch)
        {
           auto dest = processBlock.getChannelPointer(ch);
           for (int i = 0; i < processBlock.getNumSamples(); ++i)
               dest[i] = scoop.processSample(dest[i]);
        }

        if (*hiGain)
        {
            triode[2].processBlock(processBlock, 1.0, 2.0);
            triode[3].processBlock(processBlock, 1.0, 2.0);
        }

        toneStack->processBlock(processBlock);

        processBlock.multiplyBy(out_raw * 6.0);

        if (*outGainAuto)
            autoGain *= 1.0 / out_raw;

        if (!*hiGain)
            pentode.processBlockClassB(processBlock, 0.6, 0.6);
        else
            pentode.processBlockClassB(processBlock, 0.7, 0.7);

        processBlock.multiplyBy(autoGain);

#if USE_SIMD
        simd.deinterleaveBlock(processBlock);
#endif

        if (*apvts.getRawParameterValue("compPos"))
            comp.processBlock(block, *p_comp, *linked);
    }

private:
#if USE_SIMD
    dsp::IIR::Filter<vec> scoop;
#else
    dsp::IIR::Filter<double> scoop;
#endif
};

struct Channel : Processor
{
    Channel(AudioProcessorValueTreeState &a, strix::VolumeMeterSource &s) : Processor(a, ProcessorType::Channel, s)
    {
        toneStack = nullptr;
        triode.resize(2);
    }

    void prepare(const dsp::ProcessSpec &spec) override
    {
        SR = spec.sampleRate;

        defaultPrepare(spec);

        low.prepare(spec);
        setFilters(0);

        mid.prepare(spec);
        setFilters(1);

        hi.prepare(spec);
        setFilters(2);
    }

    void update(const dsp::ProcessSpec& spec, const float lowGain = 0.5f, const float midGain = 0.5f, const float trebleGain = 0.5f)
    {
        SR = spec.sampleRate;

        defaultPrepare(spec);

        this->lowGain = lowGain;
        this->midGain = midGain;
        this->trebGain = trebleGain;
        updateFilters = true;
    }

    void setFilters(int index, float newValue = 0.5f)
    {
        auto gaindB = jmap(newValue, -6.f, 6.f);
        auto gain = Decibels::decibelsToGain(gaindB);
        switch (index)
        {
        case 0:
            *low.coefficients = *dsp::IIR::Coefficients<double>::makeLowShelf(SR, 250.0, 1.0, gain);
            break;
        case 1: {
            double Q = 0.707;
            Q *= 1.0 / gain;
            *mid.coefficients = *dsp::IIR::Coefficients<double>::makePeakFilter(SR, 900.0, Q, gain);
            }
            break;
        case 2:
            *hi.coefficients = *dsp::IIR::Coefficients<double>::makeHighShelf(SR, 5000.0, 0.5, gain);
            break;
        default:
            break;
        }

        setEQAutoGain();
    }

    template <typename T>
    void processBlock(dsp::AudioBlock<T> &block)
    {
        T gain_raw = jmap(inGain->load(), 1.f, 4.f);
        T out_raw = jmap(outGain->load(), 1.f, 4.f);

#if USE_SIMD
        vec autoGain = 1.0;
#else
        double autoGain = 1.0;
#endif

        if (!*apvts.getRawParameterValue("compPos"))
            comp.processBlock(block, *p_comp, *linked);

        // if (updateSIMD) {
        //     simd.setInterleavedBlockSize(numChannels, numSamples); /*this doesn't appear to be safe after all*/
        //     updateSIMD = false;
        // }

#if USE_SIMD
        auto&& processBlock = simd.interleaveBlock(block);
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

        if (*inGainAuto)
            autoGain *= 1.0 / std::sqrt(std::sqrt(gain_raw * gain_raw * gain_raw));

        processFilters(processBlock);

        if (*eqAutoGain)
            autoGain *= getEQAutoGain();

        if (*outGain > 0.f) {
            processBlock.multiplyBy(out_raw * 6.f);

            if (!*hiGain)
                pentode.processBlockClassB(processBlock, 0.4f, 0.4f);
            else
                pentode.processBlockClassB(processBlock, 0.6f, 0.6f);
        }

        if (*outGainAuto)
            autoGain *= 1.0 / out_raw;

#if USE_SIMD
        processBlock.multiplyBy(xsimd::reduce_max(autoGain));

        simd.deinterleaveBlock(processBlock);
#else
        processBlock.multiplyBy(autoGain);
#endif
        if (*apvts.getRawParameterValue("compPos"))
            comp.processBlock(block, *p_comp, *linked);
    }

private:
#if USE_SIMD
    dsp::IIR::Filter<vec> low, mid, hi;
#else
    dsp::IIR::Filter<double> low, mid, hi;
#endif
    double lowGain = 0.0, midGain = 0.0, trebGain = 0.0;

    std::atomic<bool> updateFilters = false;

    double autoGain_m = 1.0;

    template <class Block>
    void processFilters(Block& block)
    {
        if (updateFilters)
        {
            setFilters(0, lowGain);
            setFilters(1, midGain);
            setFilters(2, trebGain);
            updateFilters = false;
        }

        for (auto ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto in = block.getChannelPointer(ch);

            for (auto i = 0; i < block.getNumSamples(); ++i)
            {
                in[i] = low.processSample(in[i]);
                in[i] = mid.processSample(in[i]);
                in[i] = hi.processSample(in[i]);
            }
        }
    }

    // get magnitude at some specific frequencies and take the reciprocal
    void setEQAutoGain()
    {
        autoGain_m = 1.0;

        auto l_mag = low.coefficients->getMagnitudeForFrequency(300.0, SR);

        auto m_mag = mid.coefficients->getMagnitudeForFrequency(2500.0, SR);

        auto h_mag = hi.coefficients->getMagnitudeForFrequency(2500.0, SR);

        if (l_mag || m_mag || h_mag)
        {
            autoGain_m *= 1.0 / l_mag;
            autoGain_m *= 1.0 / m_mag;
            autoGain_m *= 1.0 / h_mag;
        }
    }

    inline double getEQAutoGain() noexcept { return autoGain_m; }
};

} // namespace Processors