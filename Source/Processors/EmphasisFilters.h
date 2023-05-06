/**
 * EmphasisFilters.h
 * for managing emphasis filters
 */

#pragma once

#include <JuceHeader.h>

enum EmphasisFilterType
{
    Low,
    High
};

template <typename T, EmphasisFilterType type = Low>
struct EmphasisFilter : AudioProcessorValueTreeState::Listener
{
    EmphasisFilter(AudioProcessorValueTreeState &_apvts, strix::FloatParameter *_amount, strix::FloatParameter *_freq)
                    : apvts(_apvts), amount(_amount), freq(_freq)
    {
        apvts.addParameterListener(amount->getParameterID(), this);
        apvts.addParameterListener(freq->getParameterID(), this);
    }

    ~EmphasisFilter()
    {
        apvts.removeParameterListener(amount->getParameterID(), this);
        apvts.removeParameterListener(freq->getParameterID(), this);
    }

    void parameterChanged(const String &paramID, float newValue) override
    {
        if (paramID == amount->getParameterID())
            sm_amt.setTargetValue(newValue);
        else if (paramID == freq->getParameterID())
            sm_freq.setTargetValue(newValue);
    }

    void prepare(const dsp::ProcessSpec &spec)
    {
        SR = spec.sampleRate;
        float freq_ = *freq;
        if (freq_ >= SR * 0.5)
            freq_ = SR * 0.5;
        lastFreq = freq_;
        float amount_ = Decibels::decibelsToGain(amount->get());
        float namount_ = Decibels::decibelsToGain(-amount->get());

        for (size_t i = 0; i < 2; ++i)
        {
            fIn[i].prepare(spec);
            fOut[i].prepare(spec);
            if (type == Low)
            {
                fIn[i].coefficients = dsp::IIR::Coefficients<double>::makeLowShelf(SR, freq_, 0.707, amount_);
                fOut[i].coefficients = dsp::IIR::Coefficients<double>::makeLowShelf(SR, freq_, 0.707, namount_);
            }
            else
            {
                fIn[i].coefficients = dsp::IIR::Coefficients<double>::makeHighShelf(SR, freq_, 0.707, amount_);
                fOut[i].coefficients = dsp::IIR::Coefficients<double>::makeHighShelf(SR, freq_, 0.707, namount_);
            }
        }

        sm_amt.reset(spec.sampleRate, 0.01f);
        sm_freq.reset(spec.sampleRate, 0.01f);
        sm_amt.setCurrentAndTargetValue(amount_);
        sm_freq.setCurrentAndTargetValue(freq_);
    }

    // inOut: 0 = input filter, 1 = output filter
    // controls whether to update the smoother or just read from it
    void updateFilters(int inOut)
    {
        float freq_ = inOut ? sm_freq.getNextValue() : sm_freq.getCurrentValue();
        if (freq_ > SR * 0.5)
            freq_ = SR * 0.5;
        float amt = inOut ? sm_amt.getNextValue() : sm_amt.getCurrentValue();
        float amount_ = Decibels::decibelsToGain(amt);
        float namount_ = Decibels::decibelsToGain(-amt);

        for (size_t i = 0; i < 2; ++i)
        {
            if (type == Low)
            {
                *fIn[i].coefficients = dsp::IIR::ArrayCoefficients<double>::makeLowShelf(SR, freq_, 0.707, amount_);
                *fOut[i].coefficients = dsp::IIR::ArrayCoefficients<double>::makeLowShelf(SR, freq_, 0.707, namount_);
            }
            else
            {
                *fIn[i].coefficients = dsp::IIR::ArrayCoefficients<double>::makeHighShelf(SR, freq_, 0.707, amount_);
                *fOut[i].coefficients = dsp::IIR::ArrayCoefficients<double>::makeHighShelf(SR, freq_, 0.707, namount_);
            }
        }
    }

    void reset()
    {
        fIn[0].reset();
        fIn[1].reset();
        fOut[0].reset();
        fOut[1].reset();
    }

    template <typename Block>
    void processIn(Block &block)
    {
        if (sm_freq.isSmoothing() || sm_amt.isSmoothing())
        {
            for (size_t i = 0; i < block.getNumSamples(); ++i)
            {
                updateFilters(0);
                for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
                {
                    auto in = block.getChannelPointer(ch);
                    in[i] = fIn[ch].processSample(in[i]);
                }
            }
            return;
        }
        for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto *in = block.getChannelPointer(ch);
            for (size_t i = 0; i < block.getNumSamples(); ++i)
            {
                in[i] = fIn[ch].processSample(in[i]);
            }
        }
    }

    template <typename Block>
    void processOut(Block &block)
    {
        if (sm_freq.isSmoothing() || sm_amt.isSmoothing())
        {
            for (size_t i = 0; i < block.getNumSamples(); ++i)
            {
                updateFilters(1);
                for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
                {
                    auto in = block.getChannelPointer(ch);
                    in[i] = fOut[ch].processSample(in[i]);
                }
            }
            return;
        }
        for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto *in = block.getChannelPointer(ch);
            for (size_t i = 0; i < block.getNumSamples(); ++i)
            {
                in[i] = fOut[ch].processSample(in[i]);
            }
        }
        // lastAmount = *amount;
        // lastFreq = *freq;
    }

    strix::FloatParameter *amount, *freq;

private:
    AudioProcessorValueTreeState &apvts;
    dsp::IIR::Filter<T> fIn[2], fOut[2];
    SmoothedValue<float> sm_freq, sm_amt;
    double SR = 44100.0;
    float lastFreq = 1.f, lastAmount = 1.f;
};