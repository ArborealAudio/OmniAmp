// Enhancer.h

#pragma once
#include "dsp_filters/dsp_filters.h"

template <typename Type>
struct HFEnhancer
{
    HFEnhancer(){}
    ~HFEnhancer(){}

    void prepare(const dsp::ProcessSpec& spec)
    {
        hp1.setup(1, spec.sampleRate, 7500.0);
        hp2.setup(2, spec.sampleRate, 7500.0);

        tube.prepare(spec);

        wetBuffer.setSize(spec.numChannels, spec.maximumBlockSize);
    }

    void reset()
    {
        hp1.reset();
        hp2.reset();
        tube.reset();
    }

    void processBuffer(AudioBuffer<Type>& buffer, const Type enhance)
    {
        wetBuffer.makeCopyOf(buffer, true);

        hp1.process(wetBuffer.getNumSamples(), wetBuffer.getArrayOfWritePointers());

        wetBuffer.applyGain(jmap(enhance, 1.f, 4.f));

        tube.process(wetBuffer, 1.0, 0.5);

        hp2.process(wetBuffer.getNumSamples(), wetBuffer.getArrayOfWritePointers());

        buffer.addFrom(0, 0, wetBuffer.getReadPointer(0), wetBuffer.getNumSamples(), enhance);
        if (buffer.getNumChannels() > 1)
            buffer.addFrom(1, 0, wetBuffer.getReadPointer(1), wetBuffer.getNumSamples(), enhance);
    }

    template <typename T>
    void processBlock(dsp::AudioBlock<T>& block, const T enhance)
    {
        int size = (int)block.getNumChannels() * (int)block.getNumSamples();
        HeapBlock<char> heap{size};
        dsp::AudioBlock<T> wetBlock(heap, block.getNumChannels(), block.getNumSamples());

        wetBlock.copyFrom(block);

        auto inL = wetBlock.getChannelPointer(0);
        auto inR = wetBlock.getChannelPointer(1);

        hp1.process(wetBlock.getNumSamples(), 0, inL);
        hp1.process(wetBlock.getNumSamples(), 1, inR);

        wetBlock.multiplyBy(jmap(enhance, (T)1.f, (T)4.f));

        tube.processBlock(wetBlock, 1.0, 0.5);

        hp2.process(wetBlock.getNumSamples(), 0, inL);
        hp2.process(wetBlock.getNumSamples(), 1, inR);

        wetBlock.multiplyBy(enhance);

        block.add(wetBlock);
    }

private:
    Dsp::SimpleFilter<Dsp::Bessel::HighPass<4>, 2> hp1, hp2;

    AudioBuffer<Type> wetBuffer;

    AVTriode<Type> tube;
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

        wetBuffer.setSize(spec.numChannels, spec.maximumBlockSize);
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

    void processBuffer(AudioBuffer<float>& buffer, const float enhance)
    {
        wetBuffer.makeCopyOf(buffer, true);

        lp1.process(wetBuffer.getNumSamples(), wetBuffer.getArrayOfWritePointers());

        wetBuffer.applyGain(jmap(enhance, -1.f, -4.f));

        tube.processBlock(dsp::AudioBlock<float>(wetBuffer), 1.0, 3.0);

        lp2.process(wetBuffer.getNumSamples(), wetBuffer.getArrayOfWritePointers());

        buffer.addFrom(0, 0, wetBuffer.getReadPointer(0), wetBuffer.getNumSamples(), enhance);
        if (buffer.getNumChannels() > 1)
            buffer.addFrom(1, 0, wetBuffer.getReadPointer(1), wetBuffer.getNumSamples(), enhance);
    }

    void processBlock(dsp::AudioBlock<T>& block, const T enhance)
    {
        int size = (int)block.getNumChannels() * (int)block.getNumSamples();
        HeapBlock<char> heap{size};
        dsp::AudioBlock<T> wetBlock(heap, block.getNumChannels(), block.getNumSamples());

        wetBlock.copyFrom(block);

        auto inL = wetBlock.getChannelPointer(0);
        auto inR = wetBlock.getChannelPointer(1);

        lp1.process(wetBlock.getNumSamples(), 0, inL);
        lp1.process(wetBlock.getNumSamples(), 1, inR);

        wetBlock.multiplyBy(jmap(enhance, (T)-1.f, (T)-4.f));

        tube.processBlock(wetBlock, 1.0, 3.0);

        lp2.process(wetBlock.getNumSamples(), 0, inL);
        lp2.process(wetBlock.getNumSamples(), 1, inR);

        wetBlock.multiplyBy(enhance);
        block.add(wetBlock);
    }

private:

    Mode type;

    Dsp::SimpleFilter<Dsp::Bessel::LowPass<4>, 2> lp1, lp2;

    AudioBuffer<T> wetBuffer;

    AVTriode<T> tube;

    double SR = 0.0;
};
