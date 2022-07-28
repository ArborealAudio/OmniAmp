// Enhancer.h

#pragma once
#include "dsp_filters/dsp_filters.h"

struct EnhancerSaturation
{
    // higher values of k = harder clipping
    // lower values can de-amplify the signal a bit
    template <class Block>
    static void process(Block& block, double gp, double gn, double k)
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

template <typename T>
struct HFEnhancer
{
    HFEnhancer(){}
    ~HFEnhancer(){}

    void prepare(const dsp::ProcessSpec& spec)
    {
        hp1.setup(1, spec.sampleRate, 7500.0);
        hp2.setup(2, spec.sampleRate, 7500.0);

        tube.prepare(spec);

        wetBlock = dsp::AudioBlock<double>(heap, spec.numChannels, spec.maximumBlockSize);
    }

    void reset()
    {
        hp1.reset();
        hp2.reset();
        tube.reset();
    }

    void processBlock(dsp::AudioBlock<T>& block, const double enhance)
    {
        wetBlock.copyFrom(block);

        auto subBlock = wetBlock.getSubBlock(0, block.getNumSamples());

        process(subBlock, enhance);

        block.add(subBlock);
    }

private:

    void process(dsp::AudioBlock<T>& block, T enhance)
    {
        auto inL = block.getChannelPointer(0);
        auto inR = block.getChannelPointer(1);

        hp1.process(block.getNumSamples(), 0, inL);
        hp1.process(block.getNumSamples(), 1, inR);

        block.multiplyBy(jmap(enhance, 1.0, 4.0));

        EnhancerSaturation::process(block, 1.0, 3.0, 1.0);
        block.multiplyBy(2.0);

        hp2.process(block.getNumSamples(), 0, inL);
        hp2.process(block.getNumSamples(), 1, inR);

        block.multiplyBy(enhance);
    }

    Dsp::SimpleFilter<Dsp::Bessel::HighPass<4>, 2> hp1, hp2;

    AVTriode<T> tube;

    HeapBlock<char> heap;
    dsp::AudioBlock<T> wetBlock;
};

template <typename T>
struct LFEnhancer
{
    LFEnhancer(){}
    ~LFEnhancer(){}

    enum Mode
    {
        Guitar,
        Bass,
        Channel
    };

    void prepare(const dsp::ProcessSpec& spec)
    {
        SR = spec.sampleRate;

        double freq;
        switch (type)
        {
        case Guitar:
            freq = 250.0;
            break;
        case Bass:
            freq = 150.0;
            break;
        case Channel:
            freq = 100.0;
            break;
        }

        lp1.setup(1, spec.sampleRate, freq);
        lp2.setup(1, spec.sampleRate, freq);

        tube.prepare(spec);

        wetBlock = dsp::AudioBlock<T>(heap, spec.numChannels, spec.maximumBlockSize);
    }

    void setType(Mode newType)
    {
        type = newType;
    }

    void updateFilters()
    {
        double freq;
        switch (type)
        {
        case Guitar:
            freq = 250.0;
            break;
        case Bass:
            freq = 150.0;
            break;
        case Channel:
            freq = 100.0;
            break;
        }

        lp1.setup(1, SR, freq);
        lp2.setup(1, SR, freq);
    }

    void reset()
    {
        lp1.reset();
        lp2.reset();
        tube.reset();
    }

    void processBlock(dsp::AudioBlock<T>& block, const double enhance)
    {
        wetBlock.copyFrom(block);

        auto subBlock = wetBlock.getSubBlock(0, block.getNumSamples());

        process(subBlock, enhance);

        block.add(subBlock);
    }

private:
    void process(dsp::AudioBlock<T>& block, T enhance)
    {
        auto inL = block.getChannelPointer(0);
        auto inR = block.getChannelPointer(1);

        lp1.process(block.getNumSamples(), 0, inL);
        lp1.process(block.getNumSamples(), 1, inR);

        block.multiplyBy(jmap(enhance, 1.0, 4.0));

        EnhancerSaturation::process(block, 1.0, 2.0, 4.0);

        lp2.process(block.getNumSamples(), 0, inL);
        lp2.process(block.getNumSamples(), 1, inR);

        block.multiplyBy(enhance);
    }

    Mode type;

    Dsp::SimpleFilter<Dsp::Bessel::LowPass<4>, 2> lp1, lp2;

    AVTriode<T> tube;

    HeapBlock<char> heap;
    dsp::AudioBlock<T> wetBlock;

    double SR = 0.0;
};
