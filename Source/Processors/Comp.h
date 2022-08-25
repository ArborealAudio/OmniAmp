/*
  ==============================================================================

    Comp.h
    Created: 9 Mar 2022 11:12:10am
    Author:  alexb

  ==============================================================================
*/

#pragma once

template <typename T>
struct OptoComp
{
    OptoComp(ProcessorType t, strix::VolumeMeterSource& s) : type(t), grSource(s) {}

    void prepare(const dsp::ProcessSpec& spec)
    {
        lastSR = spec.sampleRate;

        grSource.prepare(spec);

        switch (type) {
        case ProcessorType::Guitar:
            sc_hp_coeffs = dsp::IIR::Coefficients<double>::makeHighPass(spec.sampleRate, 200.0, 1.02);
            sc_lp_coeffs = dsp::IIR::Coefficients<double>::makeLowPass(spec.sampleRate, 3500.0, 0.8);

            hp_coeffs = dsp::IIR::Coefficients<double>::makeFirstOrderHighPass(spec.sampleRate, 2500.0);
            lp_coeffs = dsp::IIR::Coefficients<double>::makeFirstOrderLowPass(spec.sampleRate, 6500.0);

            for (auto &h : hp)
            {
                h.prepare(spec);
                h.coefficients = hp_coeffs;
            }
            for (auto& l : lp) {
                l.prepare(spec);
                l.coefficients = lp_coeffs;
            }
            break;
        case ProcessorType::Bass:
            sc_hp_coeffs = dsp::IIR::Coefficients<double>::makeFirstOrderHighPass(spec.sampleRate, 150.0);
            sc_lp_coeffs = dsp::IIR::Coefficients<double>::makeFirstOrderLowPass(spec.sampleRate, 2500.0);

            hp_coeffs = dsp::IIR::Coefficients<double>::makeFirstOrderHighPass(spec.sampleRate, 1000.0);
            lp_coeffs = dsp::IIR::Coefficients<double>::makeFirstOrderLowPass(spec.sampleRate, 2500.0);

            for (auto& h : hp) {
                h.prepare(spec);
                h.coefficients = hp_coeffs;
            }
            for (auto& l : lp) {
                l.prepare(spec);
                l.coefficients = lp_coeffs;
            }
            break;
        case ProcessorType::Channel:
            sc_hp_coeffs = dsp::IIR::Coefficients<double>::makeHighPass(spec.sampleRate, 100.0, 0.707);
            sc_lp_coeffs = dsp::IIR::Coefficients<double>::makeLowPass(spec.sampleRate, 5000.0, 0.8);
            break;
        }

        for (auto &h : sc_hp)
        {
            h.prepare(spec);
            h.coefficients = sc_hp_coeffs;
        }

        for (auto &l : sc_lp)
        {
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

        for (auto& h : hp)
            h.reset();
        for (auto& l : lp)
            l.reset();
    }

    void setThreshold(double newComp)
    {
        double c_comp = jmap(newComp, 1.0, 6.0);

        switch (type)
        {
        case ProcessorType::Guitar:
        case ProcessorType::Bass:
            threshold.store(std::pow(10.0, -36.0 / 20.0));
            break;
        case ProcessorType::Channel:{
            auto thresh_scale = c_comp / 2.0;
            threshold.store(std::pow(10.0, (-18.0 * thresh_scale) / 20.0));
            break;
            }
        }
    }

    void processBlock(dsp::AudioBlock<double> &block, T comp, bool linked)
    {
        if (comp == 0.0) {
            grSource.measureGR(1.0);
            return;
        }
        if (block.getNumChannels() > 1 && linked)
            processStereo(block.getChannelPointer(0), block.getChannelPointer(1), comp, block.getNumSamples());
        else if (block.getNumChannels() > 1 && !linked)
        {
            processUnlinked(block.getChannelPointer(0), 0, comp, block.getNumSamples());
            processUnlinked(block.getChannelPointer(1), 1, comp, block.getNumSamples());
        }
        else
            processUnlinked(block.getChannelPointer(0), 0, comp, block.getNumSamples());
    }

    strix::VolumeMeterSource& getGRSource() { return grSource; }

private:
    /* linked stereo */
    inline void processStereo(T *inL, T *inR, T comp, int numSamples)
    {
        T c_comp = jmap(comp, (T)1.0, (T)6.0);

        for (int i = 0; i < numSamples; ++i)
        {
            auto abs0 = std::abs(xm[0]);
            auto abs1 = std::abs(xm[1]);

            T max = jmax(abs0, abs1);
            max = sc_hp[0].processSample(max);
            max = sc_lp[0].processSample(max);

            auto gr = computeGR(max, 0);

            postComp(inL[i], 0, gr, comp, c_comp);
            postComp(inR[i], 1, gr, comp, c_comp);
        }
    }

    /* mono or unlinked stereo */
    inline void processUnlinked(T *in, int ch, T comp, int numSamples)
    {
        T c_comp = jmap(comp, (T)1.0, (T)6.0);

        for (int i = 0; i < numSamples; ++i)
        {
            auto abs = std::abs(xm[ch]);

            abs = sc_hp[ch].processSample(abs);
            abs = sc_lp[ch].processSample(abs);

            auto gr = computeGR(abs, ch);

            postComp(in[i], ch, gr, comp, c_comp);
        }
    }

    /*returns gain reduction multiplier*/
    inline T computeGR(T x, int ch)
    {     
        if (x < 1.175494351e-38)
            x = 1.175494351e-38;

        auto env = jmax(0.0, 8.685889638 * std::log10(x / threshold.load()));

        T att_time = jlimit(0.0050, 0.050, (1.0 / x) * 0.015);

        T att = std::exp(-1.0 / (att_time * lastSR));
        T rel = std::exp(-1.0 / (0.6 * lastGR[ch] * lastSR));

        if (env > lastEnv[ch])
        {
            env = env + att * (lastEnv[ch] - env);
        }
        else
        {
            env = env + rel * (lastEnv[ch] - env);
        }

        lastEnv[ch] = env;

        auto gr_db = 10.0 * -env; /*using tanh here makes a nice log curve (but also fux w the att/rel curves)*/
        auto gr = std::pow(10.0, gr_db / 20.0);
        lastGR[ch] = gr;

        grSource.measureGR(lastGR[0]); // this should average btw L&R if unlinked stereo

        return gr;
    }

    inline void postComp(T &x, int ch, T gr, T comp, T c_comp)
    {
        switch (type)
        {
        case ProcessorType::Guitar:
        case ProcessorType::Bass:
            x *= c_comp * gr;
            break;
        case ProcessorType::Channel:
            if (c_comp <= 4.f)
                x *= gr;
            else
                x *= (c_comp / 4.0) * gr;
            break;
        }

        xm[ch] = x;

        switch (type)
        {
        case ProcessorType::Guitar:
        case ProcessorType::Bass:
        {
            auto bp = hp[ch].processSample(x);
            bp = lp[ch].processSample(bp);

            x += bp * comp;
            break;
        }
        case ProcessorType::Channel:
            if (c_comp > 2.f)
                x *= c_comp / 2.f;
            break;
        }
    }

    T lastSR = 44100.0;

    std::atomic<float> threshold = std::pow(10.0, -18.0 / 20.0);

    T lastEnv[2]{0.0}, lastGR[2]{0.0};
    T xm[2]{0.0};

    dsp::IIR::Filter<T> sc_hp[2], sc_lp[2], lp[2], hp[2];
    dsp::IIR::Coefficients<double>::Ptr sc_hp_coeffs, sc_lp_coeffs, lp_coeffs, hp_coeffs;

    ProcessorType type;

    strix::VolumeMeterSource& grSource;
};