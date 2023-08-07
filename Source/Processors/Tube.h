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
    Nu,
    Transformer
};

struct bias_t
{
    double first = 1.0;
    double second = 1.0;
};

/**
 * A saturation emulator for Class B tube/transformer simulation, with option for a dynamic bias
 */
template <typename T>
struct Pentode
{
    Pentode() = default;

    /**
     * @param type Pentode Type. Use widely asymmetric values in Classic mode to get the intended effect
     */
    Pentode(PentodeType _type) : type(_type)
    {
    }

    void setType(const PentodeType newType)
    {
        // TODO: Test and set bandpass for Transformer mode
        // will likely involve very wide bandpass as opposed to midrange bump/dip
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
            switch(type)
            {
            case Nu:
                processSamplesNu(in, ch, block.getNumSamples(), bias.first, bias.second);
                break;
            case Classic:
                processSamplesClassic(in, ch, block.getNumSamples(), bias.first, bias.second);
                break;
            case Transformer:
                processSamplesTransformer(in, ch, block.getNumSamples());
                break;
            }
        }
    }

    bias_t bias;
    PentodeType type = PentodeType::Nu;
    float inGain = 1.f;

private:
    void processSamplesNu(T *in, size_t ch, size_t numSamples, T gp, T gn)
    {
        float bpPreGain = -inGain * 0.5f;
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

    void processSamplesTransformer(T *in, size_t ch, size_t numSamples)
    {
        const auto transformer_input_scale = 0.66; // input trim
        const auto transformer_env_scale = 0.69;
        for (size_t i = 0; i < numSamples; ++i)
        {
            in[i] *= transformer_input_scale;
            auto e = strix::abs(in[i] - ym[ch]) * 0.5;
            in[i] += e * e * transformer_env_scale;
            auto func = [&](T in) -> T {
                in -= (in*in*in) / 3.0;
                in *= 3.0 / 2.0;
                return in;
            };
#if USE_SIMD
            in[i] = xsimd::select(in[i] > (T)1.0, (T)1.0,
                    xsimd::select(in[i] < (T)-1.0, (T)-1.0,
                    func(in[i])));
#else
            if (in[i] > 1.0)
                in[i] = 1.0;
            else if (in[i] < -1.0)
                in[i] = -1.0;
            else
                in[i] = func(in[i]);
#endif
            ym[ch] = in[i];

            in[i] = dcBlock[0].processSample(ch, in[i]);
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
        return 0.151188 * sc_lp.processSample(ch, strix::abs(x));
    }

    std::array<strix::SVTFilter<T>, 2> dcBlock; /*need 2 for pos & neg signals*/
    strix::SVTFilter<T> sc_lp, bpPre, bpPost;
    T ym[2] = {0};
};

enum TriodeType
{
    VintageTube,
    ModernTube,
    ChannelTube,
    SolidState
};

template <typename T>
struct AVTriode : PreampProcessor
{

    AVTriode() = default;

    TriodeType type;

    void prepare(const dsp::ProcessSpec &spec)
    {
        y_m.resize(spec.numChannels);
        // std::fill(y_m.begin(), y_m.end(), 0.0);

        sc_filter.prepare(spec);
        sc_filter.setType(type == SolidState ? strix::FilterType::lowpass : strix::FilterType::highpass);
        sc_filter.setCutoffFreq(20.0);

        sm_gp.reset(spec.sampleRate, 0.01);
        sm_gn.reset(spec.sampleRate, 0.01);
        sm_gp.setCurrentAndTargetValue(bias.first);
        sm_gn.setCurrentAndTargetValue(bias.second);
    }

    void reset()
    {
        std::fill(y_m.begin(), y_m.end(), 0.0);
        sc_filter.reset();
    }

    void setType(TriodeType newType)
    {
        type = newType;
        sc_filter.setType(type == SolidState ? strix::FilterType::lowpass : strix::FilterType::highpass);
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
                                     (x[i] + (x[i] * x[i])) / (1.0 + p * x[i] * x[i]),
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
                y_m[ch] = sc_filter.processSample(ch, x[i]);
            }
            break;
        case SolidState:
            const auto triode_env_scale = 0.05;
            for (size_t i = 0; i < numSamples; ++i)
            {
                // apply dynamic bias
                auto env = sc_filter.processSample(ch, strix::abs(x[i]));
                x[i] -= env * env * triode_env_scale;
                
                // 5th order polynomial
                auto func = [&](T x) -> T {
                    x -= (x*x*x*x*x) / 5.0;
                    x *= 5.0 / 4.0;
                    return x;
                };
#if USE_SIMD
                x[i] = xsimd::select(x[i] > (T)1.0, (T)1.0,
                        xsimd::select(x[i] < (T)-1.0, (T)-1.0,
                        func(x[i])));
#else
                if (x[i] > 1.0)
                    x[i] = 1.0;
                else if (x[i] < -1.0)
                    x[i] = -1.0;
                else
                    x[i] = func(x[i]);
#endif
                // adding 2nd harmonic
                x[i] += x[i] * x[i] * 0.05;
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
            case SolidState:
                processSamples<SolidState>(in, ch, block.getNumSamples());
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
            case SolidState:
                processSamples<SolidState>(in, ch, block.getNumSamples());
                break;
            }
        }
    }
#endif

    bias_t bias;

private:
    std::vector<T> y_m;
    strix::SVTFilter<T> sc_filter;
    SmoothedValue<double> sm_gp, sm_gn;
};