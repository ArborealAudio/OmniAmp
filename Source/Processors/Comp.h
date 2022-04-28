/*
  ==============================================================================

    Comp.h
    Created: 9 Mar 2022 11:12:10am
    Author:  alexb

  ==============================================================================
*/

#pragma once

struct OptoComp
{
    enum Type
    {
        Guitar,
        Bass,
        Channel
    };

    OptoComp(Type t) : type(t) {}

    void prepare(const dsp::ProcessSpec& spec)
    {
        lastSR = spec.sampleRate;

        sc_hp.prepare(spec);
        sc_lp.prepare(spec);
        
        switch (type) {
        case Guitar:
            *sc_hp.coefficients = dsp::IIR::ArrayCoefficients<float>::makeHighPass(spec.sampleRate, 200.f, 1.02f);
            *sc_lp.coefficients = dsp::IIR::ArrayCoefficients<float>::makeLowPass(spec.sampleRate, 3500.f, 0.8f);
            for (auto& h : hp) {
                h.prepare(spec);
                *h.coefficients = dsp::IIR::ArrayCoefficients<float>::makeFirstOrderHighPass(spec.sampleRate, 400.f);
            }
            for (auto& l : lp) {
                l.prepare(spec);
                *l.coefficients = dsp::IIR::ArrayCoefficients<float>::makeFirstOrderLowPass(spec.sampleRate, 5000.f);
            }
            break;
        case Channel:
            *sc_hp.coefficients = dsp::IIR::ArrayCoefficients<float>::makeHighPass(spec.sampleRate, 100.f, 0.707f);
            *sc_lp.coefficients = dsp::IIR::ArrayCoefficients<float>::makeLowPass(spec.sampleRate, 5000.f, 0.8f);
            break;
        default:
            break;
        }
    }

    void reset()
    {
        xm0 = 0.f, xm1 = 0.f, lastEnv = 0.f, lastGR = 0.f;
        sc_hp.reset();
        sc_lp.reset();
        
        for (auto& h : hp)
            h.reset();
        for (auto& l : lp)
            l.reset();
    }

    inline float detectEnv (float x)
    {
        auto t_abs = std::abs(x);

        float env;

        if (t_abs >= lastEnv)
        {
            //env = att * (lastEnv - t_abs) + t_abs;
        }
        else
        {
            //env = rel * (lastEnv - t_abs) + t_abs;
        }

        if (env < 1.175494351e-38f)
            env = 1.175494351e-38f;

        lastEnv = env;

        return 20.f * std::log10(env);
    }

    /*returns gain reduction multiplier*/
    inline float compress(float x)
    {     
        if (x < 1.175494351e-38f)
            x = 1.175494351e-38f;

        auto env = jmax(0.f, 8.685889638f * std::log10(x / threshold));

        float att_time = jlimit(0.005f, 0.050f, (1.f / x) * 0.015f);

        float att = std::exp(-1.f / (att_time * lastSR));
        float rel = std::exp(-1.f / (0.6f * lastGR * lastSR));

        if (env > lastEnv)
        {
            env = env + att * (lastEnv - env);
        }
        else
        {
            env = env + rel * (lastEnv - env);
            /*auto rel_env1 = env + rel * (lastEnv - env);
            auto rel_env2 = env + rel2 * (lastEnv - env);

            env = (rel_env1 * t_env) + (rel_env2 * (1.f-t_env));*/
            /*this method is working well, but it's difficult to get it to change w/
            the full desired range of rel time. let's try using one envelope w/ a time const
            determined by last GR*/
        }

        lastEnv = env;
        
        auto gr_db = 10.f * (-env); /*using tanh here makes a nice log curve*/
        auto gr = std::pow(10.f, gr_db / 20.f);
        lastGR = gr;

        return gr;
    }

    void process(AudioBuffer<float>& buffer, float comp)
    {
        auto inL = buffer.getWritePointer(0);
        auto inR = buffer.getWritePointer(1);

        auto c_comp = jmap(comp, 1.f, 6.f);

        switch (type) {
        case Guitar:
            threshold = std::pow(10.f, -18.f / 20.f);
            break;
        case Channel:
            if (c_comp <= 4.f) {
                auto thresh_scale = c_comp / 4.f;
                threshold = std::pow(10.f, (-18.f * thresh_scale) / 20.f);
            }
            else
                threshold = std::pow(10.f, -18.f / 20.f);
            break;
        default:
            break;
        }

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            float abs0 = std::abs(xm0);
            float abs1 = std::abs(xm1);
            float max = jmax(abs0, abs1);
            max = sc_hp.processSample(max);
            max = sc_lp.processSample(max);

            auto gr = compress(max);

            switch (type) {
            case Guitar:
                inL[i] *= c_comp * gr;
                inR[i] *= c_comp * gr;
                break;
            case Channel:
                if (c_comp <= 4.f) {
                    inL[i] *= gr;
                    inR[i] *= gr;
                }
                else {
                    inL[i] *= (c_comp / 4.f) * gr;
                    inR[i] *= (c_comp / 4.f) * gr;
                }
                break;
            }

            xm0 = inL[i];
            xm1 = inR[i];

            switch (type)
            {
            case Guitar:
                auto bpL = hp[0].processSample(inL[i]);
                bpL = lp[0].processSample(bpL);
                auto bpR = hp[1].processSample(inR[i]);
                bpR = lp[1].processSample(bpR);

                inL[i] += bpL * comp;
                inR[i] += bpR * comp;
                break;
            }
        }
    }

private:
    float lastSR = 44100.f;

    float threshold = std::pow(10.f, -18.f / 20.f);
    float lastEnv = 0.f, lastGR = 0.f;
    float xm0 = 0.f, xm1 = 0.f;

    dsp::IIR::Filter<float> sc_hp, sc_lp, lp[2], hp[2];

    Type type;
};

struct OptoModel
{
    OptoModel() = default;

    void prepare(dsp::ProcessSpec& spec)
    {
        SR = spec.sampleRate;
    }

    void reset()
    {
        z[0] = 0.f, z[1] = 0.f;
        x_m[0] = 0.f, x_m[1] = 0.f;
    }

    inline float processControlSignal(int ch, float x)
    {
        float s = (2.f / SR) * ((z[ch] - 1.f) / (z[ch] + 1.f));
        auto Vbs = ((1.f + s * Cc * (R4 + R5)) / (1.f + s * Cc * R4)) * x * s;
        auto VB = 3.9f;
        auto Ia = (Vbs * s / R5) + (Vs / (R3 * (1.f + R1 / R2)));

        auto ref = (alpha * n * Vt) / (G * (R6 + R7 - 1.f / (alpha * beta)));
        auto pos = 600.f * ref;
        auto neg = -600.f * ref;

        float V3;

        if (Ia <= neg)
        {
            V3 = -Ia / (alpha * beta);
        }
        else if (Ia > neg && Ia < pos)
        {
            V3 = ((-alpha / G) * n * Vt * (x - 6.386f) * (Ia * (1.f / ref))) - Ia / (alpha * beta);
        }
        else if (Ia >= pos)
        {
            V3 = 6.386f * (alpha / G) * n * Vt - Ia * (R6 + R7);
        }

        float If;

        auto case1 = alpha * (Ifmin - beta * V3);
        auto case2 = VB / (R6 + R7);
        auto case3 = (0.001f * G * VB + (alpha * R9) * (VB * beta + Ifmax)) / (0.001f * G * (R6 + R7) + R9);

        if (Ia <= case1)
        {
            If = Ifmin;
        }
        else if (Ia > case1 && Ia <= case2)
        {
            If = beta * V3 + (Ia / alpha);
        }
        else if (Ia > case2 && Ia < case3)
        {
            If = ((0.001f * G * (Ia * (R6 + R7) - VB)) / (alpha * R9)) - beta * VB + Ia / alpha;
        }
        else if (Ia >= case3)
        {
            If = Ifmax;
        }

        z[ch] = x;

        return If;
    }

    inline float compress(int ch, float x)
    {
        auto env = processControlSignal(ch, x);

        float Rf = A / (std::pow(env, 1.4f)) + B;

        auto y = x * Rf;

        x_m[ch] = y;

        return y;
    }

    void process(AudioBuffer<float>& buffer, float comp)
    {
        comp = jmap(comp, 1.f, 4.f);

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto in = buffer.getWritePointer(ch);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                in[i] *= comp;
                in[i] = compress(ch, in[i]);
            }
        }
    }

private:
    const float A = std::pow(3.464f, 1.4f), B = 1136.212f, G = 2e5f, Is = 2.7551e-9f, n = 3.9696f, Vt = 26e-3f,
        R1 = 5e3f, R2 = 5e3f, R3 = 150e3f, R4 = 470e3f, R5 = 100e3f, R6 = 20e3f, R7 = 33e3f, R8 = 4.7e3f, R9 = 470.f,
        Cc = 2e-9f, Vs = 15.f, Ifmin = 10e-6f, Ifmax = 40e-3f;
    const float alpha = 1.f + (R6 + R7) * (1.f / R3 + 1.f / R5), beta = (1.f / alpha - 1.f) / (R6 + R7) - 1.f / R8;
    float SR = 0.f, z[2]{ 0.f, 0.f }, x_m[2]{ 0.f, 0.f };
};