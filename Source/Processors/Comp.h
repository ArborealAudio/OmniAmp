/*
    Comp.h
*/

#pragma once

template <typename T> struct OptoComp
{
    OptoComp(ProcessorType t, strix::VolumeMeterSource &s,
             std::atomic<float> *pos)
        : position(pos), type(t), grSource(s)
    {
    }

    void prepare(const dsp::ProcessSpec &spec)
    {
        lastSR = spec.sampleRate;
        nChannels = spec.numChannels;

        grSource.prepare(spec, 0.01f);

        grData.setSize(spec.numChannels, spec.maximumBlockSize, false, false,
                       true);

        switch (type) {
        case ProcessorType::Guitar:
            sc_hp_coeffs = dsp::IIR::Coefficients<double>::makeHighPass(
                spec.sampleRate, 200.0, 1.02);
            sc_lp_coeffs = dsp::IIR::Coefficients<double>::makeLowPass(
                spec.sampleRate, 3500.0, 0.8);

            hp_coeffs = dsp::IIR::Coefficients<double>::makeFirstOrderHighPass(
                spec.sampleRate, 1000.0);
            lp_coeffs = dsp::IIR::Coefficients<double>::makeLowPass(
                spec.sampleRate, 5000.0);

            for (auto &h : hp) {
                h.prepare(spec);
                h.coefficients = hp_coeffs;
            }
            for (auto &l : lp) {
                l.prepare(spec);
                l.coefficients = lp_coeffs;
            }
            break;
        case ProcessorType::Bass:
            sc_hp_coeffs =
                dsp::IIR::Coefficients<double>::makeFirstOrderHighPass(
                    spec.sampleRate, 150.0);
            sc_lp_coeffs =
                dsp::IIR::Coefficients<double>::makeFirstOrderLowPass(
                    spec.sampleRate, 2500.0);

            hp_coeffs = dsp::IIR::Coefficients<double>::makeFirstOrderHighPass(
                spec.sampleRate, 1000.0);
            lp_coeffs = dsp::IIR::Coefficients<double>::makeLowPass(
                spec.sampleRate, 3500.0);

            for (auto &h : hp) {
                h.prepare(spec);
                h.coefficients = hp_coeffs;
            }
            for (auto &l : lp) {
                l.prepare(spec);
                l.coefficients = lp_coeffs;
            }
            break;
        case ProcessorType::Channel:
            sc_hp_coeffs = dsp::IIR::Coefficients<double>::makeHighPass(
                spec.sampleRate, 100.0, 0.707);
            sc_lp_coeffs = dsp::IIR::Coefficients<double>::makeLowPass(
                spec.sampleRate, 5000.0, 0.8);
            break;
        }

        for (auto &h : sc_hp) {
            h.prepare(spec);
            h.coefficients = sc_hp_coeffs;
        }

        for (auto &l : sc_lp) {
            l.prepare(spec);
            l.coefficients = sc_lp_coeffs;
        }
    }

    void reset()
    {
        xm[0] = xm[1] = lastEnv[0] = lastEnv[1] = lastGR[0] = lastGR[1] = 0.0;

        for (auto &h : sc_hp)
            h.reset();
        for (auto &l : sc_lp)
            l.reset();

        for (auto &h : hp)
            h.reset();
        for (auto &l : lp)
            l.reset();
    }

    // set threshold based on comp param
    void setComp(double newComp)
    {
        switch (type) {
        case ProcessorType::Guitar:
        case ProcessorType::Bass: {
            double c_comp = jmap(newComp, 1.0, 3.0);
            threshold.store(std::pow(
                10.0, (-18.0 * c_comp) *
                          0.05)); /* start at -18dB and scale down 3x */
            break;
        }
        case ProcessorType::Channel: {
            double c_comp = jmap(newComp, 1.0, 3.0);
            threshold.store(std::pow(
                10.0, (-12.0 * c_comp) *
                          0.05)); /* start at -12dB and scale down 3x */
            break;
        }
        }
    }

    void processBlock(dsp::AudioBlock<double> &block, T comp, bool linked)
    {
        if (comp == 0.0) {
            grSource.measureGR(1.0);
            reset();
            return;
        }
        if (block.getNumChannels() > 1 && linked)
            processStereo(block.getChannelPointer(0),
                          block.getChannelPointer(1), comp,
                          block.getNumSamples());
        else if (block.getNumChannels() > 1 && !linked) {
            processUnlinked(block.getChannelPointer(0), 0, comp,
                            block.getNumSamples());
            processUnlinked(block.getChannelPointer(1), 1, comp,
                            block.getNumSamples());
        } else
            processUnlinked(block.getChannelPointer(0), 0, comp,
                            block.getNumSamples());

        // copy data to GR meter
        grSource.copyBuffer(grData.getArrayOfWritePointers(),
                            block.getNumChannels(), block.getNumSamples());
    }

    strix::VolumeMeterSource &getGRSource() { return grSource; }

  private:
    /* linked stereo */
    inline void processStereo(T *inL, T *inR, T comp, int numSamples)
    {
        T c_comp = jmap(comp, (T)1.0, (T)6.0);
        T last_c = jmap(lastComp, (T)1.0, (T)6.0);

        auto inc = (comp - lastComp) / numSamples;
        auto c_inc = (c_comp - last_c) / numSamples;

        auto grBuf = grData.getArrayOfWritePointers();

        for (int i = 0; i < numSamples; ++i) {
            auto abs0 = std::abs(xm[0]);
            auto abs1 = std::abs(xm[1]);

            T max = jmax(abs0, abs1);
            max = sc_hp[0].processSample(max);
            max = sc_lp[0].processSample(max);

            max = strix::fast_tanh(max);

            auto gr = computeGR(0, max);
            for (size_t ch = 0; ch < grData.getNumChannels(); ++ch)
                grBuf[ch][i] = lastGR[0];

            postComp(inL[i], 0, gr, lastComp, last_c);
            postComp(inR[i], 1, gr, lastComp, last_c);

            lastComp += inc;
            last_c += c_inc;
        }

        // just in case
        lastComp = comp;
    }

    /* mono or unlinked stereo */
    inline void processUnlinked(T *in, int ch, T comp, int numSamples)
    {
        T c_comp = jmap(comp, (T)1.0, (T)6.0);
        T last_c = jmap(lastComp, (T)1.0, (T)6.0);

        auto inc = (comp - lastComp) / numSamples;
        auto c_inc = (c_comp - last_c) / numSamples;

        auto grBuf = grData.getWritePointer(ch);

        for (int i = 0; i < numSamples; ++i) {
            auto x = std::abs(xm[ch]);

            x = sc_hp[ch].processSample(x);
            x = sc_lp[ch].processSample(x);

            x = strix::fast_tanh(x);

            auto gr = computeGR(ch, x);

            grBuf[i] = lastGR[ch];

            postComp(in[i], ch, gr, lastComp, last_c);

            lastComp += inc;
            last_c += c_inc;
        }

        // just in case
        lastComp = comp;
    }

    /*returns gain reduction multiplier*/
    inline T computeGR(int ch, T x)
    {
        if (x < 1.175494351e-38)
            x = 1.175494351e-38;

        auto env = jmax(0.0, 8.685889638 * std::log10(x / threshold.load()));

        T att_time = jlimit(0.005, 0.05, (1.0 / x) * 0.015);
        T rel_time = jlimit(0.05, 1.2, 0.5 * (0.5 * lastGR[ch]));
        T att = std::exp(-1.0 / (att_time * lastSR));
        T rel = std::exp(-1.0 / (rel_time * lastSR));

        if (env > lastEnv[ch]) {
            env = env + att * (lastEnv[ch] - env);
        } else {
            env = env + rel * (lastEnv[ch] - env);
        }

        lastEnv[ch] = env;

        auto gr_db = 10.0 * -env;
        auto gr = std::pow(10.0, gr_db * 0.05);
        lastGR[ch] = gr;

        return gr;
    }

    /* comp: 0-1, c_comp: 1-6 */
    inline void postComp(T &x, int ch, T gr, T comp, T c_comp)
    {
        switch (type) {
        case ProcessorType::Guitar:
        case ProcessorType::Bass:
            if (c_comp <= 2.f)
                x *= gr;
            else
                x *= (c_comp / 2.0) * gr;
            break;
        case ProcessorType::Channel:
            if (c_comp <= 4.f)
                x *= gr;
            else
                x *= (c_comp / 4.0) * gr;
            break;
        } /*apply recursive gains to the sidechain if comp is high enough*/

        xm[ch] = x;

        switch (type) {
        case ProcessorType::Guitar:
        case ProcessorType::Bass: {
            auto bp = hp[ch].processSample(x);
            bp = lp[ch].processSample(bp);

            x += bp *
                 (comp * 1.5); /* use comp as gain for bp signal, this also kind
                                  of doubles as a filtered output gain */
            break;
        }
        case ProcessorType::Channel:
            if (c_comp > 2.f)
                x *= c_comp / 2.f; /* if > 2, apply output gain, max 3 */
            break;
        }
    }

    T lastSR = 44100.0;

    std::atomic<float> threshold = std::pow(10.0, -18.0 * 0.05),
                       *position = nullptr;

    double lastComp = 0.0;

    T lastEnv[2]{0.0}, lastGR[2]{0.0}, xm[2]{0.0};

    AudioBuffer<float> grData;
    size_t nChannels = 0;

    dsp::IIR::Filter<T> sc_hp[2], sc_lp[2], lp[2], hp[2];
    dsp::IIR::Coefficients<double>::Ptr sc_hp_coeffs, sc_lp_coeffs, lp_coeffs,
        hp_coeffs;

    ProcessorType type;

    strix::VolumeMeterSource &grSource;
};
