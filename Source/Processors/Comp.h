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

        sc_hp.prepare(spec);
        sc_lp.prepare(spec);
        
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

        sc_hp.coefficients = sc_hp_coeffs;
        sc_lp.coefficients = sc_lp_coeffs;
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

    void processBlock(dsp::AudioBlock<double>& block, T comp)
    {
        if (comp == 0.0) {
            grSource.measureGR(1.0);
            return;
        }
        if (block.getNumChannels() > 1)
            processStereo(block, comp);
        else
            processMono(block, comp);
    }

    strix::VolumeMeterSource& getGRSource() { return grSource; }

private:

    inline void processStereo(dsp::AudioBlock<T>& block, T comp)
    {
        auto inL = block.getChannelPointer(0);
        auto inR = block.getChannelPointer(1);

        T c_comp = jmap(comp, (T)1.0, (T)6.0);

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
                inL[i] *= jmin(jmax(3.0, c_comp), 1.0);
                inR[i] *= jmin(jmax(3.0, c_comp), 1.0);
                break;
            }
        }
    }

    inline void processMono(dsp::AudioBlock<T>& block, T comp)
    {
        auto inL = block.getChannelPointer(0);

        T c_comp = jmap(comp, (T)1.0, (T)6.0);

        for (int i = 0; i < block.getNumSamples(); ++i)
        {
            auto abs0 = std::abs(xm0);

            abs0 = sc_hp.processSample(abs0);
            abs0 = sc_lp.processSample(abs0);

            auto gr = compress(abs0);

            switch (type) {
            case ProcessorType::Guitar:
            case ProcessorType::Bass:
                inL[i] *= c_comp * gr;
                break;
            case ProcessorType::Channel:
                if (c_comp <= 4.f) {
                    inL[i] *= gr;
                }
                else {
                    inL[i] *= (c_comp / 4.0) * gr;
                }
                break;
            }

            xm0 = inL[i];

            switch (type) {
            case ProcessorType::Channel:
                inL[i] *= c_comp;
                break;
            case ProcessorType::Guitar:
            case ProcessorType::Bass:
                auto bpL = hp[0].processSample(inL[i]);
                bpL = lp[0].processSample(bpL);

                inL[i] += bpL * comp;
                inL[i] *= jmin(jmax(3.0, c_comp), 1.0);
                break;
            }
        }
    }

    // if we do default SIMD stuff here, it should process L&R unlinked. How to link L&R?
    // inline void processSIMD(chowdsp::AudioBlock<T>& block, T comp)
    // {
    //     auto in = block.getChannelPointer(0);

    //     T c_comp = jmap(comp, (T)1.0, (T)6.0);

    //     switch (type)
    //     {
    //     case ProcessorType::Channel:{
    //         auto thresh_scale = c_comp / 2.0;
    //         threshold = xsimd::pow((T)10.0, (-18.0 * thresh_scale) / 20.0);
    //         }
    //         break;
    //     case ProcessorType::Guitar:
    //     case ProcessorType::Bass:
    //         threshold = xsimd::pow(10.0, -36.0 / 20.0);
    //         break;
    //     }

    //     for (int i = 0; i < block.getNumSamples(); ++i)
    //     {
    //         auto abs = xsimd::abs(xm0);

    //         abs = sc_hp.processSample(abs);
    //         abs = sc_lp.processSample(abs);

    //         auto gr = compressSIMD(abs);

    //         switch (type) {
    //         case ProcessorType::Guitar:
    //         case ProcessorType::Bass:
    //             in[i] *= c_comp * gr;
    //             break;
    //         case ProcessorType::Channel:
    //             xsimd::batch_bool<double> range{c_comp <= 4.0};
    //             if (xsimd::any(range))
    //             {
    //                 in[i] *= gr;
    //             }
    //             else {
    //                 in[i] *= (c_comp / 4.0) * gr;
    //             }
    //             break;
    //         }

    //         xm0 = in[i];

    //         switch (type) {
    //         case ProcessorType::Channel:
    //             in[i] *= c_comp;
    //             break;
    //         case ProcessorType::Guitar:
    //         case ProcessorType::Bass:
    //             auto bp = hp[0].processSample(in[i]);
    //             bp = lp[0].processSample(bp);

    //             in[i] += bp * comp;
    //             in[i] *= xsimd::min(xsimd::max((T)3.0, c_comp), (T)1.0);
    //             break;
    //         }
    //     }
    // }

    /*returns gain reduction multiplier*/
    inline T compress(T x)
    {     
        if (x < 1.175494351e-38)
            x = 1.175494351e-38;

        auto env = jmax(0.0, 8.685889638 * std::log10(x / threshold.load()));

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

    /*returns gain reduction multiplier*/
    inline T compressSIMD(T x)
    {
        xsimd::batch_bool<double> under{x < 1.175494351e-38};
        x = xsimd::select(under, (T)1.175494351e-38, x);

        auto env = 8.685889638 * xsimd::log10(x / threshold);
        env = xsimd::max((T)0.0, env);

        // T att_time = jlimit((T)0.005, (T)0.05, (1.0 / x) * 0.015);
        T att_time = xsimd::min(xsimd::max((T)0.05, (1.0 / x) * 0.015), (T)0.005);

        T att = xsimd::exp(-1.0 / (att_time * lastSR));
        T rel = xsimd::exp(-1.0 / (0.6 * lastGR * lastSR));

        xsimd::batch_bool<double> inc{env > lastEnv};

        env = xsimd::select(inc, env + att * (lastEnv - env), env + rel * (lastEnv - env));

        // if (env > lastEnv)
        // {
        //     env = env + att * (lastEnv - env);
        // }
        // else
        // {
        //     env = env + rel * (lastEnv - env);
        // }

        lastEnv = env;
        
        auto gr_db = 10.0 * -env; /*using tanh here makes a nice log curve (but also fux w the att/rel curves)*/
        auto gr = xsimd::pow((T)10.0, gr_db / 20.0);
        lastGR = gr;

        float grf;
        gr.store_aligned(&grf);
        grSource.measureGR(grf);

        return gr;
    }

    T lastSR = 44100.0;

    std::atomic<float> threshold = std::pow(10.0, -18.0 / 20.0);
    
    T lastEnv = 0.0, lastGR = 0.0;
    T xm0 = 0.0, xm1 = 0.0;

    dsp::IIR::Filter<T> sc_hp, sc_lp, lp[2], hp[2];
    dsp::IIR::Coefficients<double>::Ptr sc_hp_coeffs, sc_lp_coeffs, lp_coeffs, hp_coeffs;

    ProcessorType type;

    strix::VolumeMeterSource& grSource;
};