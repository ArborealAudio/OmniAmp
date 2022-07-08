/*
  ==============================================================================

    WDFtest.h
    Created: 25 Feb 2022 7:38:32pm
    Author:  alexb

  ==============================================================================
*/

#pragma once

//using namespace chowdsp::WDFT;
//
//struct HFEnhancer
//{
//    HFEnhancer() = default;
//
//    void prepare(dsp::ProcessSpec& spec)
//    {
//        C1.prepare(spec.sampleRate);
//        C2.prepare(spec.sampleRate);
//        
//        hp.prepare(spec);
//        *hp.coefficients = dsp::IIR::ArrayCoefficients<float>::makeHighPass(spec.sampleRate, 1500.f);
//    }
//
//    void reset()
//    {
//        C1.reset();
//        C2.reset();
//        hp.reset();
//    }
//
//    void setCutoffFrequency(float cutoff)
//    {
//        const auto Cap = 5.0e-8f;
//        const auto Res = 1.0 / (MathConstants<float>::pi * cutoff * Cap);
//
//        R1.setResistanceValue(Res);
//        R2.setResistanceValue(Res);
//        C1.setCapacitanceValue(Cap);
//        C2.setCapacitanceValue(Cap);
//    }
//
//    inline float processSample(float x)
//    {
//        Vs.setVoltage(x);
//
//        dp.incident(P1.reflected());
//        auto y = voltage<float>(R1);
//        P1.incident(dp.reflected());
//
//        return y;
//    }
//
//    void process(AudioBuffer<float>& buffer, float mix)
//    {
//        auto in = buffer.getWritePointer(0);
//
//        for (int i = 0; i < buffer.getNumSamples(); ++i)
//        {
//            in[i] += mix * processSample(in[i]);
//        }
//
//        buffer.copyFrom(1, 0, in, buffer.getNumSamples());
//    }
//
//private:
//    
//    ResistorT<float> R1{ 1000.f }, R2{ 1000.f };
//    CapacitorT<float> C1{ 5.0e-8f }, C2{ 5.0e-8f };
//
//    ResistiveVoltageSourceT<float> Vs;
//    WDFSeriesT<float, decltype(Vs), decltype(C1)> S1{ Vs, C1 };
//    WDFParallelT<float, decltype(S1), decltype(R1)> P1{ S1, R1 };
//    
//    DiodePairT<float, decltype(P1)> dp{ P1, 2.52e-9f};
//
//    dsp::IIR::Filter<float> hp;
//};
//
//struct ToneStack
//{
//    ToneStack() = default;
//
//    void prepare(double sampleRate)
//    {
//        C1.prepare(sampleRate);
//        C2.prepare(sampleRate);
//        C3.prepare(sampleRate);
//    }
//
//    void reset()
//    {
//        C1.reset();
//        C2.reset();
//        C3.reset();
//    }
//
//    void setCircuitParams(float low, float mid, float hi)
//    {
//        R1.setResistanceValue(250e3 * (hi + 0.5));
//        R2.setResistanceValue(1e6 * (low + 0.5));
//        R3.setResistanceValue(25e3 * (mid + 0.5));
//    }
//
//    inline float processSample(float x)
//    {
//        ResVs.setVoltage(x);
//
//        ResVs.incident(S1.reflected());
//        auto y = voltage<float>(R1);
//        S1.incident(ResVs.reflected());
//        
//        return y;
//    }
//
//    void process(AudioBuffer<float>& buffer)
//    {
//        auto in = buffer.getWritePointer(0);
//
//        for (int i = 0; i < buffer.getNumSamples(); ++i)
//        {
//            in[i] = processSample(in[i]);
//        }
//
//        buffer.copyFrom(1, 0, in, buffer.getNumSamples());
//    }
//
//private:
//
//    ResistorT<float> R1{ 250e3f }, R2{ 1e6f }, R3{ 25e3f }, R4{ 56e3f };
//    CapacitorT<float> C1{ 0.25e-9f }, C2{ 20e-9f }, C3{ 20e-9f };
//
//    ResistiveVoltageSourceT<float> ResVs{56e3f};
//
//    WDFSeriesT<float, decltype(C3), decltype(ResVs)> S5{ C3, ResVs };
//    WDFSeriesT<float, decltype(R3), decltype(S5)> S4{ R3, S5 };
//    WDFSeriesT<float, decltype(R2), decltype(S4)> S3{ R2, S4 };
//    WDFParallelT<float, decltype(C2), decltype(S3)> P1{ C2, S3 };
//    WDFSeriesT<float, decltype(R1), decltype(P1)> S2{ R1, P1 };
//    WDFSeriesT<float, decltype(C1), decltype(S2)> S1{ C1, S2 };
//
//    //IdealVoltageSourceT<float, decltype(S1)> Vs{ S1 };
//};

template <typename T>
struct ToneStackNodal
{
    ToneStackNodal(T c1, T c2, T c3, T r1, T r2, T r3, T r4) :
        C1(c1), C2(c2), C3(c3), R1(r1), R2(r2), R3(r3), R4(r4)
    {}

    void prepare(const dsp::ProcessSpec& spec)
    {
        c = spec.sampleRate * 2.0;
        setCoeffs();
    }

    void reset()
    {
        z1[0] = z2[0] = z3[0],
        z1[1] = z2[1] = z3[1],
        x1[0] = x2[0] = x3[0],
        x1[1] = x2[1] = x3[1] = 0.0;
    }

    void setBass(T b)
    {
        bass = b;
        setCoeffs();
    }

    void setMid(T m)
    {
        mid = m;
        setCoeffs();
    }

    void setTreble(T t)
    {
        treble = t;
        setCoeffs();
    }

    void setCoeffs()
    {
        b1 = (treble * C1 * R1) + (mid * C3 * R3) + (bass * (C1 * R2 + C2 * R2)) + (C1 * R3 + C2 * R3);

        b2 = (treble * (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4)) - mid * mid * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3) + mid * (C1 * C3 * R1 * R3 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3) + bass * (C1 * C2 * R1 * R2 + C1 * C2 * R2 * R4 + C1 * C3 * R2 * R4) + bass * mid * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3) + (C1 * C2 * R1 * R3 + C1 * C2 * R3 * R4 + C1 * C3 * R3 * R4);

        b3 = bass * mid * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4) - mid * mid * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4) + mid * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4) + treble * (C1 * C2 * C3 * R1 * R3 * R4) - treble * mid * (C1 * C2 * C3 * R1 * R3 * R4) + treble * bass * (C1*C2*C3*R1*R2*R4);

        a0 = 1.0;

        a1 = (C1 * R1 + C1 * R3 + C2 * R3 + C2 * R4 + C3 * R4) + mid * C3 * R3 + bass * (C1 * R2 + C2 * R2);

        a2 = mid * (C1 * C3 * R1 * R3 - C2 * C3 * R3 * R4 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3) + bass * mid * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3) - mid * mid * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3) + bass * (C1 * C2 * R2 * R4 + C1 * C2 * R1 * R2 + C1 * C3 * R2 * R4 + C2 * C3 * R2 * R4) + (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4 + C1 * C2 * R3 * R4 + C1 * C2 * R1 * R3 + C1 * C3 * R3 * R4 + C2 * C3 * R3 * R4);

        a3 = bass * mid * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4) - mid * mid * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4) + mid * (C1 * C2 * C3 * R3 * R3 * R4 + C1 * C2 * C3 * R1 * R3 * R3 - C1 * C2 * C3 * R1 * R3 * R4) + bass * C1 * C2 * C3 * R1 * R2 * R4 + C1 * C2 * C3 * R1 * R3 * R4;

        discretize();
    }

    void discretize()
    {
        B0 = -b1 * c - b2 * c * c - b3 * c * c * c;
        B1 = -b1 * c + b2 * c * c + (T)3.0 * b3 * c * c * c;
        B2 = b1 * c + b2 * c * c - (T)3.0 * b3 * c * c * c;
        B3 = b1 * c - b2 * c * c + b3 * c * c * c;
        A0 = -a0 - a1 * c - a2 * c * c - a3 * c * c * c;
        A1 = (T)-3.0 * a0 - a1 * c + a2 * c * c + (T)3.0 * a3 * c * c * c;
        A2 = (T)-3.0 * a0 + a1 * c + a2 * c * c - (T)3.0 * a3 * c * c * c;
        A3 = -a0 + a1 * c - a2 * c * c + a3 * c * c * c;
    }

    inline T processSample(T x, int ch)
    {
        auto y = (1.f / A0) * (B0 * x + B1 * x1[ch] + B2 * x2[ch] + B3 * x3[ch] - A1 * z1[ch] - A2 * z2[ch] - A3 * z3[ch]);

        z3[ch] = z2[ch];
        z2[ch] = z1[ch];
        z1[ch] = y;

        x3[ch] = x2[ch];
        x2[ch] = x1[ch];
        x1[ch] = x;

        return y;
    }

    void process(AudioBuffer<T>& buffer)
    {
        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
        {
            auto in = buffer.getWritePointer(ch);

            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                in[i] = processSample(in[i], ch);
            }
        }
    }

    void processBlock(dsp::AudioBlock<T>& block)
    {
        for (int ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto in = block.getChannelPointer(ch);

            for (int i = 0; i < block.getNumSamples(); ++i)
            {
                in[i] = processSample(in[i], ch);
            }
        }
    }

    void processBlock(chowdsp::AudioBlock<T>& block)
    {
        for (int ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto in = block.getChannelPointer(ch);

            for (int i = 0; i < block.getNumSamples(); ++i)
            {
                in[i] = processSample(in[i], ch);
            }
        }
    }

private:
    T bass = 0.5, mid = 0.5, treble = 0.5, c = 0, b1 = 0, b2 = 0, b3 = 0, a0 = 0, a1 = 0, a2 = 0, a3 = 0, B0 = 0, B1 = 0, B2 = 0, B3 = 0, A0 = 0, A1 = 0, A2 = 0, A3 = 0;

    T z1[2]{ 1.f, 1.f }, z2[2]{ 1.f, 1.f }, z3[2]{ 1.f, 1.f }, x1[2]{ 1.f, 1.f }, x2[2]{ 1.f, 1.f }, x3[2]{ 1.f, 1.f };

    const T C1 = 0.25e-9f, C2 = 22e-9f, C3 = 22e-9f, R1 = 300e3f, R2 = 0.5e6f, R3 = 30e3f, R4 = 56e3f;
};