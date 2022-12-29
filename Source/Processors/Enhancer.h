// Enhancer.h

#pragma once
#define NU_ENHANCE 1
namespace EnhancerSaturation
{
    // higher values of k = harder clipping
    // lower values can attenuate the signal a bit
    inline void process(dsp::AudioBlock<double> &block, double gp, double gn)
    {
        for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto in = block.getChannelPointer(ch);
            for (size_t i = 0; i < block.getNumSamples(); ++i)
            {
#if NU_ENHANCE
                if (in[i] >= 0.0)
                    in[i] = (1.0 / gp) * strix::fast_tanh(in[i] * gp);
                else
                    in[i] = (1.0 / gn) * strix::fast_tanh(in[i] * gn);
#else
                if (in[i] >= 0.0)
                    in[i] = (1.0 / gp) * (in[i] * gp) / std::pow((1.0 + gp * std::pow(std::abs(in[i] * gp), k)), 1.0 / k);
                else
                    in[i] = (1.0 / gn) * (in[i] * gn) / std::pow((1.0 + gn * std::pow(std::abs(in[i] * gn), k)), 1.0 / k);
#endif
            }
        }
    }

    inline void process(strix::AudioBlock<vec> &block, vec gp, vec gn)
    {
        for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto in = block.getChannelPointer(ch);
            for (size_t i = 0; i < block.getNumSamples(); ++i)
            {
#if NU_ENHANCE
                in[i] = xsimd::select(in[i] >= 0.0,
                                      (1.0 / gp) * strix::fast_tanh(in[i] * gp),
                                      (1.0 / gn) * strix::fast_tanh(in[i] * gn));
#else
                in[i] = xsimd::select(in[i] >= 0.0,
                                      (1.0 / gp) * (in[i] * gp) / xsimd::pow((1.0 + gp * xsimd::pow(xsimd::abs(in[i] * gp), k)), 1.0 / k),
                                      (1.0 / gn) * (in[i] * gn) / xsimd::pow((1.0 + gn * xsimd::pow(xsimd::abs(in[i] * gn), k)), 1.0 / k));
#endif
            }
        }
    }
};

enum EnhancerType
{
    LF,
    HF
};

template <typename T, EnhancerType type>
struct Enhancer
{
    Enhancer(AudioProcessorValueTreeState &a) : apvts(a)
    {
        lfAutoGain = apvts.getRawParameterValue("lfEnhanceAuto");
        hfAutoGain = apvts.getRawParameterValue("hfEnhanceAuto");
    }

    void setMode(Processors::ProcessorType newMode)
    {
        mode = newMode;
    }

    void prepare(const dsp::ProcessSpec &spec)
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

        for (auto &c : lp_c)
        {
            lp1.emplace_back(dsp::IIR::Filter<T>(c));
            lp2.emplace_back(dsp::IIR::Filter<T>(c));
        }

        for (auto &c : hp_c)
        {
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
    void processBlock(Block &block, const double enhance, const bool invert)
    {
        if (needUpdate)
            updateFilters();

        wetBuffer.copyFrom(0, 0, block.getChannelPointer(0), block.getNumSamples());
        if (block.getNumChannels() > 1)
            wetBuffer.copyFrom(1, 0, block.getChannelPointer(1), block.getNumSamples());

        auto processBlock = Block(wetBuffer).getSubBlock(0, block.getNumSamples());

        if (type == EnhancerType::LF)
            processLF(processBlock, enhance);
        else
            processHF(processBlock, enhance);

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
    void processHF(Block &block, double enhance)
    {
        auto gain = jmap(enhance, 1.0, 4.0);
        double autoGain = 1.0;

        auto *in = block.getChannelPointer(0);

        for (int i = 0; i < block.getNumSamples(); ++i)
            in[i] = hp1[0].processSample(in[i]);
        strix::SmoothGain<T>::applySmoothGain(in, block.getNumSamples(), gain, lastGain);

        EnhancerSaturation::process(block, 1.0, 1.0);
        block.multiplyBy(2.0);

        if ((bool)*hfAutoGain)
            autoGain *= 1.0 / (2.0 * gain);

        for (int i = 0; i < block.getNumSamples(); ++i)
            in[i] = hp2[0].processSample(in[i]);
        strix::SmoothGain<T>::applySmoothGain(in, block.getNumSamples(), enhance * autoGain, lastAutoGain);
    }

    template <typename Block>
    void processLF(Block &block, double enhance)
    {
        auto inL = block.getChannelPointer(0);

        for (int i = 0; i < block.getNumSamples(); ++i)
            inL[i] = lp1[0].processSample(inL[i]);

        auto gain = jmap(enhance, 1.0, 2.0);
        double autoGain = 1.0;

        strix::SmoothGain<T>::applySmoothGain(inL, block.getNumSamples(), gain, lastGain);

        EnhancerSaturation::process(block, 1.0, 2.0);

        if (*lfAutoGain)
            autoGain *= 1.0 / (6.0 * gain);

        for (int i = 0; i < block.getNumSamples(); ++i)
            inL[i] = lp2[0].processSample(inL[i]);

        strix::SmoothGain<T>::applySmoothGain(inL, block.getNumSamples(), enhance * autoGain, lastAutoGain);
    }

    double SR = 44100.0;
    std::atomic<bool> needUpdate = false;

    Processors::ProcessorType mode;

    AudioProcessorValueTreeState &apvts;
    std::atomic<float> *hfAutoGain, *lfAutoGain;

    double lastGain = 0.0, lastAutoGain = 1.0;

    std::vector<dsp::IIR::Filter<T>> lp1, lp2, hp1, hp2;
#if USE_SIMD
    strix::Buffer<T> wetBuffer;
#else
    AudioBuffer<T> wetBuffer;
#endif
};

//sticking this here bc i don't want to make a new file

/**
 * a wrapper for low- and high-cut filters w/ param smoothing
*/
struct CutFilters
{
    CutFilters(AudioProcessorValueTreeState &a) : apvts(a)
    {
        lfParam = static_cast<strix::FloatParameter*>(apvts.getParameter("lfCut"));
        hfParam = static_cast<strix::FloatParameter*>(apvts.getParameter("hfCut"));
    }

    void prepare(const dsp::ProcessSpec &spec)
    {
        lfCut.prepare(spec);
        hfCut.prepare(spec);
        lfCut.setType(strix::FilterType::highpass);
        hfCut.setType(strix::FilterType::lowpass);
        lfCut.setCutoffFreq(*apvts.getRawParameterValue("lfCut"));
        hfCut.setCutoffFreq(*apvts.getRawParameterValue("hfCut"));

        cutoffLo.reset(spec.maximumBlockSize);
        cutoffHi.reset(spec.maximumBlockSize);
    }

    void reset()
    {
        lfCut.reset();
        hfCut.reset();
    }

    void process(dsp::AudioBlock<double> &block)
    {
        float lfParam_ = *lfParam;
        float hfParam_ = *hfParam;
        cutoffLo.setTargetValue(lfParam_);
        cutoffHi.setTargetValue(hfParam_);

        if (cutoffLo.isSmoothing())
        {
            for (size_t i = 0; i < block.getNumSamples(); ++i)
            {
                lfCut.setCutoffFreq(cutoffLo.getNextValue());
                for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
                {
                    auto &in = block.getChannelPointer(ch)[i];
                    in = lfCut.processSample(ch, in);
                }
            }
        }
        else
        {
            lfCut.setCutoffFreq(cutoffLo.getNextValue());
            if (lfParam_ > 5.f)
                lfCut.processBlock(block);
        }
        if (cutoffHi.isSmoothing())
        {
            for (size_t i = 0; i < block.getNumSamples(); ++i)
            {
                hfCut.setCutoffFreq(cutoffHi.getNextValue());
                for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
                {
                    auto &in = block.getChannelPointer(ch)[i];
                    in = hfCut.processSample(ch, in);
                }
            }
        }
        else
        {
            hfCut.setCutoffFreq(cutoffHi.getNextValue());
            if (hfParam_ < 22000.f)
                hfCut.processBlock(block);
        }
    }

private:
    strix::SVTFilter<double> lfCut, hfCut;
    AudioProcessorValueTreeState &apvts;
    SmoothedValue<float> cutoffLo, cutoffHi;
    strix::FloatParameter *lfParam, *hfParam;
};