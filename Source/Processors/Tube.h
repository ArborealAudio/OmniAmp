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

typedef struct
{
    double first = 1.0;
    double second = 1.0;
} bias_t;

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

    void prepare(const dsp::ProcessSpec &spec)
    {
        for (auto &f : dcBlock)
        {
            f.prepare(spec);
            f.setCutoffFreq(10.0);
            f.setType(strix::FilterType::highpass);
        }

        sc_lp.prepare(spec);
        sc_lp.setCutoffFreq(5.0);
        sc_lp.setType(strix::FilterType::lowpass);

        absorb.prepare(spec);
        absorb.setCutoffFreq(type == Nu ? 10000.0 : 6500.0);
        absorb.setType(strix::FilterType::firstOrderLowpass);
    }

    void reset()
    {
        for (auto &f : dcBlock)
            f.reset();
        sc_lp.reset();
        absorb.reset();
    }

    template <typename Block>
    void processBlockClassB(Block &block)
    {
        if (type == PentodeType::Nu)
            block *= 6.0;
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

private:
    void processSamplesNu(T *in, int ch, size_t numSamples, T gp, T gn)
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            in[i] -= 1.2 * processEnvelopeDetector(in[i], ch);

            in[i] = saturateSym(in[i], gp);

            in[i] = dsp::FastMathApproximations::sinh(in[i]) / (T)6.0;
            
            in[i] = absorb.processSample(ch, in[i]);
        }
    }

    void processSamplesClassic(T *in, size_t ch, size_t numSamples, T gp, T gn)
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            auto env = 1.2 * processEnvelopeDetector(in[i], ch);
            in[i] -= env;

            T yn_pos = classicPentode(in[i], gn, gp, 2.0);
            T yn_neg = classicPentode(in[i], gp, gn, 2.0);

            dcBlock[0].processSample(ch, yn_pos);
            dcBlock[1].processSample(ch, yn_neg);

            yn_pos = classicPentode(yn_pos, 1.01, 1.01);
            yn_neg = classicPentode(yn_neg, 1.01, 1.01);

            in[i] = 0.5 * (yn_pos + yn_neg);
            in[i] = absorb.processSample(ch, in[i]);
        }
    }

    inline T saturateSym(T x, T g = 1.0)
    {
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
            return xn / (1.0 - (xn / Ln));
        else
            return xn / (1.0 + (xn / Lp));
#endif
    }

    inline T processEnvelopeDetector(T x, int ch)
    {
        T xr = 0.0;
#if USE_SIMD
        xr = xsimd::abs(x);
#else
        xr = std::abs(x);
#endif

        return 0.251188 * sc_lp.processSample(ch, xr);
    }

    std::array<strix::SVTFilter<T>, 2> dcBlock; /*need 2 for pos & neg signals*/

    strix::SVTFilter<T> sc_lp, absorb;
};

/**
 * A different tube emultaor for triodes, with parameterized bias levels & Stateful saturation
 */
template <typename T>
struct AVTriode : PreampProcessor
{
    AVTriode() = default;

    void prepare(const dsp::ProcessSpec &spec)
    {
        y_m.resize(spec.numChannels);

        sc_hp.prepare(spec);
        sc_hp.setType(strix::FilterType::highpass);
        sc_hp.setCutoffFreq(10.0);
    }

    void reset()
    {
        std::fill(y_m.begin(), y_m.end(), 0.0);

        sc_hp.reset();
    }

    inline void processSamples(T *x, size_t ch, size_t numSamples, T gp, T gn)
    {
        auto inc_p = (gp - lastGp) / (T)numSamples;
        auto inc_n = (gn - lastGn) / (T)numSamples;

        for (size_t i = 0; i < numSamples; ++i)
        {
            auto f1 = (1.f / lastGp) * strix::fast_tanh(lastGp * x[i]) * y_m[ch];
#if USE_SIMD
            auto f2 = (1.f / lastGn) * xsimd::atan(lastGn * x[i]) * (1.0 - y_m[ch]);
#else
            auto f2 = (1.f / lastGn) * std::atan(lastGn * x[i]) * (1.0 - y_m[ch]);
#endif

            lastGp += inc_p;
            lastGn += inc_n;

            x[i] = f1 + f2;
            y_m[ch] = sc_hp.processSample(ch, x[i]);
        }

        lastGp = gp;
        lastGn = gn;
    }

#if USE_SIMD
    void process(strix::AudioBlock<vec> &block) override
    {
        for (size_t ch = 0; ch < block.getNumChannels(); ch++)
        {
            auto in = block.getChannelPointer(ch);

            processSamples(in, ch, block.getNumSamples(), bias.first, bias.second);
        }
    }
#else
    void process(dsp::AudioBlock<double> &block) override
    {
        for (size_t ch = 0; ch < block.getNumChannels(); ch++)
        {
            auto in = block.getChannelPointer(ch);

            processSamples(in, ch, block.getNumSamples(), bias.first, bias.second);
        }
    }
#endif

    bias_t bias;

private:
    std::vector<T> y_m;

    strix::SVTFilter<T> sc_hp;

    T lastGp = 1.0, lastGn = 1.0;
};