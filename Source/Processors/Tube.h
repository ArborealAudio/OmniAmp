/*
  ==============================================================================

    Tube.h
    Created: 25 Jan 2022 5:48:43pm
    Author:  alexb

  ==============================================================================
*/

#pragma once

enum PentodeType
{
    Classic,
    Nu
};

struct bias_t
{
    double first = 1.0;
    double second = 1.0;
};

/**
 * A tube emulator for Class B simulation, with option for a dynamic bias & different saturation algorithms
 */
template <typename T>
struct Pentode
{
    Pentode() = default;

    /**
     * @param type Pentode Type. Use widely asymmetric values in Classic mode to get the intended effect
     */
    Pentode(PentodeType type) : type(type)
    {
    }

    void setType(const PentodeType newType)
    {
        type = newType;
        if (type)
        {
            bpPre.setCutoffFreq(2000.0);
            bpPre.setResonance(0.2);
            bpPost.setCutoffFreq(2500.0);
            bpPost.setResonance(0.2);
        }
        else
        {
            bpPre.setCutoffFreq(1000.0);
            bpPre.setResonance(0.1);
            bpPost.setCutoffFreq(1000.0);
            bpPost.setResonance(0.1);
        }
    }

    void prepare(const dsp::ProcessSpec &spec)
    {
        for (auto &f : dcBlock)
        {
            f.prepare(spec);
            f.setCutoffFreq(10.0);
            f.setType(strix::FilterType::highpass);
        }

        sc_lp.prepare(spec);
        sc_lp.setType(strix::FilterType::lowpass);
        sc_lp.setCutoffFreq(5.0);

        bpPre.prepare(spec);
        bpPre.setType(strix::FilterType::bandpass);
        bpPost.prepare(spec);
        bpPost.setType(strix::FilterType::bandpass);

        setType(type);
    }

    void reset()
    {
        for (auto &f : dcBlock)
            f.reset();
        sc_lp.reset();
    }

    template <typename Block>
    void processBlockClassB(Block &block)
    {
        for (int ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto in = block.getChannelPointer(ch);
            if (type == PentodeType::Nu)
                processSamplesNu(in, ch, block.getNumSamples(), bias.first, bias.second);
            else
                processSamplesClassic(in, ch, block.getNumSamples(), bias.first, bias.second);
        }
    }

    bias_t bias;
    PentodeType type = PentodeType::Nu;
    float inGain = 1.f;

private:
    void processSamplesNu(T *in, size_t ch, size_t numSamples, T gp, T gn)
    {
        float bpPreGain = -inGain;
        float bpPostGain = -bpPreGain;
        for (size_t i = 0; i < numSamples; ++i)
        {
            in[i] += bpPreGain * bpPre.processSample(ch, in[i]);
            in[i] -= 1.2 * processEnvelopeDetector(in[i], ch);
            in[i] = saturateSym(in[i], gp);
            in[i] += bpPostGain * bpPost.processSample(ch, in[i]);
        }
    }

    void processSamplesClassic(T *in, size_t ch, size_t numSamples, T gp, T gn)
    {
        float bpPreGain = jmap(inGain, 1.f, -1.f);
        float bpPostGain = -bpPreGain;
        for (size_t i = 0; i < numSamples; ++i)
        {
            in[i] += bpPreGain * bpPre.processSample(ch, in[i]);
            in[i] -= 0.8 * processEnvelopeDetector(in[i], ch);

            T yn_pos = classicPentode(in[i], gn, gp);
            T yn_neg = classicPentode(in[i], gp, gn);

            dcBlock[0].processSample(ch, yn_pos);
            dcBlock[1].processSample(ch, yn_neg);

            yn_pos = classicPentode(yn_pos, 2.01, 2.01);
            yn_neg = classicPentode(yn_neg, 2.01, 2.01);

            in[i] = 0.5 * (yn_pos + yn_neg);
            in[i] += bpPostGain * bpPost.processSample(ch, in[i]);
        }
    }

    inline T saturateSym(T x, T g = 1.0)
    {
        x = xsimd::select(x > (T)4.0, (T)4.0, x);
        x = xsimd::select(x < (T)-4.0, (T)-4.0, x);
        return (1.0 / g) * strix::fast_tanh(x * g);
    }

    inline T saturateAsym(T x, T gp, T gn)
    {
#if USE_SIMD
        xsimd::batch_bool<double> pos{x > 0.0};
        return xsimd::select(pos, ((1.0 / gp) * strix::fast_tanh(gp * x)),
                             ((1.0 / gn) * strix::fast_tanh(gn * x)));
#else
        if (x > 0.0)
            return (1.0 / gp) * strix::fast_tanh(gp * x);
        else
            return (1.0 / gn) * strix::fast_tanh(gn * x);
#endif
    }

    inline T classicPentode(T xn, T Ln, T Lp, T g = 1.0)
    {
#if USE_SIMD
        return xsimd::select(xn <= 0.0,
                             (g * xn) / (1.0 - ((g * xn) / Ln)),
                             (g * xn) / (1.0 + ((g * xn) / Lp)));
#else
        if (xn <= 0.0)
            return (xn * g) / (1.0 - ((g * xn) / Ln));
        else
            return (xn * g) / (1.0 + ((g * xn) / Lp));
#endif
    }

    inline T processEnvelopeDetector(T x, int ch)
    {
#if USE_SIMD
        x = xsimd::abs(x);
#else
        x = std::abs(x);
#endif
        return 0.151188 * sc_lp.processSample(ch, x);
    }

    std::array<strix::SVTFilter<T>, 2> dcBlock; /*need 2 for pos & neg signals*/
    strix::SVTFilter<T> sc_lp, bpPre, bpPost;
};

enum TriodeType
{
    VintageTube,
    ModernTube,
    ChannelTube
};
/**
 * A different tube emultaor for triodes, with parameterized bias levels & Stateful saturation
 */
template <typename T>
struct AVTriode : PreampProcessor
{
    AVTriode() = default;

    TriodeType type;

    void prepare(const dsp::ProcessSpec &spec)
    {
        y_m.resize(spec.numChannels);
        // std::fill(y_m.begin(), y_m.end(), 0.0);

        sc_hp.prepare(spec);
        sc_hp.setType(strix::FilterType::highpass);
        sc_hp.setCutoffFreq(20.0);

        sm_gp.reset(spec.sampleRate, 0.01);
        sm_gn.reset(spec.sampleRate, 0.01);
        sm_gp.setCurrentAndTargetValue(bias.first);
        sm_gn.setCurrentAndTargetValue(bias.second);
    }

    void reset()
    {
        std::fill(y_m.begin(), y_m.end(), 0.0);
        sc_hp.reset();
    }

    template <TriodeType mode = VintageTube>
    inline void processSamples(T *x, size_t ch, size_t numSamples)
    {
        sm_gp.setTargetValue(bias.first);
        sm_gn.setTargetValue(bias.second);
        switch (mode)
        {
        case VintageTube:
            for (size_t i = 0; i < numSamples; ++i)
            {
                auto p = sm_gp.getNextValue();
                auto n = sm_gn.getNextValue();
#if USE_SIMD
                x[i] = xsimd::select(x[i] > 0.0,
                                     (x[i] + (x[i] * x[i])) / (1.0 + p * x[i] * x[i])),
                                     x[i] / (1.0 - n * x[i]));
#else
                if (x[i] > 0.0)
                    x[i] = (x[i] + x[i] * x[i]) / (1.0 + p * x[i] * x[i]);
                else
                    x[i] = x[i] / (1.0 - n * x[i]);
#endif
            }
            break;
        case ModernTube:
            for (size_t i = 0; i < numSamples; ++i)
            {
                auto p = sm_gp.getNextValue();
                x[i] = (1.f / p) * strix::fast_tanh(p * x[i]);
            }
            break;
        case ChannelTube:
            for (size_t i = 0; i < numSamples; ++i)
            {
                auto p = sm_gp.getNextValue();
                auto n = sm_gn.getNextValue();
                auto f1 = (1.f / p) * strix::tanh(p * x[i]) * y_m[ch];
                auto f2 = (1.f / n) * strix::atan(n * x[i]) * (1.f - y_m[ch]);

                x[i] = f1 + f2;
                y_m[ch] = sc_hp.processSample(ch, x[i]);
            }
            break;
        }
    }

#if USE_SIMD
    void process(strix::AudioBlock<vec> &block) override
    {
        for (size_t ch = 0; ch < block.getNumChannels(); ch++)
        {
            auto in = block.getChannelPointer(ch);

            switch (type)
            {
            case VintageTube:
                processSamples<VintageTube>(in, ch, block.getNumSamples());
                break;
            case ModernTube:
                processSamples<ModernTube>(in, ch, block.getNumSamples());
                break;
            case ChannelTube:
                processSamples<ChannelTube>(in, ch, block.getNumSamples());
                break;
            }
        }
    }
#else
    void process(dsp::AudioBlock<double> &block) override
    {
        for (size_t ch = 0; ch < block.getNumChannels(); ch++)
        {
            auto in = block.getChannelPointer(ch);

            switch (type)
            {
            case VintageTube:
                processSamples<VintageTube>(in, ch, block.getNumSamples());
                break;
            case ModernTube:
                processSamples<ModernTube>(in, ch, block.getNumSamples());
                break;
            case ChannelTube:
                processSamples<ChannelTube>(in, ch, block.getNumSamples());
                break;
            }
        }
    }
#endif

    bias_t bias;

private:
    std::vector<T> y_m;
    strix::SVTFilter<T> sc_hp;
    SmoothedValue<double> sm_gp, sm_gn;
};