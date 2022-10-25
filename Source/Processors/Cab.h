// Cab.h

#pragma once

enum CabType
{
    off,
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
                f_order = 4;
                break;
            case med:
                f_order = 4;
                break;
            case large:
                f_order = 4;
                break;
            default:
                f_order = 1;
                break;
            }

            delay.clear();
            dtime.clear();
            delay.resize(f_order);
            dtime.resize(f_order);

            dtime[0] = 40.f;
            dtime[1] = 57.f;
            dtime[2] = 73.f;
            dtime[3] = 79.f;
            if (t == 1)
            {
                dtime[0] = 57.f;
                dtime[1] = 24.f;
                dtime[2] = 40.f;
                dtime[3] = 79.f;
            }
            else if (t == 2)
            {
                dtime[0] = 40.f;
                dtime[1] = 71.f;
                dtime[2] = 53.f;
                dtime[3] = 15.f;
            }

            lp.clear();
            lp.resize(f_order);
        }

        void prepare(const dsp::ProcessSpec& spec)
        {
            auto monoSpec = spec;
        #if USE_SIMD
            monoSpec.numChannels = 1;
        #endif

            auto ratio = spec.sampleRate / 44100.0; // make delay times relative to sample-rate

            for (size_t i = 0; i < delay.size(); ++i)
            {
                delay[i].prepare(monoSpec);
                delay[i].setMaximumDelayInSamples(100 * ratio);
                delay[i].setDelay(dtime[i] * ratio);
            }

            for (size_t i = 0; i < lp.size(); ++i)
            {
                lp[i].prepare(spec);
                lp[i].setType(strix::FilterType::lowpass);
                lp[i].setCutoffFreq(5000.0 - (std::pow(2.0, (double)i) * 50.0));
                lp[i].setResonance(0.7);
            }
        }

        void reset()
        {
            for (auto& d : delay)
                d.reset();
            for (auto& f : lp)
                f.reset();
        }

        template <class Block>
        void processBlock(Block& block)
        {
            for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
            {
                auto in = block.getChannelPointer(ch);

                for (size_t i = 0; i < block.getNumSamples(); ++i)
                {
                    T out = 0.0;

                    for (size_t n = 0; n < f_order; ++n)
                    {
                        auto d = delay[n].popSample(ch, dtime[n]);

                        out += d;
                        if (n % 2 == 0)
                            out += in[i] * -fdbk;

                        auto f = d * fdbk + in[i];

                        f = lp[n].processSample(ch, f);

                        delay[n].pushSample(ch, f);
                    }

                    in[i] = out;
                }
            }

            block.multiplyBy(1.0 / f_order);
        }

    private:

        size_t f_order;

        std::vector<strix::Delay<T>> delay;
        std::vector<double> dtime;

        T fdbk = 0.1f;

        std::vector<strix::SVTFilter<T>> lp;

        AudioProcessorValueTreeState &apvts;
    };

    strix::SVTFilter<Type> hp, lp1, lp2, lowshelf;

    double sr = 44100.0;

    AudioProcessorValueTreeState &apvts;

    // std::array<FDN<Type>, 3> fdn{FDN<Type>(apvts, CabType::small), FDN<Type>(apvts, CabType::med), FDN<Type>(apvts, CabType::large)};
    std::shared_ptr<FDN<Type>> fdn;

    dsp::ProcessSpec memSpec;

    CabType type;

    strix::ReleasePoolShared releasePool;

public:
    FDNCab(AudioProcessorValueTreeState& a, CabType t) : apvts(a)
    {
        fdn = std::make_shared<FDN<Type>>(a, t);
    }

    void setCabType(CabType newType)
    {
        if (newType == CabType::off)
            return;
        
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

        lowshelf.prepare(spec);
        lowshelf.setType(strix::FilterType::firstOrderLowpass);

        fdn->prepare(spec);
        // for (auto& c : fdn)
        //     c.prepare(spec);
    }

    void reset()
    {
        // for (auto& c : fdn)
        //     c.reset();
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
            hp.setCutoffFreq(90.0);
            hp.setResonance(0.7);
            lp1.setCutoffFreq(6500.0);
            lp1.setResonance(0.7);
            lp2.setCutoffFreq(5067.0);
            lp2.setResonance(1.5);
            break;
        case med:
            hp.setCutoffFreq(70.0);
            hp.setResonance(1.0);
            lp1.setCutoffFreq(5011.0);
            lp1.setResonance(0.7);
            lp2.setCutoffFreq(4500.0);
            lp2.setResonance(1.29);
            lowshelf.setCutoffFreq(232.0);
            break;
        case large:
            hp.setCutoffFreq(80.0);
            hp.setResonance(2.0);
            lp1.setCutoffFreq(6033.0);
            lp1.setResonance(1.21);
            lp2.setCutoffFreq(3193.0);
            lp2.setResonance(1.5);
            lowshelf.setCutoffFreq(307.0);
            break;
        default:
            break;
        }
    }

    template <class Block>
    void processBlock(Block& block)
    {
        std::shared_ptr<FDN<Type>> proc_fdn = std::atomic_load(&fdn);

        // size_t cabIndex = (size_t)type;

        lp1.processBlock(block);
        proc_fdn->processBlock(block);
        hp.processBlock(block);
        if (type > 0)
        {
            auto lsgain = type > 1 ? 0.7 : 0.5;
            for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
            {
                auto in = block.getChannelPointer(ch);
                for (size_t i = 0; i < block.getNumSamples(); ++i)
                {
                    in[i] += lsgain * lowshelf.processSample(ch, in[i]);
                }
            }
        }
        lp2.processBlock(block);
    }
};