// Enhancer.h

#pragma once
#include "dsp_filters/dsp_filters.h"

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

        block.multiplyBy(jmap(enhance, (T)1.f, (T)4.f));

        tube.processBlock(block, 1.0, 0.5);

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

    void checkForInvalidSamples (const dsp::AudioBlock<double>& blockToCheck)
    {
        auto numChans = blockToCheck.getNumChannels();
        auto numSamps = blockToCheck.getNumSamples();

        for (auto c = 0; c < numChans; ++c)
        {
            for (auto s = 0; s < numSamps; ++s)
            {
                auto sample = blockToCheck.getSample (c, s);
                jassert (!std::isnan (sample));
                // Probably also this ones
                jassert (sample <= 100.0f);
                jassert (sample >= -100.0f);
            }
        }
    }

    void process(dsp::AudioBlock<T>& block, T enhance)
    {
        auto inL = block.getChannelPointer(0);
        auto inR = block.getChannelPointer(1);

        lp1.process(block.getNumSamples(), 0, inL);
        lp1.process(block.getNumSamples(), 1, inR);

        checkForInvalidSamples(block);

        block.multiplyBy(jmap(enhance, -1.0, -4.0));

        tube.processBlock(block, 1.0, 3.0);

        checkForInvalidSamples(block);

        lp2.process(block.getNumSamples(), 0, inL);
        lp2.process(block.getNumSamples(), 1, inR);

        checkForInvalidSamples(block);

        block.multiplyBy(enhance);
    }

    // void processSIMD(chowdsp::AudioBlock<T>& block, double enhance)
    // {
    //     auto in = block.getChannelPointer(0);

    //     lp1.process(block.getNumSamples(), 0, in);

    //     block.multiplyBy(jmap(enhance, -1.0, -4.0));

    //     tube.processBlock(block, 1.0, 3.0);

    //     lp2.process(block.getNumSamples(), 0, in);

    //     block.multiplyBy(enhance);
    // }

    Mode type;

    Dsp::SimpleFilter<Dsp::Bessel::LowPass<4>, 2> lp1, lp2;

    AVTriode<T> tube;

    HeapBlock<char> heap;
    dsp::AudioBlock<T> wetBlock;

    double SR = 0.0;
};
