/*
  ==============================================================================

    Tube.h
    Created: 25 Jan 2022 5:48:43pm
    Author:  alexb

  ==============================================================================
*/

enum PentodeType
{
    Classic,
    Nu
};

#pragma once

/**
 * A tube emulator for Class B simulation, with option for a dynamic bias & different saturation algorithms
 */
template <typename T, PentodeType type = Nu>
struct Pentode
{
    Pentode() = default;

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
    }

    void reset()
    {
        for (auto &f : dcBlock)
            f.reset();
        sc_lp.reset();
    }

    template <typename Block>
    void processBlockClassB(Block &block, T gp, T gn)
    {
        for (int ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto in = block.getChannelPointer(ch);
            if (type == PentodeType::Nu)
                processSamplesNu(in, ch, block.getNumSamples(), gp, gn);
            else
                processSamplesClassic(in, ch, block.getNumSamples(), gp, gn);
        }
    }

private:
    void processSamplesNu(T *in, int ch, size_t numSamples, T gp, T gn)
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            in[i] -= 1.2 * processEnvelopeDetector(in[i], ch);

            in[i] = saturateAsym(in[i], gp, gp);

            in[i] = dsp::FastMathApproximations::sinh(in[i]) / (T)6.0;
        }
    }

    void processSamplesClassic(T* in, size_t ch, size_t numSamples, T gp, T gn)
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            in[i] -= 1.2 * processEnvelopeDetector(in[i], ch);

            T yn_pos = classicPentode(in[i], 10.0, gp);
            T yn_neg = classicPentode(in[i], gn, 10.0);

            dcBlock[0].processSample(ch, yn_pos);
            dcBlock[1].processSample(ch, yn_neg);

            yn_pos = classicPentode(yn_pos, 1.01, 1.01);
            yn_neg = classicPentode(yn_neg, 1.01, 1.01);

            in[i] = yn_pos + yn_neg;
            in[i] *= 0.5;
        }
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

    inline T classicPentode(T xn, T Ln, T Lp)
    {
#if USE_SIMD
        return xsimd::select(xn <= 0.0,
                             xn / (1.0 - (xn / Ln)),
                             xn / (1.0 + (xn / Lp)));
#else
        if (xn <= 0.0)
            return (g * xn) / (1.0 - ((g * xn) / Ln));
        else
            return (g * xn) / (1.0 + ((g * xn) / Lp));
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

    strix::SVTFilter<T> sc_lp;
};

/**
 * A different tube emultaor for triodes, with parameterized bias levels & Stateful saturation
 */
template <typename T>
struct AVTriode
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
        auto inc_p = (gp - lastGp) / numSamples;
        auto inc_n = (gn - lastGn) / numSamples;

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

    template <typename Block>
    void processBlock(Block &block, T gp, T gn)
    {
        for (size_t ch = 0; ch < block.getNumChannels(); ch++)
        {
            auto in = block.getChannelPointer(ch);

            processSamples(in, ch, block.getNumSamples(), gp, gn);
        }
    }

private:
    std::vector<T> y_m;

    strix::SVTFilter<T> sc_hp;

    T lastGp = 1.0, lastGn = 1.0;
};