// Enhancer.h

#pragma once
#include "dsp_filters/dsp_filters.h"

struct EnhancerSaturation
{
    // higher values of k = harder clipping
    // lower values can de-amplify the signal a bit
    static void process(dsp::AudioBlock<double>& block, double gp, double gn, double k)
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

    Enhancer(Type t) : type(t)
    {
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
            freq = 250.0;
            break;
        case Processors::ProcessorType::Bass:
            freq = 150.0;
            break;
        case Processors::ProcessorType::Channel:
            freq = 100.0;
            break;
        }

        lp1.setup(1, spec.sampleRate, freq);
        lp2.setup(1, spec.sampleRate, freq);

        hp1.setup(1, spec.sampleRate, 7500.0);
        hp2.setup(2, spec.sampleRate, 7500.0);

        wetBuffer.setSize(spec.numChannels, spec.maximumBlockSize);
    }

    void updateFilters()
    {
        double freq;
        switch (mode)
        {
        case Processors::ProcessorType::Guitar:
            freq = 250.0;
            break;
        case Processors::ProcessorType::Bass:
            freq = 150.0;
            break;
        case Processors::ProcessorType::Channel:
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
        hp1.reset();
        hp2.reset();
    }

    void processBlock(dsp::AudioBlock<double>& block, const double enhance, bool mono)
    {
        wetBuffer.copyFrom(0, 0, block.getChannelPointer(0), block.getNumSamples());
        if (wetBuffer.getNumChannels() > 1)
            wetBuffer.copyFrom(1, 0, block.getChannelPointer(1), block.getNumSamples());

        auto processBlock = dsp::AudioBlock<double>(wetBuffer);

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
        // hp1.processBlock(block);

        block.multiplyBy(jmap(enhance, 1.0, 4.0));

        EnhancerSaturation::process(block, 1.0, 3.0, 1.0);
        block.multiplyBy(2.0);

        hp2.process(block.getNumSamples(), 0, inL);
        if (block.getNumChannels() > 1)
            hp2.process(block.getNumSamples(), 1, inR);
        // hp2.processBlock(block);

        block.multiplyBy(enhance);
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

        block.multiplyBy(jmap(enhance, 1.0, 4.0));

        EnhancerSaturation::process(block, 1.0, 2.0, 4.0);

        lp2.process(block.getNumSamples(), 0, inL);
        if (block.getNumChannels() > 1)
            lp2.process(block.getNumSamples(), 1, inR);
        // lp2.processBlock(block);

        block.multiplyBy(enhance);
    }

    double SR = 44100.0;

    Type type;
    Processors::ProcessorType mode;

    Dsp::SimpleFilter<Dsp::Bessel::LowPass<4>, 2> lp1, lp2;
    Dsp::SimpleFilter<Dsp::Bessel::HighPass<4>, 2> hp1, hp2;

    AudioBuffer<double> wetBuffer;
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
        // hp1.prepare(spec);
        // hp2.prepare(spec);

        // hp1.setType(strix::FilterType::firstOrderHighpass);
        // hp1.setCutoffFreq(7500.0);

        // hp2.setType(strix::FilterType::highpass);
        // hp2.setCutoffFreq(7500.0);

        wetBlock = dsp::AudioBlock<double>(heap, spec.numChannels, spec.maximumBlockSize);
    }

    void reset()
    {
        hp1.reset();
        hp2.reset();
    }

    void processBlock(dsp::AudioBlock<T>& block, const double enhance)
    {
        wetBlock.copyFrom(block);

        auto subBlock = wetBlock.getSubBlock(0, block.getNumSamples());

        process(subBlock, enhance);

        FloatVectorOperations::add(block.getChannelPointer(0), subBlock.getChannelPointer(0), block.getNumSamples());
        // if (block.getNumChannels() > 1)
        //     FloatVectorOperations::add(block.getChannelPointer(1), subBlock.getChannelPointer(1), block.getNumSamples());
    }

private:

    void process(dsp::AudioBlock<T>& block, T enhance)
    {
        auto inL = block.getChannelPointer(0);
        auto inR = inL;
        if (block.getNumChannels() > 1)
            inR = block.getChannelPointer(1);

        hp1.process(block.getNumSamples(), 0, inL);
        if (block.getNumChannels() > 1)
            hp1.process(block.getNumSamples(), 1, inR);
        // hp1.processBlock(block);

        block.multiplyBy(jmap(enhance, 1.0, 4.0));

        EnhancerSaturation::process(block, 1.0, 3.0, 1.0);
        block.multiplyBy(2.0);

        hp2.process(block.getNumSamples(), 0, inL);
        if (block.getNumChannels() > 1)
            hp2.process(block.getNumSamples(), 1, inR);
        // hp2.processBlock(block);

        block.multiplyBy(enhance);
    }

    Dsp::SimpleFilter<Dsp::Bessel::HighPass<4>, 2> hp1, hp2;
    // strix::SVTFilter<double> hp1, hp2;

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
        // lp1.prepare(spec);
        // lp1.setType(strix::FilterType::firstOrderLowpass);
        // lp1.setCutoffFreq(freq);

        // lp2.prepare(spec);
        // lp2.setType(strix::FilterType::firstOrderLowpass);
        // lp2.setCutoffFreq(freq);

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

        // lp1.setCutoffFreq(freq);
        // lp2.setCutoffFreq(freq);
    }

    void reset()
    {
        lp1.reset();
        lp2.reset();
    }

    void processBlock(dsp::AudioBlock<T>& block, const double enhance)
    {
        wetBlock.copyFrom(block);

        auto subBlock = wetBlock.getSubBlock(0, block.getNumSamples());

        process(subBlock, enhance);

        // block.add(subBlock);
    }

private:
    void process(dsp::AudioBlock<T>& block, T enhance)
    {
        auto inL = block.getChannelPointer(0);
        auto inR = inL;
        if (block.getNumChannels() > 1)
            inR = block.getChannelPointer(1);

        lp1.process(block.getNumSamples(), 0, inL);
        if (block.getNumChannels() > 1)
            lp1.process(block.getNumSamples(), 1, inR);
        // lp1.processBlock(block);

        block.multiplyBy(jmap(enhance, 1.0, 4.0));

        EnhancerSaturation::process(block, 1.0, 2.0, 4.0);

        lp2.process(block.getNumSamples(), 0, inL);
        if (block.getNumChannels() > 1)
            lp2.process(block.getNumSamples(), 1, inR);
        // lp2.processBlock(block);

        block.multiplyBy(enhance);
    }

    Mode type;

    Dsp::SimpleFilter<Dsp::Bessel::LowPass<4>, 2> lp1, lp2;
    // strix::SVTFilter<double> lp1, lp2;

    HeapBlock<char> heap;
    dsp::AudioBlock<T> wetBlock;

    double SR = 0.0;
};
