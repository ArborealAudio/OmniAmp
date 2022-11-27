/*
  ==============================================================================

    ToneStack.h
    Nodal tone stack for guitar amp sims
    Based on a Fender Bassman tone stack
  ==============================================================================
*/

#pragma once

template <typename T>
struct ToneStackNodal : PreampProcessor
{
    struct Coeffs
    {
        Coeffs() = default;

        Coeffs(T c1, T c2, T c3, T r1, T r2, T r3, T r4) : C1(c1), C2(c2), C3(c3), R1(r1), R2(r2), R3(r3), R4(r4)
        {
        }

        Coeffs &operator=(const Coeffs &newCoeffs)
        {
            return *(Coeffs *) memcpy(this, &newCoeffs, sizeof(newCoeffs));
        }

        void prepare(double sampleRate)
        {
            c = sampleRate * 2.0;
        }

        void setCoeffs(T bass, T mid, T treble)
        {
            b1 = (treble * C1 * R1) + (mid * C3 * R3) + (bass * (C1 * R2 + C2 * R2)) + (C1 * R3 + C2 * R3);

            b2 = (treble * (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4)) - mid * mid * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3) + mid * (C1 * C3 * R1 * R3 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3) + bass * (C1 * C2 * R1 * R2 + C1 * C2 * R2 * R4 + C1 * C3 * R2 * R4) + bass * mid * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3) + (C1 * C2 * R1 * R3 + C1 * C2 * R3 * R4 + C1 * C3 * R3 * R4);

            b3 = bass * mid * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4) - mid * mid * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4) + mid * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4) + treble * (C1 * C2 * C3 * R1 * R3 * R4) - treble * mid * (C1 * C2 * C3 * R1 * R3 * R4) + treble * bass * (C1 * C2 * C3 * R1 * R2 * R4);

            a0 = 1.0;

            a1 = (C1 * R1 + C1 * R3 + C2 * R3 + C2 * R4 + C3 * R4) + mid * C3 * R3 + bass * (C1 * R2 + C2 * R2);

            a2 = mid * (C1 * C3 * R1 * R3 - C2 * C3 * R3 * R4 + C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3) + bass * mid * (C1 * C3 * R2 * R3 + C2 * C3 * R2 * R3) - mid * mid * (C1 * C3 * R3 * R3 + C2 * C3 * R3 * R3) + bass * (C1 * C2 * R2 * R4 + C1 * C2 * R1 * R2 + C1 * C3 * R2 * R4 + C2 * C3 * R2 * R4) + (C1 * C2 * R1 * R4 + C1 * C3 * R1 * R4 + C1 * C2 * R3 * R4 + C1 * C2 * R1 * R3 + C1 * C3 * R3 * R4 + C2 * C3 * R3 * R4);

            a3 = bass * mid * (C1 * C2 * C3 * R1 * R2 * R3 + C1 * C2 * C3 * R2 * R3 * R4) - mid * mid * (C1 * C2 * C3 * R1 * R3 * R3 + C1 * C2 * C3 * R3 * R3 * R4) + mid * (C1 * C2 * C3 * R3 * R3 * R4 + C1 * C2 * C3 * R1 * R3 * R3 - C1 * C2 * C3 * R1 * R3 * R4) + bass * C1 * C2 * C3 * R1 * R2 * R4 + C1 * C2 * C3 * R1 * R3 * R4;

            /*discretize*/

            B0 = -b1 * c - b2 * c * c - b3 * c * c * c;
            B1 = -b1 * c + b2 * c * c + (T)3.0 * b3 * c * c * c;
            B2 = b1 * c + b2 * c * c - (T)3.0 * b3 * c * c * c;
            B3 = b1 * c - b2 * c * c + b3 * c * c * c;
            A0 = -a0 - a1 * c - a2 * c * c - a3 * c * c * c;
            A1 = (T)-3.0 * a0 - a1 * c + a2 * c * c + (T)3.0 * a3 * c * c * c;
            A2 = (T)-3.0 * a0 + a1 * c + a2 * c * c - (T)3.0 * a3 * c * c * c;
            A3 = -a0 + a1 * c - a2 * c * c + a3 * c * c * c;
        }

        void reset()
        {
            z1[0] = z2[0] = z3[0],
            z1[1] = z2[1] = z3[1],
            x1[0] = x2[0] = x3[0],
            x1[1] = x2[1] = x3[1] = 0.0;
        }

        void processSamples(T *x, size_t ch, size_t numSamples)
        {
            for (size_t i = 0; i < numSamples; ++i)
            {
                auto y = (1.f / A0) * (B0 * x[i] + B1 * x1[ch] + B2 * x2[ch] + B3 * x3[ch] - A1 * z1[ch] - A2 * z2[ch] - A3 * z3[ch]);

                z3[ch] = z2[ch];
                z2[ch] = z1[ch];
                z1[ch] = y;

                x3[ch] = x2[ch];
                x2[ch] = x1[ch];
                x1[ch] = x[i];

                x[i] = y;
            }
        }

        inline T processSample(T x, size_t ch)
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

    private:
        T c = 88200.0, b1 = 0, b2 = 0, b3 = 0, a0 = 0, a1 = 0, a2 = 0, a3 = 0, B0 = 0, B1 = 0, B2 = 0, B3 = 0, A0 = 1.0, A1 = 0, A2 = 0, A3 = 0;

        T z1[2]{0.f}, z2[2]{0.f}, z3[2]{0.f}, x1[2]{0.f}, x2[2]{0.f}, x3[2]{0.f};
        const T C1 = 0.25e-9f, C2 = 22e-9f, C3 = 22e-9f, R1 = 300e3f, R2 = 0.5e6f, R3 = 30e3f, R4 = 56e3f;
    };

    ToneStackNodal(Coeffs c)
    {
        coeffs = std::make_unique<Coeffs>(c);
        bass.setTargetValue(0.5);
        mid.setTargetValue(0.5);
        treble.setTargetValue(0.5);
    }

    void changeCoeffs(Coeffs newC)
    {
        coeffs.reset(new Coeffs(newC));
        coeffs->prepare(SR);
        updateCoeffs();
    }

    void prepare(const dsp::ProcessSpec &spec)
    {
        SR = spec.sampleRate;

        coeffs->prepare(spec.sampleRate);

        bass.reset(spec.sampleRate, 0.005);
        mid.reset(spec.sampleRate, 0.005);
        treble.reset(spec.sampleRate, 0.005);

        updateCoeffs();
    }

    void reset()
    {
        coeffs->reset();
    }

    void setBass(double b)
    {
        bass.setTargetValue(b);
    }

    void setMid(double m)
    {
        mid.setTargetValue(m);
    }

    void setTreble(double t)
    {
        treble.setTargetValue(t);
    }

    void updateCoeffs()
    {
        coeffs->setCoeffs(bass.getNextValue(), mid.getNextValue(), treble.getNextValue());
    }

#if USE_SIMD
    void process(strix::AudioBlock<vec> &block) override
    {
        if (bass.isSmoothing() || mid.isSmoothing() || treble.isSmoothing())
        {
            for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
            {
                auto in = block.getChannelPointer(ch);
                for (size_t i = 0; i < block.getNumSamples(); ++i)
                {
                    updateCoeffs();
                    in[i] = coeffs->processSample(in[i], ch);
                }
            }
            return;
        }

        assert(coeffs != nullptr);

        for (int ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto in = block.getChannelPointer(ch);

            coeffs->processSamples(in, ch, block.getNumSamples());
        }
    }
#else
    void process(dsp::AudioBlock<double> &block) override
    {
        if (bass.isSmoothing() || mid.isSmoothing() || treble.isSmoothing())
        {
            for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
            {
                auto in = block.getChannelPointer(ch);
                for (size_t i = 0; i < block.getNumSamples(); ++i)
                {
                    updateCoeffs();
                    in[i] = coeffs->processSample(in[i], ch);
                }
            }
            return;
        }

        assert(coeffs != nullptr);

        for (int ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto in = block.getChannelPointer(ch);

            coeffs->processSamples(in, ch, block.getNumSamples());
        }
    }
#endif

    std::unique_ptr<Coeffs> coeffs = nullptr;

private:
    SmoothedValue<double> bass, mid, treble;
    double SR = 44100.0;
};