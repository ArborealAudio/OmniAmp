/*
  ==============================================================================

    ToneStack.h
    Nodal tone stack for guitar amp sims
    Based on a Fender Bassman tone stack
  ==============================================================================
*/

#pragma once

template <typename T>
struct NodalCoeffs
{
    // NodalCoeffs(T c1, T c2, T c3, T r1, T r2, T r3, T r4) : C1(c1), C2(c2), C3(c3), R1(r1), R2(r2), R3(r3), R4(r4)
    // {
    // }

    NodalCoeffs() = default;

    NodalCoeffs & operator=(const NodalCoeffs &newC)
    {
        return *(NodalCoeffs *)memcpy(this, &newC, sizeof(newC));
    }
    
    void prepare(const dsp::ProcessSpec &spec)
    {
        c = spec.sampleRate * 2.0;
    }

    void setCoeffs(T c1, T c2, T c3, T r1, T r2, T r3, T r4) noexcept
    {
        C1 = c1; C2 = c2; C3 = c3; R1 = r1; R2 = r2; R3 = r3; R4 = r4;
    }

    void setToneControls(double bass, double mid, double treble) noexcept
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

    void reset() noexcept
    {
        z1[0] = z2[0] = z3[0],
        z1[1] = z2[1] = z3[1],
        x1[0] = x2[0] = x3[0],
        x1[1] = x2[1] = x3[1] = 0.0;
    }

    void processSamples(T *x, size_t ch, size_t numSamples) noexcept
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

    inline T processSample(T x, size_t ch) noexcept
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
    T C1 = 0.25e-9f, C2 = 22e-9f, C3 = 22e-9f, R1 = 300e3f, R2 = 0.5e6f, R3 = 30e3f, R4 = 56e3f;
};

template <typename T>
struct Biquads
{
    Biquads() = default;

    Biquads & operator=(const Biquads &newC)
    {
        return *(Biquads *)memcpy(this, &newC, sizeof(newC));
    }

    /**
     * @param pLP cutoff for passive LP
     * @param pHP cutoff for passive HP
     * @param L cutoff for low freq control
     * @param M cutoff for mid freq control
     * @param H cutoff for hi freq control
    */
    void setParams(double pLow, double pHi, double LF, double MF, double HF) noexcept
    {
        pLPFreq = pLow;
        pLP.setCutoffFreq(pLPFreq);
        pHPFreq = pHi;
        pHP.setCutoffFreq(pHPFreq);
        LFreq = LF;
        L.setCutoffFreq(LFreq);
        MFreq = MF;
        M.setCutoffFreq(MFreq);
        HFreq = HF;
        H.setCutoffFreq(HFreq);
    }

    void prepare(const dsp::ProcessSpec &spec)
    {
        for (auto *f : getFilters())
            f->prepare(spec);
        pLP.setType(strix::FilterType::lowpass);
        pLP.setCutoffFreq(pLPFreq);
        pHP.setType(strix::FilterType::highpass);
        pHP.setCutoffFreq(pHPFreq);
        L.setType(strix::FilterType::firstOrderLowpass);
        L.setCutoffFreq(LFreq);
        M.setType(strix::FilterType::bandpass);
        M.setCutoffFreq(MFreq);
        H.setType(strix::FilterType::firstOrderHighpass);
        H.setCutoffFreq(HFreq);
    }

    void setToneControls(double bass, double mid, double treble) noexcept
    {
        lowGain = jmap(bass, -1.0, 1.0);
        midGain = jmap(mid, -1.0, 1.0);
        hiGain = jmap(treble, -1.0, 1.0);
    }

    void reset() noexcept
    {
        for (auto *f : getFilters())
            f->reset();
    }

    void processSamples(T *in, size_t ch, size_t numSamples) noexcept
    {
        for (size_t i = 0; i < numSamples; ++i)
        {
            in[i] = processSample(in[i], ch);
        }
    }

    inline T processSample(T in, size_t ch) noexcept
    {
        auto lp = pLP.processSample(ch, in);
        auto hp = pHP.processSample(ch, in);
        in = (lp * 0.5) + hp;
        in += lowGain * L.processSample(ch, in);
        in += midGain * M.processSample(ch, in);
        in += hiGain * H.processSample(ch, in);
        return in;
    }

private:
    strix::SVTFilter<T> pLP, pHP, L, M, H;
    double lowGain = 0.0, midGain = 0.0, hiGain = 0.0;
    double pLPFreq, pHPFreq, LFreq, MFreq, HFreq;

    std::vector<strix::SVTFilter<T>*> getFilters()
    {
        return {
            &pLP,
            &pHP,
            &L,
            &M,
            &H
        };
    }
};

template <typename T>
struct ToneStack : PreampProcessor
{
    enum Type
    {
        Nodal,
        Biquad
    };
    Type type;

    ToneStack(Type t) : type(t) 
    {}

    void setNodalCoeffs(T c1, T c2, T c3, T r1, T r2, T r3, T r4)
    {
        type = Nodal;
        nCoeffs.setCoeffs(c1, c2, c3, r1, r2, r3, r4);
        updateCoeffs();
    }

    void setBiquadFreqs(double pLow, double pHi, double LF, double MF, double HF)
    {
        type = Biquad;
        bCoeffs.setParams(pLow, pHi, LF, MF, HF);
        updateCoeffs();
    }

    void prepare(const dsp::ProcessSpec &spec)
    {
        lastSpec = spec;

        nCoeffs.prepare(spec);
        bCoeffs.prepare(spec);

        bass.reset(spec.sampleRate, 0.005);
        mid.reset(spec.sampleRate, 0.005);
        treble.reset(spec.sampleRate, 0.005);

        updateCoeffs();
    }

    void reset()
    {
        nCoeffs.reset();
        bCoeffs.reset();
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
        nCoeffs.setToneControls(bass.getNextValue(), mid.getNextValue(), treble.getNextValue());
        bCoeffs.setToneControls(std::sqrt(std::sqrt(bass.getNextValue())), mid.getNextValue(), treble.getNextValue());
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
                    in[i] = type ? bCoeffs.processSample(in[i], ch) : nCoeffs.processSample(in[i], ch);
                }
            }
            return;
        }

        for (int ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto in = block.getChannelPointer(ch);

            type ? bCoeffs.processSamples(in, ch, block.getNumSamples()) : nCoeffs.processSamples(in, ch, block.getNumSamples());
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
                    in[i] = type ? bCoeffs.processSample(in[i], ch) : nCoeffs.processSample(in[i], ch);
                }
            }
            return;
        }

        for (int ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto in = block.getChannelPointer(ch);

            type ? bCoeffs.processSamples(in, ch, block.getNumSamples()) : nCoeffs.processSamples(in, ch, block.getNumSamples());
        }
    }
#endif

    NodalCoeffs<T> nCoeffs;
    Biquads<T> bCoeffs;

private:
    SmoothedValue<double> bass, mid, treble;
    dsp::ProcessSpec lastSpec;
};