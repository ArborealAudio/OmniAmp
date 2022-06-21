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
    OptoComp(ProcessorType t) : type(t) {}

    void prepare(const dsp::ProcessSpec& spec)
    {
        lastSR = spec.sampleRate;

        grSource.prepare(spec);

        sc_hp.prepare(spec);
        sc_lp.prepare(spec);
        
        switch (type) {
        case ProcessorType::Guitar:
            *sc_hp.coefficients = dsp::IIR::ArrayCoefficients<float>::makeHighPass(spec.sampleRate, 200.f, 1.02f);
            *sc_lp.coefficients = dsp::IIR::ArrayCoefficients<float>::makeLowPass(spec.sampleRate, 3500.f, 0.8f);
            for (auto& h : hp) {
                h.prepare(spec);
                *h.coefficients = dsp::IIR::ArrayCoefficients<float>::makeFirstOrderHighPass(spec.sampleRate, 2500.f);
            }
            for (auto& l : lp) {
                l.prepare(spec);
                *l.coefficients = dsp::IIR::ArrayCoefficients<float>::makeFirstOrderLowPass(spec.sampleRate, 6500.f);
            }
            break;
        case ProcessorType::Bass:
            *sc_hp.coefficients = dsp::IIR::ArrayCoefficients<float>::makeFirstOrderHighPass(spec.sampleRate, 150.f);
            *sc_lp.coefficients = dsp::IIR::ArrayCoefficients<float>::makeFirstOrderLowPass(spec.sampleRate, 2500.f);
            for (auto& h : hp) {
                h.prepare(spec);
                *h.coefficients = dsp::IIR::ArrayCoefficients<float>::makeFirstOrderHighPass(spec.sampleRate, 1000.f);
            }
            for (auto& l : lp) {
                l.prepare(spec);
                *l.coefficients = dsp::IIR::ArrayCoefficients<float>::makeFirstOrderLowPass(spec.sampleRate, 2500.f);
            }
            break;
        case ProcessorType::Channel:
            *sc_hp.coefficients = dsp::IIR::ArrayCoefficients<float>::makeHighPass(spec.sampleRate, 100.f, 0.707f);
            *sc_lp.coefficients = dsp::IIR::ArrayCoefficients<float>::makeLowPass(spec.sampleRate, 5000.f, 0.8f);
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
        
        auto gr_db = 10.f * -env; /*using tanh here makes a nice log curve (but also fux w the att/rel curves)*/
        auto gr = std::pow(10.f, gr_db / 20.f);
        lastGR = gr;

        grSource.measureGR(gr);

        return gr;
    }

    void processBlock(dsp::AudioBlock<float>& block, float comp)
    {
        auto inL = block.getChannelPointer(0);
        auto inR = block.getChannelPointer(1);

        auto c_comp = jmap(comp, 1.f, 6.f);

        switch (type)
        {
        case ProcessorType::Channel:{
            auto thresh_scale = c_comp / 2.f;
            threshold = std::pow(10.f, (-18.f * thresh_scale) / 20.f);
            }
            break;
        case ProcessorType::Guitar:
        case ProcessorType::Bass:
            threshold = std::pow(10.f, -36.f / 20.f);
            break;
        }

        for (int i = 0; i < block.getNumSamples(); ++i)
        {
            auto abs0 = std::abs(xm0);
            auto abs1 = std::abs(xm1);

            float max = jmax(abs0, abs1);
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
                    inL[i] *= (c_comp / 4.f) * gr;
                    inR[i] *= (c_comp / 4.f) * gr;
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
                inL[i] *= jlimit(1.f, 3.f, c_comp);
                inR[i] *= jlimit(1.f, 3.f, c_comp);
                break;
            }
        }
    }

    VolumeMeterSource& getGRSource() { return grSource; }

private:
    float lastSR = 44100.f;

    float threshold = std::pow(10.f, -18.f / 20.f);
    float lastEnv = 0.f, lastGR = 0.f;
    float xm0 = 0.f, xm1 = 0.f;

    dsp::IIR::Filter<float> sc_hp, sc_lp, lp[2], hp[2];

    ProcessorType type;

    VolumeMeterSource grSource;
};