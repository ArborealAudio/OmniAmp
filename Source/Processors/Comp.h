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
    OptoComp() = default;

    void prepare(const dsp::ProcessSpec& spec)
    {
        lastSR = spec.sampleRate;
    }

    void reset()
    {
        xm0 = 0.f, xm1 = 0.f, lastEnv = 0.f;
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
        //auto env = detectEnv(x); //resistance envelope
        
        //auto det = jmax(0.f, x);
        if (x < 1.175494351e-38f)
            x = 1.175494351e-38f;

        auto env = jmax(0.f, 8.685889638f * std::log10(x / threshold));
        auto t_env = env - lastEnv;

        float att = std::exp(-1.f / ((1.f/x) * 0.015f * lastSR));
        float rel = std::exp(-1.f / (0.05f * lastSR));
        float rel2 = std::exp(-1.f / (2.f * lastSR));

        if (env > lastEnv)
        {
            env = env + att * (lastEnv - env);
        }
        else
        {
            auto rel_env1 = env + rel * (lastEnv - env);
            auto rel_env2 = env + rel2 * (lastEnv - env);

            env = (rel_env1 * (1.f-t_env)) + (rel_env2 * t_env);
            /*this method is working well, but getting opposite of desired behavior.
            perhaps using prev and current gr will get better results than using the env*/
        }

        lastEnv = env;
        
        auto gr_db = 10.f * -env; /*the ratio was applied here as (ratio - 1)/ratio. An opto ratio should logarithmically
                           taper back towards unity as GR increases*/
        auto gr = std::pow(10.f, gr_db / 20.f);
        lastGR = gr;

        return gr;
    }

    void process(AudioBuffer<float>& buffer, float comp)
    {
        auto inL = buffer.getWritePointer(0);
        auto inR = buffer.getWritePointer(1);

        comp = jmap(comp, 1.f, 4.f);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            //const float smooth = 0.0001f;
            //auto abs0 = std::sqrt(xm0 * xm0 + smooth) - std::sqrt(smooth);
            //auto abs1 = std::sqrt(xm1 * xm1 + smooth) - std::sqrt(smooth);
            float abs0 = std::abs(xm0);
            float abs1 = std::abs(xm1);

            auto gr = compress(jmax(abs0, abs1));

            inL[i] *= gr * comp;
            inR[i] *= gr * comp;

            xm0 = inL[i];
            xm1 = inR[i];

            inL[i] /= comp;
            inR[i] /= comp;
        }
    }

private:
    float lastSR = 44100.f;

    const float threshold = std::pow(10.f, -18.f / 20.f);
    const float e = MathConstants<float>::euler;
    float lastEnv = 0.f, lastGR = 0.f;
    float xm0 = 0.f, xm1 = 0.f;
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