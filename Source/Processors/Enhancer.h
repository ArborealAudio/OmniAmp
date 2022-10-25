// Enhancer.h

#pragma once

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

    inline static void process(strix::AudioBlock<vec>& block, vec gp, vec gn, vec k)
    {
        for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto in = block.getChannelPointer(ch);
            for (size_t i = 0; i < block.getNumSamples(); ++i)
            {
                in[i] = xsimd::select(in[i] >= 0.0,
                (1.0 / gp) * (in[i] * gp) / xsimd::pow((1.0 + gp * xsimd::pow(xsimd::abs(in[i] * gp), k)), 1.0 / k),
                (1.0 / gn) * (in[i] * gn) / xsimd::pow((1.0 + gn * xsimd::pow(xsimd::abs(in[i] * gn), k)), 1.0 / k));
            }
        }
    }
};

template <typename T>
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

        double freq = 0.0;
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

        auto lp_c = dsp::FilterDesign<double>::designIIRLowpassHighOrderButterworthMethod(freq, spec.sampleRate, 1);
        auto hp_c = dsp::FilterDesign<double>::designIIRHighpassHighOrderButterworthMethod(7500.0, spec.sampleRate, 1);

        lp1.clear();
        lp2.clear();
        hp1.clear();
        hp2.clear();

        for (auto& c : lp_c) {
            lp1.emplace_back(dsp::IIR::Filter<T>(c));
            lp2.emplace_back(dsp::IIR::Filter<T>(c));
        }

        for (auto& c : hp_c) {
            hp1.emplace_back(dsp::IIR::Filter<T>(c));
            hp2.emplace_back(dsp::IIR::Filter<T>(c));
        }

        wetBuffer.setSize(spec.numChannels, spec.maximumBlockSize);
    }

    /*method for updating filters IN SYNC w/ audio thread*/
    void updateFilters()
    {
        double freq = 0.0;
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

        auto lp_c = dsp::FilterDesign<double>::designIIRLowpassHighOrderButterworthMethod(freq, SR, 1);
        lp1.clear();
        lp2.clear();
        for (auto &c : lp_c)
        {
            lp1.emplace_back(dsp::IIR::Filter<T>(c));
            lp2.emplace_back(dsp::IIR::Filter<T>(c));
        }

        needUpdate = false;
    }

    void flagUpdate(bool newFlag)
    {
        needUpdate.store(newFlag);
    }

    void reset()
    {
        if (!lp1.empty())
            lp1[0].reset();
        if (!lp2.empty())
            lp2[0].reset();
        if (!hp1.empty())
            hp1[0].reset();
        if (!hp2.empty())
            hp2[0].reset();
    }

    template <typename Block>
    void processBlock(Block& block, const double enhance, const bool invert)
    {
        if (needUpdate) 
            updateFilters();
        
        wetBuffer.copyFrom(0, 0, block.getChannelPointer(0), block.getNumSamples());
        if (block.getNumChannels() > 1)
            wetBuffer.copyFrom(1, 0, block.getChannelPointer(1), block.getNumSamples());

        auto processBlock = Block(wetBuffer).getSubBlock(0, block.getNumSamples());

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

        auto dest = block.getChannelPointer(0);
        auto src = processBlock.getChannelPointer(0);
        for (auto i = 0; i < block.getNumSamples(); ++i)
        {
            if (invert)
                dest[i] -= src[i];
            else
                dest[i] += src[i];
        }
    }

private:

    template <typename Block>
    void processHF(Block& block, double enhance)
    {
        auto inL = block.getChannelPointer(0);

        for (int i = 0; i < block.getNumSamples(); ++i)
            inL[i] = hp1[0].processSample(inL[i]);
        
        auto gain = jmap(enhance, 1.0, 4.0);
        double autoGain = 1.0;

        block.multiplyBy(gain);

        EnhancerSaturation::process(block, 1.0, 1.0, 1.0);
        block.multiplyBy(2.0);

        if (*hfAutoGain)
            autoGain *= 1.0 / (2.0 * gain);

        for (int i = 0; i < block.getNumSamples(); ++i)
            inL[i] = hp2[0].processSample(inL[i]);

        block.multiplyBy(enhance * autoGain);
    }

    template <typename Block>
    void processLF(Block& block, double enhance)
    {
        auto inL = block.getChannelPointer(0);
        
        for (int i = 0; i < block.getNumSamples(); ++i)
            inL[i] = lp1[0].processSample(inL[i]);

        auto gain = jmap(enhance, 1.0, 2.0);
        double autoGain = 1.0;

        block.multiplyBy(gain);

        EnhancerSaturation::process(block, 1.0, 2.0, 4.0);

        if (*lfAutoGain)
            autoGain *= 1.0 / (6.0 * gain);

        for (int i = 0; i < block.getNumSamples(); ++i)
            inL[i] = lp2[0].processSample(inL[i]);

        block.multiplyBy(enhance * autoGain);
    }

    double SR = 44100.0;
    std::atomic<bool> needUpdate = false;

    Type type;
    Processors::ProcessorType mode;

    AudioProcessorValueTreeState &apvts;
    std::atomic<float> *hfAutoGain, *lfAutoGain;

    std::vector<dsp::IIR::Filter<T>> lp1, lp2, hp1, hp2;
#if USE_SIMD
    strix::Buffer<T> wetBuffer;
#else
    AudioBuffer<T> wetBuffer;
#endif
};
