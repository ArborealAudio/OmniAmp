/*
  ==============================================================================

    Tube.h
    Created: 25 Jan 2022 5:48:43pm
    Author:  alexb

  ==============================================================================
*/

#pragma once

template <typename T>
struct Tube
{
    Tube() = default;

    void prepare(const dsp::ProcessSpec& spec)
    {
        lastSampleRate = spec.sampleRate;

        sc_coeffs = dsp::IIR::Coefficients<double>::makeLowPass(lastSampleRate, 5.f);
        m_coeffs = dsp::IIR::Coefficients<double>::makeFirstOrderLowPass(lastSampleRate, 10000.f);

        for (auto& f : sc_lp) {
            f.prepare(spec);
            f.coefficients = sc_coeffs;
        }
        for (auto& f : m_lp) {
            f.prepare(spec);
            f.coefficients = m_coeffs;
        }
    }

    void reset()
    {
        for (auto& f : sc_lp)
            f.reset();
        for (auto& f : m_lp)
            f.reset();
    }

    template <class Block>
    void processBlock(Block& block, T gp, T gn)
    {
        for (int ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto in = block.getChannelPointer(ch);

            processSamples(in, ch, block.getNumSamples(), gp, gn);
        }
    }

    void processBlockClassB(dsp::AudioBlock<double>& block, T gp, T gn)
    {
        for (int ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto in = block.getChannelPointer(ch);

            processSamplesClassB(in, ch, block.getNumSamples(), gp, gn);
        }
    }

    void processBlockClassB(strix::AudioBlock<vec>& block, T gp, T gn)
    {
        auto in = block.getChannelPointer(0);

        processSamplesClassBSIMD(in, 0, block.getNumSamples(), gp, gn);
    }

private:

    inline void processSamples(T* in, int ch, size_t numSamples, T gp, T gn)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            in[i] -= processEnvelopeDetector(in[i], ch);

            in[i] = -1.0 * saturate(in[i], gp, gn);

            x_1 = in[i];
            in[i] = x_1 - xm1[ch] + r * ym1[ch];
            xm1[ch] = x_1;
            ym1[ch] = in[i];

            in[i] = m_lp[ch].processSample(in[i]);
        }
    }

    inline void processSamplesClassB(T* in, int ch, size_t numSamples, T gp, T gn)
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            in[i] -= 1.2 * processEnvelopeDetector(in[i], ch);

            in[i] = saturate(in[i], gp, gn);

            in[i] = std::sinh(in[i]) / 6.f;
        }
    }

    inline void processSamplesClassBSIMD(T* in, int ch, size_t numSamples, T gp, T gn)
    {
        for (int i = 0; i < numSamples; ++i)
        {            
            in[i] -= 1.2 * processEnvelopeDetectorSIMD(in[i], ch);

            in[i] = saturateSIMD(in[i], gp, gn);

            in[i] = xsimd::sinh(in[i]) / (T)6.0;
        }
    }

    inline T saturate(T x, T gp, T gn)
    {
        if (x > 0.f)
        {
            x = ((1.f / gp) * std::tanh(gp * x));
        }
        else
        {
            x = ((1.f / gn) * std::tanh(gn * x));
        }

        return x;
    }

    inline T saturateSIMD(T x, T gp, T gn)
    {
        xsimd::batch_bool<double> pos {x > 0.0};
        return xsimd::select(pos, ((1.0 / gp) * xsimd::tanh(gp * x)),
        ((1.0 / gn) * xsimd::tanh(gn * x)));
    }

    inline T processEnvelopeDetector(T x, int ch)
    {
        auto xr = std::fabs(x);

        return 0.251188 * sc_lp[ch].processSample(xr);
    }

    inline T processEnvelopeDetectorSIMD(T x, int ch)
    {
        auto xr = xsimd::fabs(x);

        return 0.251188 * sc_lp[ch].processSample(xr);
    }

    dsp::IIR::Coefficients<double>::Ptr sc_coeffs, m_coeffs;

    std::array<dsp::IIR::Filter<T>, 2> sc_lp, m_lp;

    double lastSampleRate = 0.0;

    T r = 1.0 - (1.0 / 800.0);

    T x_1 = 0.0;
    T xm1[2]{ 0.0 }, ym1[2]{ 0.0 };
};

template <typename T>
struct AVTriode
{
    AVTriode() = default;

    void prepare(const dsp::ProcessSpec& spec)
    {
        y_m.resize(spec.numChannels);

        sc_hp.prepare(spec);
        sc_hp.setType(strix::FilterType::highpass);
        sc_hp.setCutoffFreq(20.0);
    }

    void reset()
    {
        std::fill(y_m.begin(), y_m.end(), 0.0);

        sc_hp.reset();
    }

    inline void processSamples(T* x, size_t ch, size_t numSamples, T gp, T gn)
    {
        auto inc_p = (gp - lastGp) / numSamples;
        auto inc_n = (gn - lastGn) / numSamples;

        for (size_t i = 0; i < numSamples; ++i)
        {
            auto f1 = (1.f / lastGp) * std::tanh(lastGp * x[i]) * y_m[ch];
            auto f2 = (1.f / lastGn) * std::atan(lastGn * x[i]) * (1.f - y_m[ch]);

            lastGp += inc_p;
            lastGn += inc_n;

            auto y = sc_hp.processSample(ch, f1 + f2);
            y_m[ch] = y;
            x[i] = y;
        }

        lastGp = gp;
        lastGn = gn;
    }

    inline void processSamplesSIMD(T* x, size_t numSamples, T gp, T gn)
    {
        auto inc_p = (gp - lastGp) / numSamples;
        auto inc_n = (gn - lastGn) / numSamples;

        for (size_t i = 0; i < numSamples; ++i)
        {
            auto f1 = (1.0 / lastGp) * xsimd::tanh(lastGp * x[i]) * y_m[0];
            auto f2 = (1.0 / lastGn) * xsimd::atan(lastGn * x[i]) * (1.0 - y_m[0]);

            lastGp += inc_p;
            lastGn += inc_n;

            auto y = sc_hp.processSample(0, f1 + f2);
            y_m[0] = y;
            x[i] = y;
        }

        lastGp = gp;
        lastGn = gn;
    }

    void processBlock(dsp::AudioBlock<double>& block, T gp, T gn)
    {
        for (size_t ch = 0; ch < block.getNumChannels(); ch++)
        {
            auto in = block.getChannelPointer(ch);

            processSamples(in, ch, block.getNumSamples(), gp, gn);
        }
    }

    void processBlock(strix::AudioBlock<vec>& block, T gp, T gn)
    {
        for (size_t ch = 0; ch < block.getNumChannels(); ch++)
        {
            auto in = block.getChannelPointer(ch);

            processSamplesSIMD(in, block.getNumSamples(), gp, gn);
        }
    }

private:

    std::vector<T> y_m;

    strix::SVTFilter<T> sc_hp;

    T lastGp = 1.0, lastGn = 1.0;
};