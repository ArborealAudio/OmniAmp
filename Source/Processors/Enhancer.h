// Enhancer.h

#pragma once
#include "dsp_filters/dsp_filters.h"

struct EnhancerSaturation
{
    // higher values of k = harder clipping
    // lower values can attenuate the signal a bit
    inline static void process(dsp::AudioBlock<double>& block, double gp, double gn, double k)
    {
        for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto in = block.getChannelPointer(ch);
            for (size_t i = 0; i < block.getNumSamples(); ++i)
            {
                if (in[i] >= 0.0)
                    in[i] = (1.0 / gp) * (in[i] * gp) / std::pow((1.0 + gp * std::pow(std::abs(in[i] * gp), k)), 1.0 / k);
                else
                    in[i] = (1.0 / gn) * (in[i] * gn) / std::pow((1.0 + gn * std::pow(std::abs(in[i] * gn), k)), 1.0 / k);
            }
        }
    }
};

struct Enhancer
{
    enum Type
    {
        LF,
        HF
    };

    Enhancer(AudioProcessorValueTreeState& a, Type t) : type(t), apvts(a)
    {
        lfAutoGain = apvts.getRawParameterValue("lfEnhanceAuto");
        hfAutoGain = apvts.getRawParameterValue("hfEnhanceAuto");
    }

    void setMode(Processors::ProcessorType newMode)
    {
        mode = newMode;
    }

    void prepare(const dsp::ProcessSpec& spec)
    {
        SR = spec.sampleRate;

        double freq;
        switch (mode)
        {
        case Processors::ProcessorType::Guitar:
            freq = 300.0;
            break;
        case Processors::ProcessorType::Bass:
            freq = 175.0;
            break;
        case Processors::ProcessorType::Channel:
            freq = 200.0;
            break;
        }

        lp1.setup(1, spec.sampleRate, freq);
        lp2.setup(1, spec.sampleRate, freq);

        hp1.setup(1, spec.sampleRate, 7500.0);
        hp2.setup(1, spec.sampleRate, 7500.0);

        wetBuffer.setSize(spec.numChannels, spec.maximumBlockSize);
    }

    void updateFilters()
    {
        double freq;
        switch (mode)
        {
        case Processors::ProcessorType::Guitar:
            freq = 300.0;
            break;
        case Processors::ProcessorType::Bass:
            freq = 175.0;
            break;
        case Processors::ProcessorType::Channel:
            freq = 200.0;
            break;
        }

        lp1.setup(1, SR, freq);
        lp2.setup(1, SR, freq);
    }

    void reset()
    {
        lp1.reset();
        lp2.reset();
        hp1.reset();
        hp2.reset();
    }

    void processBlock(dsp::AudioBlock<double>& block, const double enhance, bool mono)
    {
        wetBuffer.copyFrom(0, 0, block.getChannelPointer(0), block.getNumSamples());
        if (!mono)
            wetBuffer.copyFrom(1, 0, block.getChannelPointer(1), block.getNumSamples());

        auto processBlock = dsp::AudioBlock<double>(wetBuffer).getSubBlock(0, block.getNumSamples());

        switch (type)
        {
        case LF:
            processLF(processBlock, enhance);
            break;
        case HF:
            processHF(processBlock, enhance);
            break;
        default:
            break;
        }

        FloatVectorOperations::add(block.getChannelPointer(0), processBlock.getChannelPointer(0), block.getNumSamples());
        if (!mono)
            FloatVectorOperations::add(block.getChannelPointer(1), processBlock.getChannelPointer(1), block.getNumSamples());
    }

private:

    void processHF(dsp::AudioBlock<double>& block, double enhance)
    {
        auto inL = block.getChannelPointer(0);
        auto inR = inL;
        if (block.getNumChannels() > 1)
            inR = block.getChannelPointer(1);

        hp1.process(block.getNumSamples(), 0, inL);
        if (block.getNumChannels() > 1)
            hp1.process(block.getNumSamples(), 1, inR);

        auto gain = jmap(enhance, 1.0, 4.0);
        double autoGain = 1.0;

        block.multiplyBy(gain);

        EnhancerSaturation::process(block, 1.0, 3.0, 1.0);
        block.multiplyBy(2.0);

        if (*hfAutoGain)
            autoGain *= 1.0 / (2.0 * gain);

        hp2.process(block.getNumSamples(), 0, inL);
        if (block.getNumChannels() > 1)
            hp2.process(block.getNumSamples(), 1, inR);

        block.multiplyBy(enhance * autoGain);
    }

    void processLF(dsp::AudioBlock<double>& block, double enhance)
    {
        auto inL = block.getChannelPointer(0);
        auto inR = inL;
        if (block.getNumChannels() > 1)
            inR = block.getChannelPointer(1);

        lp1.process(block.getNumSamples(), 0, inL);
        if (block.getNumChannels() > 1)
            lp1.process(block.getNumSamples(), 1, inR);
        // lp1.processBlock(block);

        auto gain = jmap(enhance, 1.0, 4.0);
        double autoGain = 1.0;

        block.multiplyBy(gain);

        EnhancerSaturation::process(block, 1.0, 2.0, 4.0);

        if (*lfAutoGain)
            autoGain *= 1.0 / (6.0 * gain);

        lp2.process(block.getNumSamples(), 0, inL);
        if (block.getNumChannels() > 1)
            lp2.process(block.getNumSamples(), 1, inR);
        // lp2.processBlock(block);

        block.multiplyBy(enhance * autoGain);
    }

    double SR = 44100.0;

    Type type;
    Processors::ProcessorType mode;

    AudioProcessorValueTreeState &apvts;
    std::atomic<float> *hfAutoGain, *lfAutoGain;

    Dsp::SimpleFilter<Dsp::Butterworth::LowPass<4>, 2> lp1, lp2;
    Dsp::SimpleFilter<Dsp::Butterworth::HighPass<4>, 2> hp1, hp2;

    AudioBuffer<double> wetBuffer;
};
