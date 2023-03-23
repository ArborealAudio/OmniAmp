// Cab.h

#pragma once
#include <JuceHeader.h>

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
    struct FDN : AudioProcessorValueTreeState::Listener
    {
        CabType type;

        FDN(AudioProcessorValueTreeState &a, CabType t) : apvts(a), type(t)
        {
            micDepth = static_cast<strix::FloatParameter *>(apvts.getParameter("cabMicPosZ"));
            apvts.addParameterListener("cabMicPosZ", this);
            micPos = static_cast<strix::FloatParameter *>(apvts.getParameter("cabMicPosX"));

            changeDelay();
        }

        ~FDN()
        {
            apvts.removeParameterListener("cabMicPosZ", this);
        }

        void parameterChanged(const String &, float newValue) override
        {
            sm_micDepth.setTargetValue(newValue);
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

            changeAllpass(micPos->get());
            ap.prepare(spec);
            ap.setResonance(0.7);
            ap.setType(strix::FilterType::allpass);

            sm_micDepth.reset(spec.sampleRate, 0.01f);
            sm_micDepth.setCurrentAndTargetValue(micDepth->get());
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
                dtime[0] = (45.f);
                dtime[1] = (71.f);
                dtime[2] = (51.f);
                dtime[3] = (5.f);
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

        template <typename Block>
        void processBlock(Block &block)
        {
            if (sm_micDepth.isSmoothing())
            {
                for (size_t i = 0; i < block.getNumSamples(); ++i)
                {
                    auto depth_ = 0.5f * sm_micDepth.getNextValue();

                    for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
                    {
                        T out = 0.0;
                        auto in = block.getChannelPointer(ch);

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

                        out += depth_ * ap.processSample(ch, out);
                        out *= 0.65f;

                        in[i] = out;
                    }
                }
                block.multiplyBy(1.0 / (f_order * 2.0));
                return;
            }

            float micDepth_ = micDepth->get() * 0.5f;

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

                    out += micDepth_ * ap.processSample(ch, out);
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

        double fdbk = 0.1;

        std::array<strix::SVTFilter<T>, f_order> lp;
        strix::SVTFilter<T, true> ap;

        AudioProcessorValueTreeState &apvts;

        strix::FloatParameter *micDepth, *micPos;
        SmoothedValue<float> sm_micDepth;
    };

    strix::SVTFilter<Type, true> hp, lp1, lp2, ap;
    strix::SVTFilter<Type> lowshelf;
    double sr = 44100.0;

    AudioProcessorValueTreeState &apvts;

    FDN<Type> fdn;

#if USE_SIMD
    strix::Buffer<Type> apBuffer;
#else
    AudioBuffer<Type> apBuffer;
#endif

    CabType type;

    strix::FloatParameter *micPos, *resoLo, *resoHi;
    strix::ChoiceParameter *type_p;

    std::atomic<bool> updateType = false;

public:
    FDNCab(AudioProcessorValueTreeState &a, CabType t) : apvts(a), fdn(a, t)
    {
        micPos = static_cast<strix::FloatParameter *>(apvts.getParameter("cabMicPosX"));
        type_p = static_cast<strix::ChoiceParameter *>(apvts.getParameter("cabType"));
        resoLo = static_cast<strix::FloatParameter *>(apvts.getParameter("cabResoLo"));
        resoHi = static_cast<strix::FloatParameter *>(apvts.getParameter("cabResoHi"));
        apvts.addParameterListener("cabMicPosX", this);
        // apvts.addParameterListener("cabMicPosZ", this);
        apvts.addParameterListener("cabType", this);
        apvts.addParameterListener("cabResoLo", this);
        apvts.addParameterListener("cabResoHi", this);
    }

    ~FDNCab()
    {
        apvts.removeParameterListener("cabMicPosX", this);
        // apvts.removeParameterListener("cabMicPosZ", this);
        apvts.removeParameterListener("cabType", this);
        apvts.removeParameterListener("cabResoLo", this);
        apvts.removeParameterListener("cabResoHi", this);
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
        else if (paramID == "cabResoLo" || paramID == "cabResoHi")
            setParams();
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

        apBuffer.setSize(spec.numChannels, spec.maximumBlockSize);

        setCabType();

        hp.prepare(spec);
        hp.setType(strix::FilterType::highpass);

        lp1.prepare(spec);
        lp2.prepare(spec);
        lp1.setType(strix::FilterType::lowpass);
        lp2.setType(strix::FilterType::firstOrderLowpass);

        lowshelf.prepare(spec);
        lowshelf.setType(strix::FilterType::firstOrderLowpass);

        ap.setResonance(0.5);
        ap.prepare(spec);
        ap.setType(strix::FilterType::allpass);

        fdn.prepare(spec);
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
        auto resoLo_ = resoLo->get();
        auto resoHi_ = resoHi->get();

        switch (type)
        {
        case small:
            hp.setCutoffFreq(90.0);
            hp.setResonance(0.7 * resoLo_);
            lp1.setCutoffFreq(6500.0 * mic_);
            lp1.setResonance(0.7 * resoHi_);
            lp2.setCutoffFreq(5067.0 * mic_);
            lp2.setResonance(1.5 * resoHi_);
            break;
        case med:
            hp.setCutoffFreq(70.0);
            hp.setResonance(1.0 * resoLo_);
            lp1.setCutoffFreq(5511.0 * mic_);
            lp1.setResonance(1.25 * resoHi_);
            lp2.setCutoffFreq(4500.0 * mic_);
            lp2.setResonance(1.29 * resoHi_);
            lowshelf.setCutoffFreq(232.0);
            break;
        case large:
            hp.setCutoffFreq(80.0);
            hp.setResonance(1.0 * resoLo_);
            lp1.setCutoffFreq(7033.0 * mic_);
            lp1.setResonance(1.21 * resoHi_);
            lp2.setCutoffFreq(4193.0 * mic_);
            lp2.setResonance(1.5 * resoHi_);
            lowshelf.setCutoffFreq(307.0);
            break;
        }

        ap.setCutoffFreq(5000.0 * jmap(micPos->get(), 0.1f, 1.f));
    }

    template <typename Block>
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
            // auto lsgain = type > 1 ? 0.7 : 0.5;
            auto lsgain = 0.5;
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
            apBuffer.copyFrom(ch, 0, block.getChannelPointer(ch), block.getNumSamples());

        auto apBlock = Block(apBuffer);

        // PROBLEM: We need to call ap block-wise since that's how the built-in smoother works
        ap.processBlock(apBlock.getSubBlock(0, block.getNumSamples()));
        apBlock *= 0.2;
        block += apBlock;
        block *= 0.65;

        lp2.processBlock(block);
    }
};