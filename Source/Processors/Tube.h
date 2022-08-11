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

        sag.prepare(spec);
        sag.setAttackTime(3.0);
        sag.setReleaseTime(150.0);
        sag.setLevelCalculationType(dsp::BallisticsFilterLevelCalculationType::RMS);
    }

    void reset()
    {
        for (auto& f : sc_lp)
            f.reset();
        for (auto& f : m_lp)
            f.reset();

        sag.reset();
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
            auto sV = sag.processSample(ch, in[i]);
            if (sV > sagThresh)
                in[i] -= (sV - sagThresh);

            // in[i] -= 1.2f * processEnvelopeDetector(in[i], ch);

            in[i] = saturate(in[i], gp, gn);

            in[i] = std::sinh(in[i]) / 6.f;
        }
    }

    inline void processSamplesClassBSIMD(T* in, int ch, size_t numSamples, T gp, T gn)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            auto sV = sag.processSample(ch, xsimd::reduce_max(in[i]));
            if (sV > sagThresh)
                in[i] -= (sV - sagThresh);
            
            // in[i] -= (T)1.2 * processEnvelopeDetectorSIMD(in[i], ch);

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

    dsp::BallisticsFilter<double> sag;
    double sagThresh = 12.0;

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

    inline T processSample(T x, size_t ch, T gp, T gn)
    {
        auto f1 = (1.f / gp) * std::tanh(gp * x) * y_m[ch];
        auto f2 = (1.f / gn) * std::atan(gn * x) * (1.f - y_m[ch]);
        // auto f1 = (gp * x) / 3.4;
        // f1 = f1 * 0.5f;
        // f1 = std::abs(f1 + 0.5) - abs(f1 - 0.5);
        // f1 = (std::abs(f1) - 2.0) * f1;
        // f1 = (std::abs(f1) - 2.0) * f1 * (1.0 / gp) * y_m[0];
        // auto f2 = (1.0 / gn) * ((x * gn) / (1.0 + std::abs(x))) * (1.0 - y_m[ch]);

        auto y = sc_hp.processSample(ch, f1 + f2);
        y_m[ch] = y;

        return y;
    }

    inline T processSampleSIMD(T x, T gp, T gn)
    {
        auto f1 = (1.0 / gp) * xsimd::tanh(gp * x) * y_m[0];
        auto f2 = (1.0 / gn) * xsimd::atan(gn * x) * (1.0 - y_m[0]);

        auto y = sc_hp.processSample(0, f1 + f2);
        y_m[0] = y;

        return y;
    }

    void processBlock(dsp::AudioBlock<double>& block, T gp, T gn)
    {
        for (size_t ch = 0; ch < block.getNumChannels(); ch++)
        {
            auto in = block.getChannelPointer(ch);

            for (size_t i = 0; i < block.getNumSamples(); i++)
                in[i] = processSample(in[i], ch, gp, gn);
        }
    }

    void processBlock(strix::AudioBlock<vec>& block, T gp, T gn)
    {
        for (size_t ch = 0; ch < block.getNumChannels(); ch++)
        {
            auto in = block.getChannelPointer(ch);

            for (size_t i = 0; i < block.getNumSamples(); i++)
                in[i] = processSampleSIMD(in[i], gp, gn);
        }
    }

private:

    std::vector<T> y_m;

    strix::SVTFilter<T> sc_hp;
};

struct DZTube
{
    DZTube(float mu, float gamma, float xi, float G, float Gg, float C, float Cg, float Ig0,
        bool kFollow) :
        mu(mu), gamma(gamma), xi(xi), G(G), Gg(Gg), C(C), Cg(Cg), Ig0(Ig0), kFollow(kFollow)
    {}

    inline float processIk(float Vg)
    {
        auto log = std::log10(1.f + std::exp(C * (1.f / mu) * 325.f * Vg)) * (1.f / C);
        return G * std::pow(log, gamma);
    }

    inline float processIg(float Vg)
    {
        auto log = std::log10(1.f + std::exp(Cg * Vg)) * (1.f / Cg);
        return Gg * std::pow(log, xi) + Ig0;
    }

    inline float processSample(float x)
    {
        auto Ik = processIk(x);
        auto Ig = processIg(x);

        return kFollow ? Ik * 250.f : (Ik - Ig) * -250.f;
    }

    void processBuffer(AudioBuffer<float>& buffer)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto in = buffer.getWritePointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                in[i] = processSample(in[i]);

                x1 = in[i];
                in[i] = x1 - xm[ch] + r * ym[ch];
                xm[ch] = x1;
                ym[ch] = in[i];
            }
        }
    }

private:
    float mu, gamma, xi, G, Gg, C, Cg, Ig0;
    float xm[2]{ 0.f, 0.f }, ym[2]{ 0.f, 0.f }, x1;
    const float r = 1.0 - (1.0 / 800.0);
    bool kFollow = false;
};

struct KorenTriode
{
    KorenTriode(float mu, float KG1, float Ep, float kp, float Kvb, float X) :
        mu(mu), KG1(KG1), Ep(Ep), kp(kp), Kvb(Kvb), X(X)
    {}

    inline void processE1(float Vin)
    {
        float Epkp = Ep / kp;
        float log = std::log10(1.f + std::exp(kp * (1.f / mu + (Vin / std::sqrt(Kvb + Ep * Ep)))));
        E1 = Epkp * log;
    }

    inline float processPlateCurrent()
    {
        return std::pow(E1, X) * (1.f + E1 >= 0.f ? 1.f : -1.f);
    }

    inline float processSample(float x)
    {
        processE1(x);
        auto Ip = processPlateCurrent();

        return -(Ip * 0.7);
    }

    void processBuffer(AudioBuffer<float>& buffer)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto in = buffer.getWritePointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                in[i] = processSample(in[i]);

                x1 = in[i];
                in[i] = x1 - xm[ch] + r * ym[ch];
                xm[ch] = x1;
                ym[ch] = in[i];
            }
        }
    }

private:
    float mu, KG1, Ep, kp, Kvb, E1 = 0.f, X;
    float xm[2]{ 0.f, 0.f }, ym[2]{ 0.f, 0.f }, x1 = 0.f;
    const float r = 1.0 - (1.0 / 800.0);
};

struct KorenPentode
{
    KorenPentode(float mu, float X, float KG1, float KG2, float Kp, float Kvb, float Vg2) :
        mu(mu), X(X), KG1(KG1), KG2(KG2), Kp(Kp), Kvb(Kvb), Vg2(Vg2)
    {}

    inline void processE1(float Vg)
    {
        auto log = std::log10(1.f + std::exp(Kp * (1.f / mu + (Vg / Vg2))));
        E1 = (Vg2 / Kp) * log;
    }

    inline float processIp()
    {
        auto first = std::pow(E1, X) / KG1;
        auto second = 1.f + E1 >= 0.f ? 1.f : -1.f;
        auto third = std::atan(30.f / Kvb);

        return first * second * third;
    }

    inline float processSample(float x)
    {
        processE1(35.f * x);
        return processIp();
    }

    void processBuffer(AudioBuffer<float>& buffer)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto in = buffer.getWritePointer(ch);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                auto xp = -processSample(-in[i]);
                auto xn = processSample(in[i]);

                in[i] = (xn + xp) / 2.f;

                /*x1 = in[i];
                in[i] = x1 - xm[ch] + r * ym[ch];
                xm[ch] = x1;
                ym[ch] = in[i];*/
            }
        }
    }

private:
    float mu, X, KG1, KG2, Kp, Kvb, E1 = 0.f, Vg2;
    float xm[2]{ 0.f, 0.f }, ym[2]{ 0.f, 0.f }, x1 = 0.f;
    const float r = 1.0 - (1.0 / 800.0);
};