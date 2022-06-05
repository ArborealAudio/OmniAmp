/*
  ==============================================================================

    Tube.h
    Created: 25 Jan 2022 5:48:43pm
    Author:  alexb

  ==============================================================================
*/

#pragma once

struct Cab
{
    Cab() = default;

    void prepare(const dsp::ProcessSpec& spec)
    {
        for (auto& f : sc_lp) {
            f.prepare(spec);
            *f.coefficients = dsp::IIR::ArrayCoefficients<float>::makeLowPass(spec.sampleRate, 5000.f);
        }
    }

    inline float processSample(float x, int ch)
    {
        return std::tanh(sc_lp[ch].processSample(x));
    }

private:

    std::array<dsp::IIR::Filter<float>, 2> sc_lp;
};

struct Tube
{
    Tube() = default;

    void prepare(const dsp::ProcessSpec& spec)
    {
        lastSampleRate = spec.sampleRate;

        for (auto& f : sc_lp) {
            f.prepare(spec);
            *f.coefficients = dsp::IIR::ArrayCoefficients<float>::makeLowPass(lastSampleRate, 5.f);
        }
        for (auto& f : m_lp) {
            f.prepare(spec);
            *f.coefficients = dsp::IIR::ArrayCoefficients<float>::makeFirstOrderLowPass(lastSampleRate, 10000.f);
        }
    }

    void reset()
    {
        for (auto& f : sc_lp)
            f.reset();
        for (auto& f : m_lp)
            f.reset();
    }

    void processBuffer(AudioBuffer<float>& buffer, float gp, float gn)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto in = buffer.getWritePointer(ch);

            processSamples(in, ch, buffer.getNumSamples(), gp, gn);
        }
    }

    void processBufferClassB(AudioBuffer<float>& buffer, float gp, float gn)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto in = buffer.getWritePointer(ch);

            processSamplesClassB(in, ch, buffer.getNumSamples(), gp, gn);
        }
    }

private:

    inline void processSamples(float* in, int ch, size_t numSamples, float gp, float gn)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            in[i] -= processEnvelopeDetector(in[i], ch);

            in[i] = -1.f * saturate(in[i], gp, gn);

            x_1 = in[i];
            in[i] = x_1 - xm1[ch] + r * ym1[ch];
            xm1[ch] = x_1;
            ym1[ch] = in[i];

            in[i] = m_lp[ch].processSample(in[i]);
        }
    }

    inline void processSamplesClassB(float* in, int ch, size_t numSamples, float gp, float gn)
    {
        for (int i = 0; i < numSamples; ++i)
        {
            in[i] -= 1.2f * processEnvelopeDetector(in[i], ch);

            in[i] = saturate(in[i], gp, gn);
        }
    }

    inline float saturate(float x, float gp, float gn)
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

    inline float processEnvelopeDetector(float x, int ch)
    {
        auto xr = std::fabs(x);

        return 0.251188f * sc_lp[ch].processSample(xr);
    }

    std::array<dsp::IIR::Filter<float>, 2> sc_lp, m_lp;

    double lastSampleRate = 0.0;

    double r = 1.0 - (1.0 / 800.0);

    float x_1 = 0.f, gp_raw = 0.f, gn_raw = 0.f;
    float xm1[2]{ 0.0, 0.0 }, ym1[2]{ 0.0, 0.0 };

};

struct AVTriode
{
    AVTriode() = default;

    void prepare(const dsp::ProcessSpec& spec)
    {
        for (auto& f : sc_hp) {
            f.prepare(spec);
            *f.coefficients = dsp::IIR::ArrayCoefficients<float>::makeHighPass(spec.sampleRate, 20.f);
        }

        cab.prepare(spec);
    }

    void reset()
    {
        for (auto& f : sc_hp)
            f.reset();

        y_m[0] = 0.f;
    }

    inline float processSample(float x, int ch, float gp, float gn)
    {
        auto f1 = (1.f / gp) * std::tanh(gp * x) * y_m[ch];
        auto f2 = (1.f / gn) * std::atan(gn * x) * (1.f - y_m[ch]);

        auto y = -sc_hp[ch].processSample(f1 + f2);

        y_m[ch] = cab.processSample(y, ch);
        // y_m[ch] = y;

        return y;
    }

    void process(AudioBuffer<float>& buffer, float gp, float gn)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto in = buffer.getWritePointer(ch);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                in[i] = processSample(in[i], ch, gp, gn);
            }
        }
    }

private:

    float y_m[2]{ 0.f, 0.f };

    std::array<dsp::IIR::Filter<float>,2> sc_hp;

    Cab cab;
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