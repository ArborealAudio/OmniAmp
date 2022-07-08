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
    OptoComp(ProcessorType t, VolumeMeterSource& s) : type(t), grSource(s) {}

    void prepare(const dsp::ProcessSpec& spec)
    {
        lastSR = spec.sampleRate;

        grSource.prepare(spec);

        sc_hp.prepare(spec);
        sc_lp.prepare(spec);
        
        switch (type) {
        case ProcessorType::Guitar:
            *sc_hp.coefficients = dsp::IIR::ArrayCoefficients<T>::makeHighPass(spec.sampleRate, 200.f, 1.02f);
            *sc_lp.coefficients = dsp::IIR::ArrayCoefficients<T>::makeLowPass(spec.sampleRate, 3500.f, 0.8f);
            for (auto& h : hp) {
                h.prepare(spec);
                *h.coefficients = dsp::IIR::ArrayCoefficients<T>::makeFirstOrderHighPass(spec.sampleRate, 2500.f);
            }
            for (auto& l : lp) {
                l.prepare(spec);
                *l.coefficients = dsp::IIR::ArrayCoefficients<T>::makeFirstOrderLowPass(spec.sampleRate, 6500.f);
            }
            break;
        case ProcessorType::Bass:
            *sc_hp.coefficients = dsp::IIR::ArrayCoefficients<T>::makeFirstOrderHighPass(spec.sampleRate, 150.f);
            *sc_lp.coefficients = dsp::IIR::ArrayCoefficients<T>::makeFirstOrderLowPass(spec.sampleRate, 2500.f);
            for (auto& h : hp) {
                h.prepare(spec);
                *h.coefficients = dsp::IIR::ArrayCoefficients<T>::makeFirstOrderHighPass(spec.sampleRate, 1000.f);
            }
            for (auto& l : lp) {
                l.prepare(spec);
                *l.coefficients = dsp::IIR::ArrayCoefficients<T>::makeFirstOrderLowPass(spec.sampleRate, 2500.f);
            }
            break;
        case ProcessorType::Channel:
            *sc_hp.coefficients = dsp::IIR::ArrayCoefficients<T>::makeHighPass(spec.sampleRate, 100.f, 0.707f);
            *sc_lp.coefficients = dsp::IIR::ArrayCoefficients<T>::makeLowPass(spec.sampleRate, 5000.f, 0.8f);
            break;
        }
    }

    void reset()
    {
        xm0 = 0.0, xm1 = 0.0, lastEnv = 0.0, lastGR = 0.0;
        sc_hp.reset();
        sc_lp.reset();
        
        for (auto& h : hp)
            h.reset();
        for (auto& l : lp)
            l.reset();
    }

    /*returns gain reduction multiplier*/
    inline T compress(T x)
    {     
        if (x < 1.175494351e-38)
            x = 1.175494351e-38;

        auto env = jmax(0.0, 8.685889638 * std::log10(x / threshold));

        T att_time = jlimit(0.0050, 0.050, (1.0 / x) * 0.015);

        T att = std::exp(-1.0 / (att_time * lastSR));
        T rel = std::exp(-1.0 / (0.6 * lastGR * lastSR));

        if (env > lastEnv)
        {
            env = env + att * (lastEnv - env);
        }
        else
        {
            env = env + rel * (lastEnv - env);
        }

        lastEnv = env;
        
        auto gr_db = 10.0 * -env; /*using tanh here makes a nice log curve (but also fux w the att/rel curves)*/
        auto gr = std::pow(10.0, gr_db / 20.0);
        lastGR = gr;

        grSource.measureGR(gr);

        return gr;
    }

    void processBlock(dsp::AudioBlock<T>& block, T comp)
    {
        if (comp == 0.0) {
            grSource.measureGR(1.0);
            return;
        }

        auto inL = block.getChannelPointer(0);
        auto inR = block.getChannelPointer(1);

        auto c_comp = jmap(comp, 1.0, 6.0);

        switch (type)
        {
        case ProcessorType::Channel:{
            auto thresh_scale = c_comp / 2.0;
            threshold = std::pow(10.0, (-18.0 * thresh_scale) / 20.0);
            }
            break;
        case ProcessorType::Guitar:
        case ProcessorType::Bass:
            threshold = std::pow(10.0, -36.0 / 20.0);
            break;
        }

        for (int i = 0; i < block.getNumSamples(); ++i)
        {
            auto abs0 = std::abs(xm0);
            auto abs1 = std::abs(xm1);

            T max = jmax(abs0, abs1);
            max = sc_hp.processSample(max);
            max = sc_lp.processSample(max);

            auto gr = compress(max);

            switch (type) {
            case ProcessorType::Guitar:
            case ProcessorType::Bass:
                inL[i] *= c_comp * gr;
                inR[i] *= c_comp * gr;
                break;
            case ProcessorType::Channel:
                if (c_comp <= 4.f) {
                    inL[i] *= gr;
                    inR[i] *= gr;
                }
                else {
                    inL[i] *= (c_comp / 4.0) * gr;
                    inR[i] *= (c_comp / 4.0) * gr;
                }
                break;
            }

            xm0 = inL[i];
            xm1 = inR[i];

            switch (type) {
            case ProcessorType::Channel:
                inL[i] *= c_comp;
                inR[i] *= c_comp;
                break;
            case ProcessorType::Guitar:
            case ProcessorType::Bass:
                auto bpL = hp[0].processSample(inL[i]);
                bpL = lp[0].processSample(bpL);
                auto bpR = hp[1].processSample(inR[i]);
                bpR = lp[1].processSample(bpR);

                inL[i] += bpL * comp;
                inR[i] += bpR * comp;
                inL[i] *= jlimit(1.0, 3.0, c_comp);
                inR[i] *= jlimit(1.0, 3.0, c_comp);
                break;
            }
        }
    }

    VolumeMeterSource& getGRSource() { return grSource; }

private:
    T lastSR = 44100.0;

    T threshold = std::pow(10.0, -18.0 / 20.0);
    T lastEnv = 0.0, lastGR = 0.0;
    T xm0 = 0.0, xm1 = 0.0;

    dsp::IIR::Filter<T> sc_hp, sc_lp, lp[2], hp[2];

    ProcessorType type;

    VolumeMeterSource& grSource;
};