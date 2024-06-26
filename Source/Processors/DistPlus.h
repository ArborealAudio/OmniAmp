// DistPlus.h

#pragma once
namespace WDFT = chowdsp::wdft;

template <typename T> class MXRDistWDF
{
  public:
    // whether or not the process method will call an init method within itself
    bool init = true;

    MXRDistWDF() = default;

    void prepare(const dsp::ProcessSpec &spec)
    {
        SR = spec.sampleRate;
        C1.prepare((T)SR);
        C2.prepare((T)SR);
        C3.prepare((T)SR);
        C4.prepare((T)SR);
        C5.prepare((T)SR);

        Vb.setVoltage((T)4.5f);

        dist.reset(SR, 0.01);
        updateParams();

        dcBlock.prepare(spec);
        dcBlock.setCutoffFreq(10.0);
        dcBlock.setType(strix::FilterType::highpass);

        dry.setSize(spec.numChannels, spec.maximumBlockSize);
        for (size_t i = 0; i < spec.maximumBlockSize; i++) {
            processInit(0.5);
        }

        setInit(true);
    }

    /*set the target value for the distortion param*/
    void setParams(double distParam) { dist.setTargetValue(distParam); }

    /*update the distortion param from smoothed value*/
    void updateParams()
    {
        ResDist_R3.setResistanceValue(dist.getNextValue() * rDistVal + R3Val);
    }

    void setInit(bool isInit)
    {
        init = isInit;
        fade.setFadeTime(SR, 0.5f);
    }

    inline T processSample(T x)
    {
        Vin.setVoltage(x);

        DP.incident(P3.reflected());
        P3.incident(DP.reflected());

        return WDFT::voltage<T>(Rout);
    }

#if !USE_SIMD
    /*for reg doubles currently just one channel, copies L->R*/
    void processBlock(dsp::AudioBlock<T> &block)
    {
        auto *inL = block.getChannelPointer(0);
        FloatVectorOperations::copy(dry.getWritePointer(0), inL,
                                    block.getNumSamples());
        auto *inR = inL;
        if (block.getNumChannels() > 1) {
            inR = block.getChannelPointer(1);
            FloatVectorOperations::copy(dry.getWritePointer(1), inR,
                                        block.getNumSamples());
        }
        dsp::AudioBlock<T> dryBlock(dry.getArrayOfWritePointers(),
                                    block.getNumChannels(),
                                    block.getNumSamples());

        if (dist.isSmoothing()) {
            for (auto i = 0; i < block.getNumSamples(); ++i) {
                updateParams();
                inL[i] = processSample(inL[i]);
            }
            FloatVectorOperations::copy(inR, inL, block.getNumSamples());
            if (init) {
                fade.processWithState(
                    dryBlock, block,
                    jmin(block.getNumSamples(), dryBlock.getNumSamples()));
                if (fade.complete)
                    init = false;
            }
            dcBlock.processBlock(block);
            return;
        }

        updateParams();

        for (auto i = 0; i < block.getNumSamples(); ++i) {
            inL[i] = processSample(inL[i]);
        }
        FloatVectorOperations::copy(inR, inL, block.getNumSamples());
        if (init) {
            fade.processWithState(
                dryBlock, block,
                jmin(block.getNumSamples(), dryBlock.getNumSamples()));
            if (fade.complete)
                init = false;
        }
        dcBlock.processBlock(block);
    }

    void processBlockInit(dsp::AudioBlock<T> &block)
    {
        auto *in = block.getChannelPointer(0);

        updateParams();
        for (size_t i = 0; i < block.getNumSamples(); ++i) {
            processInit(in[i]);
        }
    }
#else
    void processBlock(strix::AudioBlock<T> &block)
    {
        auto *in = block.getChannelPointer(0);
        memcpy(dry.getWritePointer(0), in, sizeof(T) * block.getNumSamples());
        strix::AudioBlock<T> dryBlock(dry.getArrayOfWritePointers(),
                                      block.getNumChannels(),
                                      block.getNumSamples());
        if (dist.isSmoothing()) {
            for (size_t i = 0; i < block.getNumSamples(); ++i) {
                updateParams();
                in[i] = processSample(in[i]);
            }
            if (init) {
                fade.processWithState(
                    dryBlock, block,
                    jmin(block.getNumSamples(), dryBlock.getNumSamples()));
                if (fade.complete)
                    init = false;
            }
            dcBlock.processBlock(block);
            return;
        }

        updateParams();

        for (size_t i = 0; i < block.getNumSamples(); ++i) {
            in[i] = processSample(in[i]);
        }
        if (init) {
            fade.processWithState(
                dryBlock, block,
                jmin(block.getNumSamples(), dryBlock.getNumSamples()));
            if (fade.complete)
                init = false;
        }
        dcBlock.processBlock(block);
    }

    void processBlockInit(strix::AudioBlock<T> &block)
    {
        auto *in = block.getChannelPointer(0);

        updateParams();
        for (size_t i = 0; i < block.getNumSamples(); ++i) {
            processInit(in[i]);
        }
    }

#endif
  private:
    // propagate signal without returning anything
    inline void processInit(T x)
    {
        Vin.setVoltage(x);

        DP.incident(P3.reflected());
        P3.incident(DP.reflected());
        // dcBlock.processSample(0, x);
        // dcBlock.processSample(1, x);
    }

    double SR = 44100.0;

#if USE_SIMD
    strix::Buffer<T> dry;
#else
    AudioBuffer<T> dry;
#endif
    strix::Crossfade fade;

    // Port A
    WDFT::ResistorT<T> R4{1.0e6f};

    // Port B
    WDFT::ResistiveVoltageSourceT<T> Vin;
    WDFT::CapacitorT<T> C1{1.0e-9f};
    WDFT::WDFParallelT<T, decltype(Vin), decltype(C1)> P1{Vin, C1};

    WDFT::ResistorT<T> R1{10.0e3f};
    WDFT::CapacitorT<T> C2{10.0e-9f};
    WDFT::WDFSeriesT<T, decltype(R1), decltype(C2)> S1{R1, C2};

    WDFT::WDFSeriesT<T, decltype(S1), decltype(P1)> S2{S1, P1};
    WDFT::ResistiveVoltageSourceT<T> Vb{1.0e6f}; // encompasses R2
    WDFT::WDFParallelT<T, decltype(Vb), decltype(S2)> P2{Vb, S2};

    // Port C
    static constexpr float R3Val = 4.7e3f;
    static constexpr float rDistVal = 1.0e6f;
    WDFT::ResistorT<T> ResDist_R3{rDistVal + R3Val}; // distortion potentiometer
    WDFT::CapacitorT<T> C3{47.0e-9f};
    WDFT::WDFSeriesT<T, decltype(ResDist_R3), decltype(C3)> S4{ResDist_R3, C3};

    struct ImpedanceCalc
    {
        template <typename RType> static T calcImpedance(RType &R)
        {
            const T A = 100.0f;   // op-amp gain
            const T Ri = 1.0e9f;  // op-amp input impedance
            const T Ro = 1.0e-1f; // op-amp output impedance

            const auto [Ra, Rb, Rc] = R.getPortImpedances();

            R.setSMatrixData(
                {{-(Ra * Ra * Rb * Rb + 2 * Ra * Ra * Rb * Rc +
                    (Ra * Ra - Rb * Rb) * Rc * Rc -
                    ((A + 1) * Rc * Rc - Ra * Ra) * Ri * Ri -
                    ((A + 2) * Rb * Rc * Rc - 2 * Ra * Ra * Rb -
                     2 * Ra * Ra * Rc) *
                        Ri +
                    (Rb * Rb * Rc + Rb * Rc * Rc + Rc * Ri * Ri +
                     (2 * Rb * Rc + Rc * Rc) * Ri) *
                        Ro) /
                      (Ra * Ra * Rb * Rb +
                       (Ra * Ra + 2 * Ra * Rb + Rb * Rb) * Rc * Rc +
                       ((A + 2) * Ra * Rc + (A + 1) * Rc * Rc + Ra * Ra) * Ri *
                           Ri +
                       2 * (Ra * Ra * Rb + Ra * Rb * Rb) * Rc +
                       (2 * Ra * Ra * Rb +
                        ((A + 2) * Ra + (A + 2) * Rb) * Rc * Rc +
                        ((A + 4) * Ra * Rb + 2 * Ra * Ra) * Rc) *
                           Ri -
                       (Ra * Rb * Rb + (Ra + Rb) * Rc * Rc +
                        (Ra + Rc) * Ri * Ri + (2 * Ra * Rb + Rb * Rb) * Rc +
                        (2 * Ra * Rb + 2 * (Ra + Rb) * Rc + Rc * Rc) * Ri) *
                           Ro),
                  -(2 * Ra * Ra * Rb * Rc + 2 * (Ra * Ra + Ra * Rb) * Rc * Rc -
                    (A * Ra * Ra + A * Ra * Rc) * Ri * Ri -
                    (A * Ra * Ra * Rb - (A + 2) * Ra * Rc * Rc +
                     ((A - 2) * Ra * Ra + A * Ra * Rb) * Rc) *
                        Ri -
                    (Ra * Rb * Rc + Ra * Rc * Rc + Ra * Rc * Ri) * Ro) /
                      (Ra * Ra * Rb * Rb +
                       (Ra * Ra + 2 * Ra * Rb + Rb * Rb) * Rc * Rc +
                       ((A + 2) * Ra * Rc + (A + 1) * Rc * Rc + Ra * Ra) * Ri *
                           Ri +
                       2 * (Ra * Ra * Rb + Ra * Rb * Rb) * Rc +
                       (2 * Ra * Ra * Rb +
                        ((A + 2) * Ra + (A + 2) * Rb) * Rc * Rc +
                        ((A + 4) * Ra * Rb + 2 * Ra * Ra) * Rc) *
                           Ri -
                       (Ra * Rb * Rb + (Ra + Rb) * Rc * Rc +
                        (Ra + Rc) * Ri * Ri + (2 * Ra * Rb + Rb * Rb) * Rc +
                        (2 * Ra * Rb + 2 * (Ra + Rb) * Rc + Rc * Rc) * Ri) *
                           Ro),
                  -(2 * Ra * Ra * Rb * Rb +
                    ((A + 2) * Ra * Ra + 2 * (A + 1) * Ra * Rc) * Ri * Ri +
                    2 * (Ra * Ra * Rb + Ra * Rb * Rb) * Rc +
                    ((A + 4) * Ra * Ra * Rb +
                     ((A + 2) * Ra * Ra + 2 * (A + 2) * Ra * Rb) * Rc) *
                        Ri -
                    (Ra * Rb * Rb + Ra * Rb * Rc + Ra * Ri * Ri +
                     (2 * Ra * Rb + Ra * Rc) * Ri) *
                        Ro) /
                      (Ra * Ra * Rb * Rb +
                       (Ra * Ra + 2 * Ra * Rb + Rb * Rb) * Rc * Rc +
                       ((A + 2) * Ra * Rc + (A + 1) * Rc * Rc + Ra * Ra) * Ri *
                           Ri +
                       2 * (Ra * Ra * Rb + Ra * Rb * Rb) * Rc +
                       (2 * Ra * Ra * Rb +
                        ((A + 2) * Ra + (A + 2) * Rb) * Rc * Rc +
                        ((A + 4) * Ra * Rb + 2 * Ra * Ra) * Rc) *
                           Ri -
                       (Ra * Rb * Rb + (Ra + Rb) * Rc * Rc +
                        (Ra + Rc) * Ri * Ri + (2 * Ra * Rb + Rb * Rb) * Rc +
                        (2 * Ra * Rb + 2 * (Ra + Rb) * Rc + Rc * Rc) * Ri) *
                           Ro),
                  (Ra * Rb + Ra * Rc + Ra * Ri) /
                      (Ra * Rb + (Ra + Rb) * Rc + (Ra + Rc) * Ri)},
                 {-(2 * Ra * Rb * Rb * Rc + 2 * (Ra * Rb + Rb * Rb) * Rc * Rc +
                    ((A + 2) * Rb * Rc * Rc + 2 * Ra * Rb * Rc) * Ri -
                    (Rb * Rb * Rc + Rb * Rc * Rc + Rb * Rc * Ri) * Ro) /
                      (Ra * Ra * Rb * Rb +
                       (Ra * Ra + 2 * Ra * Rb + Rb * Rb) * Rc * Rc +
                       ((A + 2) * Ra * Rc + (A + 1) * Rc * Rc + Ra * Ra) * Ri *
                           Ri +
                       2 * (Ra * Ra * Rb + Ra * Rb * Rb) * Rc +
                       (2 * Ra * Ra * Rb +
                        ((A + 2) * Ra + (A + 2) * Rb) * Rc * Rc +
                        ((A + 4) * Ra * Rb + 2 * Ra * Ra) * Rc) *
                           Ri -
                       (Ra * Rb * Rb + (Ra + Rb) * Rc * Rc +
                        (Ra + Rc) * Ri * Ri + (2 * Ra * Rb + Rb * Rb) * Rc +
                        (2 * Ra * Rb + 2 * (Ra + Rb) * Rc + Rc * Rc) * Ri) *
                           Ro),
                  -(Ra * Ra * Rb * Rb + 2 * Ra * Rb * Rb * Rc -
                    (Ra * Ra - Rb * Rb) * Rc * Rc -
                    ((A + 2) * Ra * Rc + (A + 1) * Rc * Rc + Ra * Ra) * Ri *
                        Ri -
                    ((A + 2) * Ra * Rc * Rc + 2 * Ra * Ra * Rc) * Ri -
                    (Ra * Rb * Rb + Rb * Rb * Rc - Ra * Rc * Rc -
                     (Ra + Rc) * Ri * Ri - (2 * Ra * Rc + Rc * Rc) * Ri) *
                        Ro) /
                      (Ra * Ra * Rb * Rb +
                       (Ra * Ra + 2 * Ra * Rb + Rb * Rb) * Rc * Rc +
                       ((A + 2) * Ra * Rc + (A + 1) * Rc * Rc + Ra * Ra) * Ri *
                           Ri +
                       2 * (Ra * Ra * Rb + Ra * Rb * Rb) * Rc +
                       (2 * Ra * Ra * Rb +
                        ((A + 2) * Ra + (A + 2) * Rb) * Rc * Rc +
                        ((A + 4) * Ra * Rb + 2 * Ra * Ra) * Rc) *
                           Ri -
                       (Ra * Rb * Rb + (Ra + Rb) * Rc * Rc +
                        (Ra + Rc) * Ri * Ri + (2 * Ra * Rb + Rb * Rb) * Rc +
                        (2 * Ra * Rb + 2 * (Ra + Rb) * Rc + Rc * Rc) * Ri) *
                           Ro),
                  (2 * Ra * Ra * Rb * Rb +
                   2 * (Ra * Ra * Rb + Ra * Rb * Rb) * Rc +
                   ((A + 2) * Ra * Rb * Rc + 2 * Ra * Ra * Rb) * Ri -
                   (2 * Ra * Rb * Rb + (2 * Ra * Rb + Rb * Rb) * Rc +
                    (2 * Ra * Rb + Rb * Rc) * Ri) *
                       Ro) /
                      (Ra * Ra * Rb * Rb +
                       (Ra * Ra + 2 * Ra * Rb + Rb * Rb) * Rc * Rc +
                       ((A + 2) * Ra * Rc + (A + 1) * Rc * Rc + Ra * Ra) * Ri *
                           Ri +
                       2 * (Ra * Ra * Rb + Ra * Rb * Rb) * Rc +
                       (2 * Ra * Ra * Rb +
                        ((A + 2) * Ra + (A + 2) * Rb) * Rc * Rc +
                        ((A + 4) * Ra * Rb + 2 * Ra * Ra) * Rc) *
                           Ri -
                       (Ra * Rb * Rb + (Ra + Rb) * Rc * Rc +
                        (Ra + Rc) * Ri * Ri + (2 * Ra * Rb + Rb * Rb) * Rc +
                        (2 * Ra * Rb + 2 * (Ra + Rb) * Rc + Rc * Rc) * Ri) *
                           Ro),
                  Rb * Rc / (Ra * Rb + (Ra + Rb) * Rc + (Ra + Rc) * Ri)},
                 {-(2 * Ra * Rb * Rb * Rc + 2 * (Ra * Rb + Rb * Rb) * Rc * Rc +
                    ((A + 2) * Rc * Rc + 2 * Ra * Rc) * Ri * Ri +
                    (4 * Ra * Rb * Rc + ((A + 4) * Rb + 2 * Ra) * Rc * Rc) *
                        Ri -
                    (Rb * Rb * Rc + Rb * Rc * Rc + Rc * Ri * Ri +
                     (2 * Rb * Rc + Rc * Rc) * Ri) *
                        Ro) /
                      (Ra * Ra * Rb * Rb +
                       (Ra * Ra + 2 * Ra * Rb + Rb * Rb) * Rc * Rc +
                       ((A + 2) * Ra * Rc + (A + 1) * Rc * Rc + Ra * Ra) * Ri *
                           Ri +
                       2 * (Ra * Ra * Rb + Ra * Rb * Rb) * Rc +
                       (2 * Ra * Ra * Rb +
                        ((A + 2) * Ra + (A + 2) * Rb) * Rc * Rc +
                        ((A + 4) * Ra * Rb + 2 * Ra * Ra) * Rc) *
                           Ri -
                       (Ra * Rb * Rb + (Ra + Rb) * Rc * Rc +
                        (Ra + Rc) * Ri * Ri + (2 * Ra * Rb + Rb * Rb) * Rc +
                        (2 * Ra * Rb + 2 * (Ra + Rb) * Rc + Rc * Rc) * Ri) *
                           Ro),
                  (2 * Ra * Ra * Rb * Rc + 2 * (Ra * Ra + Ra * Rb) * Rc * Rc +
                   (A * Ra * Rc + A * Rc * Rc) * Ri * Ri +
                   ((2 * (A + 1) * Ra + A * Rb) * Rc * Rc +
                    (A * Ra * Rb + 2 * Ra * Ra) * Rc) *
                       Ri -
                   (2 * Ra * Rb * Rc + (2 * Ra + Rb) * Rc * Rc +
                    (2 * Ra * Rc + Rc * Rc) * Ri) *
                       Ro) /
                      (Ra * Ra * Rb * Rb +
                       (Ra * Ra + 2 * Ra * Rb + Rb * Rb) * Rc * Rc +
                       ((A + 2) * Ra * Rc + (A + 1) * Rc * Rc + Ra * Ra) * Ri *
                           Ri +
                       2 * (Ra * Ra * Rb + Ra * Rb * Rb) * Rc +
                       (2 * Ra * Ra * Rb +
                        ((A + 2) * Ra + (A + 2) * Rb) * Rc * Rc +
                        ((A + 4) * Ra * Rb + 2 * Ra * Ra) * Rc) *
                           Ri -
                       (Ra * Rb * Rb + (Ra + Rb) * Rc * Rc +
                        (Ra + Rc) * Ri * Ri + (2 * Ra * Rb + Rb * Rb) * Rc +
                        (2 * Ra * Rb + 2 * (Ra + Rb) * Rc + Rc * Rc) * Ri) *
                           Ro),
                  (Ra * Ra * Rb * Rb -
                   (Ra * Ra + 2 * Ra * Rb + Rb * Rb) * Rc * Rc -
                   ((A + 1) * Rc * Rc - Ra * Ra) * Ri * Ri +
                   (2 * Ra * Ra * Rb -
                    ((A + 2) * Ra + (A + 2) * Rb) * Rc * Rc) *
                       Ri -
                   (Ra * Rb * Rb - (Ra + Rb) * Rc * Rc + Ra * Ri * Ri +
                    (2 * Ra * Rb - Rc * Rc) * Ri) *
                       Ro) /
                      (Ra * Ra * Rb * Rb +
                       (Ra * Ra + 2 * Ra * Rb + Rb * Rb) * Rc * Rc +
                       ((A + 2) * Ra * Rc + (A + 1) * Rc * Rc + Ra * Ra) * Ri *
                           Ri +
                       2 * (Ra * Ra * Rb + Ra * Rb * Rb) * Rc +
                       (2 * Ra * Ra * Rb +
                        ((A + 2) * Ra + (A + 2) * Rb) * Rc * Rc +
                        ((A + 4) * Ra * Rb + 2 * Ra * Ra) * Rc) *
                           Ri -
                       (Ra * Rb * Rb + (Ra + Rb) * Rc * Rc +
                        (Ra + Rc) * Ri * Ri + (2 * Ra * Rb + Rb * Rb) * Rc +
                        (2 * Ra * Rb + 2 * (Ra + Rb) * Rc + Rc * Rc) * Ri) *
                           Ro),
                  (Rb * Rc + Rc * Ri) /
                      (Ra * Rb + (Ra + Rb) * Rc + (Ra + Rc) * Ri)},
                 {(A * Rc * Ri - (Rb + Rc + Ri) * Ro) /
                      (Ra * Rb + (Ra + Rb) * Rc + ((A + 1) * Rc + Ra) * Ri -
                       (Rb + Rc + Ri) * Ro),
                  ((A * Ra + A * Rc) * Ri - Rc * Ro) /
                      (Ra * Rb + (Ra + Rb) * Rc + ((A + 1) * Rc + Ra) * Ri -
                       (Rb + Rc + Ri) * Ro),
                  -(A * Ra * Ri + (Rb + Ri) * Ro) /
                      (Ra * Rb + (Ra + Rb) * Rc + ((A + 1) * Rc + Ra) * Ri -
                       (Rb + Rc + Ri) * Ro),
                  0}});

            const auto Rd = -(Ra * Rb + (Ra + Rb) * Rc + (Ra + Rc) * Ri) * Ro /
                            (Ra * Rb + (Ra + Rb) * Rc +
                             ((A + 1) * Rc + Ra) * Ri - (Rb + Rc + Ri) * Ro);
            return Rd;
        }
    };

    WDFT::RtypeAdaptor<T, 3, ImpedanceCalc, decltype(R4), decltype(P2),
                       decltype(S4)>
        R{std::tie(R4, P2, S4)};

    // Port D
    WDFT::ResistorT<T> R5{10.0e3f};
    WDFT::CapacitorT<T> C4{1.0e-6f};
    WDFT::WDFSeriesT<T, decltype(R5), decltype(C4)> S6{R5, C4};
    WDFT::WDFSeriesT<T, decltype(S6), decltype(R)> S7{S6, R};

    WDFT::ResistorT<T> Rout{10.0e3f};
    WDFT::WDFParallelT<T, decltype(Rout), decltype(S7)> P4{Rout, S7};
    WDFT::CapacitorT<T> C5{1.0e-9f};
    WDFT::WDFParallelT<T, decltype(C5), decltype(P4)> P3{C5, P4};

    WDFT::DiodePairT<T, decltype(P3), WDFT::DiodeQuality::Best> DP{
        P3, 2.52e-9f, 25.85e-3f * 1.75f};

    SmoothedValue<double> dist;

    strix::SVTFilter<T> dcBlock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(MXRDistWDF)
};