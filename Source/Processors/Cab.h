// Cab.h

#pragma once

enum CabType
{
    small,
    med,
    large
};

template <typename Type>
class FDNCab : AudioProcessorValueTreeState::Listener
{
    template <typename T>
    struct FDN
    {
        CabType type;

        FDN(AudioProcessorValueTreeState &a, CabType t) : apvts(a), type(t)
        {
            micDepth = static_cast<strix::FloatParameter *>(apvts.getParameter("cabMicPosZ"));
            micPos = static_cast<strix::FloatParameter *>(apvts.getParameter("cabMicPosX"));

            changeDelay();
        }

        void prepare(const dsp::ProcessSpec &spec)
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

            for (auto i = 0; i < f_order; ++i)
            {
                lp[i].prepare(spec);
                lp[i].setType(strix::FilterType::lowpass);
                lp[i].setCutoffFreq(5000.0 - (std::pow(2.0, (double)i) * 50.0));
                lp[i].setResonance(0.7);
            }

            float mic_ = jmap(micPos->get(), 0.8f, 1.f);
            ap.prepare(spec);
            ap.setType(strix::FilterType::allpass);
            ap.setCutoffFreq(3500.0 * mic_);
            ap.setResonance(0.7);
        }

        void changeDelay()
        {
            switch (type)
            {
            case small:
                dtime[0] = 40.f;
                dtime[1] = 57.f;
                dtime[2] = 73.f;
                dtime[3] = 79.f;
                break;
            case med:
                dtime[0] = 57.f;
                dtime[1] = 24.f;
                dtime[2] = 40.f;
                dtime[3] = 79.f;
                break;
            case large:
                dtime[0] = 40.f;
                dtime[1] = 71.f;
                dtime[2] = 53.f;
                dtime[3] = 15.f;
                break;
            }
        }

        void changeAllpass(float newValue)
        {
            float mic_ = jmap(newValue, 0.8f, 1.f);
            ap.setCutoffFreq(3500.0 * mic_);
        }

        void reset()
        {
            for (auto &d : delay)
                d.reset();
            for (auto &f : lp)
                f.reset();
            ap.reset();
        }

        template <class Block>
        void processBlock(Block &block)
        {
            float mic_ = micDepth->get() * 0.5f;

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

                    out += mic_ * ap.processSample(ch, out);
                    out *= 0.65f;

                    in[i] = out;
                }
            }

            block.multiplyBy(1.0 / (f_order * 2.0));
        }

    private:
        static constexpr size_t f_order = 4;

        std::array<strix::Delay<T>, f_order> delay;
        std::array<double, f_order> dtime;

        T fdbk = 0.1f;

        std::array<strix::SVTFilter<T>, f_order> lp;
        strix::SVTFilter<T> ap;

        AudioProcessorValueTreeState &apvts;

        strix::FloatParameter *micDepth, *micPos;
    };

    strix::SVTFilter<Type> hp, lp1, lp2, lowshelf, ap;

    double sr = 44100.0;

    AudioProcessorValueTreeState &apvts;

    FDN<Type> fdn;

    CabType type;

    strix::FloatParameter *micPos;
    strix::ChoiceParameter *type_p;

    std::atomic<bool> updateType = false;

public:
    FDNCab(AudioProcessorValueTreeState &a, CabType t) : apvts(a), fdn(a, t)
    {
        micPos = static_cast<strix::FloatParameter *>(apvts.getParameter("cabMicPosX"));
        type_p = static_cast<strix::ChoiceParameter *>(apvts.getParameter("cabType"));
        apvts.addParameterListener("cabMicPosX", this);
        // apvts.addParameterListener("cabMicPosZ", this);
        apvts.addParameterListener("cabType", this);
    }

    ~FDNCab()
    {
        apvts.removeParameterListener("cabMicPosX", this);
        // apvts.removeParameterListener("cabMicPosZ", this);
        apvts.removeParameterListener("cabType", this);
    }

    void parameterChanged(const String &paramID, float newValue) override
    {
        if (paramID == "cabMicPosX")
        {
            setParams();
            fdn.changeAllpass(newValue);
        }
        else if (paramID == "cabType")
            updateType = true;
    }

    void setCabType()
    {
        auto newType = type_p->getIndex();

        if (newType == 0)
            return;

        type = static_cast<CabType>(newType - 1);
        fdn.type = type;
        fdn.changeDelay();
        setParams();
    }

    void prepare(const dsp::ProcessSpec &spec)
    {
        sr = spec.sampleRate;

        hp.prepare(spec);
        hp.setType(strix::FilterType::highpass);

        lp1.prepare(spec);
        lp2.prepare(spec);
        lp1.setType(strix::FilterType::lowpass);
        lp2.setType(strix::FilterType::firstOrderLowpass);

        lowshelf.prepare(spec);
        lowshelf.setType(strix::FilterType::firstOrderLowpass);

        ap.prepare(spec);
        ap.setType(strix::FilterType::allpass);
        ap.setResonance(0.5);

        fdn.prepare(spec);

        setCabType();
    }

    void reset()
    {
        fdn.reset();
        hp.reset();
        lp1.reset();
        lp2.reset();
        ap.reset();
    }

    void setParams()
    {
        auto mic_ = jmap(micPos->get(), 0.75f, 1.f);

        switch (type)
        {
        case small:
            hp.setCutoffFreq(90.0);
            hp.setResonance(0.7);
            lp1.setCutoffFreq(6500.0 * mic_);
            lp1.setResonance(0.7);
            lp2.setCutoffFreq(5067.0 * mic_);
            lp2.setResonance(1.5);
            break;
        case med:
            hp.setCutoffFreq(70.0);
            hp.setResonance(1.0);
            lp1.setCutoffFreq(5011.0 * mic_);
            lp1.setResonance(0.7);
            lp2.setCutoffFreq(4500.0 * mic_);
            lp2.setResonance(1.29);
            lowshelf.setCutoffFreq(232.0);
            break;
        case large:
            hp.setCutoffFreq(80.0);
            hp.setResonance(2.0);
            lp1.setCutoffFreq(6033.0 * mic_);
            lp1.setResonance(1.21);
            lp2.setCutoffFreq(3193.0 * mic_);
            lp2.setResonance(1.5);
            lowshelf.setCutoffFreq(307.0);
            break;
        default:
            break;
        }

        ap.setCutoffFreq(5000.0 * jmap(micPos->get(), 0.1f, 1.f));
    }

    template <class Block>
    void processBlock(Block &block)
    {
        if (updateType)
        {
            setCabType();
            updateType = false;
        }

        lp1.processBlock(block);
        fdn.processBlock(block);
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

        for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto *in = block.getChannelPointer(ch);
            for (size_t i = 0; i < block.getNumSamples(); ++i)
            {
                in[i] += 0.4f * ap.processSample(ch, in[i]);
                in[i] *= 0.65f;
            }
        }

        lp2.processBlock(block);
    }
};