// Cab.h

#pragma once

enum CabType
{
    small,
    med,
    large
};

class FDNCab
{
    struct FDN
    {
        FDN(CabType t)
        {
            switch (t)
            {
            case small:
                f_order = 1;
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
            delay.resize(f_order);
            dtime.resize(f_order);
            z[0].resize(f_order);
            z[1].resize(f_order);

            lp.resize(f_order);
            hp.resize(f_order);

            dtime[0] = 103.f;
            if (t > 0)
            {
                dtime[1] = 85.f;
                dtime[2] = 57.5f;
                dtime[3] = 81.5f;
            }
            // MUST change these to ms or µs values so as to be samplerate-agnostic
        }

        void prepare(const dsp::ProcessSpec &spec)
        {
            for (auto &d : delay) {
                d.prepare(spec);
                d.setMaximumDelayInSamples(44100);
            }

            double i = 1.0;
            for (auto &f : lp) {
                f.prepare(spec);
                f.setType(dsp::StateVariableTPTFilterType::lowpass);
                f.setCutoffFrequency(5000.0 - (i * 500.0));
                f.setResonance(0.7);
                ++i;
            }

            i = 1.0;
            for (auto &f : hp)
            {
                f.prepare(spec);
                f.setType(dsp::StateVariableTPTFilterType::highpass);
                f.setCutoffFrequency(100.0 - (20.0 * i));
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
            for (auto& ch : z)
                for (auto& s : ch)
                    s = 0.0;
        }

        void processBlock(dsp::AudioBlock<double>& block)
        {
            auto left = block.getChannelPointer(0);
            auto right = left;
            if (block.getNumChannels() > 1)
                right = block.getChannelPointer(1);

            for (auto i = 0; i < block.getNumSamples(); ++i)
            {
                double out_l = 0.0, out_r = 0.0;

                for (auto n = 0; n < f_order; ++n)
                {
                    auto d_l = delay[n].popSample(0, dtime[n]);
                    auto d_r = delay[n].popSample(1, dtime[n]);

                    out_l += d_l + left[i] * -fdbk;
                    out_r += d_r + right[i] * -fdbk;

                    z[0][n] = out_l;
                    z[1][n] = out_r;

                    auto f_l = z[0][n] * fdbk + left[i];
                    auto f_r = z[1][n] * fdbk + right[i];

                    f_l = hp[n].processSample(0, f_l);
                    f_r = hp[n].processSample(1, f_r);

                    f_l = lp[n].processSample(0, f_l);
                    f_r = lp[n].processSample(1, f_r);

                    delay[n].pushSample(0, f_l);
                    delay[n].pushSample(1, f_r);
                }

                left[i] = out_l;
                right[i] = out_r;
            }
        }

    private:
        size_t f_order;

        std::vector<dsp::DelayLine<double>> delay;
        std::vector<float> dtime;

        float fdbk = 0.06f;
        std::vector<double> z[2];

        std::vector<dsp::StateVariableTPTFilter<double>> lp, hp;
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

        template<typename T>
        void add(const std::shared_ptr<T>& object)
        {
            if (object == nullptr)
                return;

            std::lock_guard<std::mutex> lock(m);
            pool.emplace_back(object);
        }
    };

    dsp::StateVariableTPTFilter<double> hp, lp1, lp2, bp;

    std::shared_ptr<FDN> fdn;

    dsp::ProcessSpec memSpec;

    CabType type;

    ReleasePool releasePool;

public:
    FDNCab(CabType t)
    {
        fdn = std::make_shared<FDN>(t);
    }

    void setCabType(CabType newType)
    {
        type = newType;
        setParams();

        std::shared_ptr<FDN> new_fdn = std::make_shared<FDN>(newType);
        new_fdn->prepare(memSpec);
        releasePool.add(new_fdn);
        std::atomic_store(&fdn, new_fdn);
    }

    void prepare(const dsp::ProcessSpec &spec)
    {
        memSpec = spec;

        hp.prepare(spec);
        hp.setType(dsp::StateVariableTPTFilterType::highpass);

        lp1.prepare(spec);
        lp2.prepare(spec);
        lp1.setType(dsp::StateVariableTPTFilterType::lowpass);
        lp2.setType(dsp::StateVariableTPTFilterType::firstOrderLowpass);

        bp.prepare(spec);
        bp.setType(dsp::StateVariableTPTFilterType::notch);

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
            hp.setCutoffFrequency(90.0);
            hp.setResonance(5.0);
            lp1.setCutoffFrequency(3500.0);
            lp1.setResonance(0.7);
            lp2.setCutoffFrequency(5000.0);
            lp2.setResonance(1.5);
            break;
        case med:
            hp.setCutoffFrequency(75.0);
            hp.setResonance(0.8);
            lp1.setCutoffFrequency(2821.0);
            lp1.setResonance(0.7);
            lp2.setCutoffFrequency(4191.0);
            lp2.setResonance(1.2);
            bp.setCutoffFrequency(1700.0);
            bp.setResonance(2.0);
            break;
        case large:
            break;
        default:
            break;
        }
    }

    void processBlock(dsp::AudioBlock<double> &block)
    {
        std::shared_ptr<FDN> proc_fdn = std::atomic_load(&fdn);

        lp1.process(dsp::ProcessContextReplacing<double>(block));
        proc_fdn->processBlock(block);
        hp.process(dsp::ProcessContextReplacing<double>(block));
        if (type > 0)
            bp.process(dsp::ProcessContextReplacing<double>(block));
        lp2.process(dsp::ProcessContextReplacing<double>(block));
    }
};
