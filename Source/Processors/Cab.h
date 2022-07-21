// Cab.h

#pragma once

enum CabType
{
    small,
    med,
    large
};

template <typename Type>
class FDNCab
{
    template <typename T>
    struct FDN
    {
        FDN(AudioProcessorValueTreeState& a, CabType t) : apvts(a)
        {
            switch (t)
            {
            case small:
                f_order = 1;
                break;
            case med:
                f_order = 3;
                break;
            case large:
                f_order = 4;
                break;
            default:
                f_order = 1;
                break;
            }

            delay.resize(f_order);
            dtime.resize(f_order);

            hp.resize(f_order);
            lp.resize(f_order);

            dtime[0] = 103.f;
            if (t > 0)
            {
                dtime[0] = 377.f;
                dtime[1] = 365.f;
                dtime[2] = 375.f;
            }
            // MUST change these to ms or µs values so as to be samplerate-agnostic
        }

        void prepare(const dsp::ProcessSpec spec)
        {
            auto monoSpec = spec;
        #if USE_SIMD
            monoSpec.numChannels = 1;
        #endif
            for (auto i = 0; i < delay.size(); ++i)
            {
                delay[i].prepare(monoSpec);
                delay[i].setMaximumDelayInSamples(dtime[i] + 1);
            }

            int i = 1;
            for (auto &f : lp)
            {
                f.prepare(spec);
                f.setType(strix::FilterType::lowpass);
                f.setCutoffFreq(5000.0 - (i * 500.0));
                f.setResonance(0.7);
                ++i;
            }

            i = 1;
            for (auto &f : hp)
            {
                f.prepare(spec);
                f.setType(strix::FilterType::highpass);
                f.setCutoffFreq(100.0 - (20.0 * i));
                f.setResonance(1.0);
                ++i;
            }
        }

        void reset()
        {
            for (auto& d : delay)
                d.reset();
            for (auto& f : lp)
                f.reset();
            for (auto& f : hp)
                f.reset();
        }

        template <class Block>
        void processBlock(Block& block)
        {
            for (auto ch = 0; ch < block.getNumChannels(); ++ch)
            {
                auto in = block.getChannelPointer(ch);

                for (auto i = 0; i < block.getNumSamples(); ++i)
                {
                    T out = 0.0;

                    for (auto n = 0; n < f_order; ++n)
                    {
                        auto d = delay[n].popSample(ch, dtime[n]);

                        out += d + in[i] * -fdbk;

                        auto f = out * fdbk + in[i];

                        f = hp[n].processSample(ch, f);

                        f = lp[n].processSample(ch, f);

                        delay[n].pushSample(ch, f);
                    }

                    in[i] = out;
                }
            }
        }

    private:

        // inline T processNestedAllpass(T in, int ch, int index)
        // {
        //     auto d = delay[index].popSample(ch, dtime[index]);

        //     auto out = d + in * -fdbk;

        //     auto f = out * fdbk + in;

        //     delay[index].pushSample(ch, f);

        //     return out;
        // }

        size_t f_order;

        std::vector<strix::Delay<T>> delay;
        std::vector<double> dtime;

        T fdbk = 0.1f;

        std::vector<strix::SVTFilter<T>> lp, hp;

        AudioProcessorValueTreeState &apvts;
    };

    class ReleasePool : Timer
    {
        std::vector<std::shared_ptr<void>> pool;
        std::mutex m;

        void timerCallback() override
        {
            std::lock_guard<std::mutex> lock(m);
            pool.erase(
                std::remove_if(
                    pool.begin(), pool.end(),
                    [](auto &object)
                    { return object.use_count() <= 1; }),
                pool.end());
        }
    public:
        ReleasePool() { startTimer(1000); }

        // template<typename T>
        template <typename obj>
        void add(const std::shared_ptr<obj>& object)
        {
            if (object == nullptr)
                return;

            std::lock_guard<std::mutex> lock(m);
            pool.emplace_back(object);
        }
    };

    strix::SVTFilter<Type> hp, lp1, lp2, bp;
    // dsp::IIR::Coefficients<double>::Ptr hp_c, lp1_c, lp2_c, bp_c;

    double sr = 44100.0;

    std::shared_ptr<FDN<Type>> fdn;

    dsp::ProcessSpec memSpec;

    CabType type;

    ReleasePool releasePool;

    AudioProcessorValueTreeState &apvts;

public:
    FDNCab(AudioProcessorValueTreeState& a, CabType t) : apvts(a)
    {
        fdn = std::make_shared<FDN<Type>>(a, t);
    }

    void setCabType(CabType newType)
    {
        type = newType;
        setParams();

        std::shared_ptr<FDN<Type>> new_fdn = std::make_shared<FDN<Type>>(apvts, newType);
        new_fdn->prepare(memSpec);
        releasePool.add(new_fdn);
        std::atomic_store(&fdn, new_fdn);
    }

    void prepare(const dsp::ProcessSpec &spec)
    {
        memSpec = spec;

        sr = spec.sampleRate;

        hp.prepare(spec);
        hp.setType(strix::FilterType::highpass);

        lp1.prepare(spec);
        lp2.prepare(spec);
        lp1.setType(strix::FilterType::lowpass);
        lp2.setType(strix::FilterType::firstOrderLowpass);

        bp.prepare(spec);
        bp.setType(strix::FilterType::notch);

        fdn->prepare(spec);
    }

    void reset()
    {
        fdn->reset();
        hp.reset();
        lp1.reset();
        lp2.reset();
    }

    void setParams()
    {
        switch (type)
        {
        case small:
            // hp.coefficients = dsp::IIR::Coefficients<double>::makeHighPass(sr, 90.0, 5.0);
            hp.setCutoffFreq(90.0);
            hp.setResonance(5.0);
            // lp1.coefficients = dsp::IIR::Coefficients<double>::makeLowPass(sr, 3500.0);
            lp1.setCutoffFreq(3500.0);
            lp1.setResonance(0.7);
            // lp2.coefficients = dsp::IIR::Coefficients<double>::makeFirstOrderLowPass(sr, 5000.0);
            lp2.setCutoffFreq(5000.0);
            lp2.setResonance(1.5);
            break;
        case med:
            // hp.coefficients = dsp::IIR::Coefficients<double>::makeHighPass(sr, 75.0, 0.8);
            hp.setCutoffFreq(75.0);
            hp.setResonance(1.4);
            // lp1.coefficients = dsp::IIR::Coefficients<double>::makeLowPass(sr, 2821.0);
            lp1.setCutoffFreq(2821.0);
            lp1.setResonance(1.4);
            // lp2.coefficients = dsp::IIR::Coefficients<double>::makeFirstOrderLowPass(sr, 5000.0);
            lp2.setCutoffFreq(4581.0);
            lp2.setResonance(1.8);
            // bp.coefficients = dsp::IIR::Coefficients<double>::makeNotch(sr, 1700.0, 2.0);
            bp.setCutoffFreq(1700.0);
            bp.setResonance(2.0);
            break;
        case large:
            break;
        default:
            break;
        }
    }

    // void setParams()
    // {
    //     hp.setCutoffFrequency(*apvts.getRawParameterValue("hpFreq"));
    //     hp.setResonance(*apvts.getRawParameterValue("hpQ"));
    //     lp1.setCutoffFrequency(*apvts.getRawParameterValue("lp1Freq"));
    //     lp1.setResonance(*apvts.getRawParameterValue("lp1Q"));
    //     lp2.setCutoffFrequency(*apvts.getRawParameterValue("lp2Freq"));
    //     lp2.setResonance(*apvts.getRawParameterValue("lp2Q"));
    //     bp.setCutoffFrequency(*apvts.getRawParameterValue("bpFreq"));
    //     bp.setResonance(*apvts.getRawParameterValue("bpQ"));
    // }

    template <class Block>
    void processBlock(Block& block)
    {
        std::shared_ptr<FDN<Type>> proc_fdn = std::atomic_load(&fdn);

        lp1.processBlock(block);
        proc_fdn->processBlock(block);
        hp.processBlock(block);
        if (type > 0)
            bp.processBlock(block);
        lp2.processBlock(block);
    }
};