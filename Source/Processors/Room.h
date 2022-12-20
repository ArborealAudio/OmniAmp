// Room.h

#pragma once

/**
 * A struct for containing all reverb parameters, can be modified together or individually
 */
struct ReverbParams
{
    /* room size in Ms, use this in conjunction w/ rt60 to create bigger or larger rooms */
    float roomSizeMs;
    /* controls density of reverberation (basically the feedback) of the algo */
    float rt60;
    /* a gain value that sets the level of early reflections. Tapers logarithmically from this
    value to something lower. */
    float erLevel;
    /* Set the level of dampening, as a fraction of Nyquist, in the feedback path */
    float dampening;
    /* Sets the max frequency of the modulation. Each delay line will have a mod rate that
    logarithmically ranges from zero to this parameter */
    float modulation;
    /* Pre-delay in samples */
    float preDelay;
};

enum class ReverbType
{
    Off,
    Room,
    Hall
};

template <int channels, typename Type>
class Room
{
    template <int size>
    struct MixMatrix
    {
        MixMatrix() = default;

        // Expects an array of N channels worth of samples
        static inline void processHadamardMatrix(double *ch)
        {
            recursive(ch);

            auto scale = std::sqrt(1.0 / (double)size);

            for (int i = 0; i < size; ++i)
                ch[i] *= scale;
        }

        static inline void processHadamardMatrix(vec *ch)
        {
            recursive(ch);

            auto scale = xsimd::sqrt(1.0 / (vec)size);

            for (int i = 0; i < size; ++i)
                ch[i] *= scale;
        }

        // Expects an array of N channels worth of samples
        template <typename T>
        static void processHouseholder(T *ch)
        {
            const T h_mult = -2.0 / (T)size;

            T sum = 0.0;
            for (int j = 0; j < size; ++j)
                sum += ch[j];

            sum *= h_mult;

            for (int j = 0; j < size; ++j)
                ch[j] += sum;
        }

        template <typename T>
        static inline void recursive(T *data)
        {
            if (size <= 1)
                return;
            const int hSize = size / 2;

            MixMatrix<hSize>::recursive(data);
            MixMatrix<hSize>::recursive(data + hSize);

            for (int i = 0; i < hSize; ++i)
            {
                T a = data[i];
                T b = data[i + hSize];
                data[i] = a + b;
                data[i + hSize] = a - b;
            }
        }
    };

    template <typename T>
    struct Diffuser
    {
        /**
         * @param seed_ seed for delay time RNG
         */
        Diffuser(int64_t seed_) : seed(seed_)
        {
        }

        /**
        @param delayRangeinS you guessed it, delay range in seconds
        */
        Diffuser(float delayRangeinS, int64_t seed_) : delayRange(delayRangeinS), seed(seed_)
        {
        }

        /* delay range should be initialized before this is called */
        void prepare(dsp::ProcessSpec spec)
        {
            spec.numChannels = 1;
            SR = spec.sampleRate;

            invert.clear();

            Random rand(seed);

            int i = 0;
            auto delayRangeSamples = delayRange * SR;
            for (auto &d : delay)
            {
                double minDelay = delayRangeSamples * i / channels;
                double maxDelay = delayRangeSamples * (i + 1) / channels;
                d.prepare(spec);
                d.setMaximumDelayInSamples(2 * maxDelay + 1);
                randDelay[i] = rand.nextInt(Range<int>(minDelay, maxDelay));
                d.setDelay(randDelay[i]);
                invert.push_back(rand.nextInt() % 2 == 0);
                ++i;
            }
        }

        /* change delay ranges after changing main delayRange */
        void changeDelay()
        {
            invert.clear();

            Random rand(seed);

            for (size_t i = 0; i < delay.size(); ++i)
            {
                auto delayRangeSamples = delayRange * SR;
                double minDelay = delayRangeSamples * i / channels;
                double maxDelay = delayRangeSamples * (i + 1) / channels;
                randDelay[i] = rand.nextInt(Range<int>(minDelay, maxDelay));
                delay[i].setDelay(randDelay[i]);
                invert.push_back(rand.nextInt() % 2 == 0);
            }
        }

        void reset()
        {
            for (auto &d : delay)
                d.reset();
        }

        // expects a block of N channels
        template <typename Block>
        void process(Block &block)
        {
            for (auto i = 0; i < block.getNumSamples(); ++i)
            {
                std::vector<T> vec;

                for (auto ch = 0; ch < channels; ++ch)
                {
                    delay[ch].pushSample(0, block.getSample(ch, i));
                    vec.push_back(delay[ch].popSample(0));
                }

                MixMatrix<channels>::processHadamardMatrix(vec.data());

                for (auto ch = 0; ch < channels; ++ch)
                    block.getChannelPointer(ch)[i] = vec[ch];
            }
            for (auto ch = 0; ch < channels; ++ch)
                if (invert[ch])
                    FloatVectorOperations::multiply(block.getChannelPointer(ch), -1.0, block.getNumSamples());
        }

        float delayRange;

    private:
        std::array<dsp::DelayLine<T>, channels> delay;
        std::array<int, channels> randDelay;
        const int64_t seed;
        std::vector<bool> invert;
        double SR = 44100.0;
    };

    template <typename T>
    struct MixedFeedback
    {
        MixedFeedback() = default;

        void prepare(const dsp::ProcessSpec &spec)
        {
            SR = spec.sampleRate;

            double delaySamplesBase = delayMs * 0.001 * spec.sampleRate;

            for (int ch = 0; ch < channels; ++ch)
            {
                double r = ch * 1.0 / channels;
                delaySamples[ch] = std::pow(2.0, r) * delaySamplesBase;
                delays[ch].prepare(spec);
                delays[ch].setMaximumDelayInSamples(2 * delaySamples[ch] + 1);

                osc[ch].initialise([](double x)
                                   { return std::sin(x); });
                osc[ch].setFrequency(std::pow(modFreq, (double)ch / channels));
                osc[ch].prepare(spec);
            }

            auto multiSpec = spec;
            multiSpec.numChannels = channels;

            lp.prepare(multiSpec);
            lp.setType(strix::FilterType::firstOrderLowpass);
            lp.setCutoffFreq(dampening * multiSpec.sampleRate * 0.5);

            hp.prepare(multiSpec);
            hp.setType(strix::FilterType::firstOrderHighpass);
            hp.setCutoffFreq(150.0);
        }

        /* call an update on the mod freq, after updating the global value */
        void changeModFreq()
        {
            for (int ch = 0; ch < channels; ++ch)
                osc[ch].setFrequency(std::pow(modFreq, (double)ch / channels));
        }

        /* use this to change the feedback's delay times (don't change delayMS directly) */
        void changeDelay(float newDelayMs)
        {
            delayMs = newDelayMs;
            double delaySamplesBase = delayMs * 0.001 * SR;
            for (int ch = 0; ch < channels; ++ch)
            {
                double r = ch * 1.0 / channels;
                delaySamples[ch] = std::pow(2.0, r) * delaySamplesBase;
            }
        }

        void reset()
        {
            for (auto &d : delays)
                d.reset();

            lp.reset();
            hp.reset();
            for (auto &o : osc)
                o.reset();
        }

        template <typename Block>
        void process(Block &block)
        {
            for (int i = 0; i < block.getNumSamples(); ++i)
            {
                for (int ch = 0; ch < channels; ++ch)
                {
                    auto mod = osc[ch].processSample(delaySamples[ch]);
                    auto dtime = delaySamples[ch] - (0.2 * mod);
                    auto d = delays[ch].popSample(0, dtime);
                    d = lp.processSample(ch, d);
                    d = hp.processSample(ch, d);
                    delayed[ch] = d;
                }

                MixMatrix<channels>::processHouseholder(delayed.data());

                for (int ch = 0; ch < channels; ++ch)
                {
                    auto in = block.getChannelPointer(ch)[i];
                    auto sum = in + delayed[ch] * decayGain;
                    delays[ch].pushSample(0, sum);

                    block.setSample(ch, i, delayed[ch]);
                }
            }
        }

        T delayMs = 150.0;
        T decayGain = 0.85;

        /*A fraction of Nyquist, where the lowpass will be placed in the feedback path*/
        T dampening = 1.0;

        T modFreq = 1.0;

    private:
        std::array<int, channels> delaySamples;
        std::array<dsp::DelayLine<T>, channels> delays;

        // container for N number of delayed & filtered samples
        std::array<T, channels> delayed;

        strix::SVTFilter<T> lp, hp;

        std::array<dsp::Oscillator<T>, channels> osc;

        double SR = 44100.0;
    };

    std::vector<Diffuser<Type>> diff;

    MixedFeedback<Type> feedback;

    AudioBuffer<double> splitBuf, erBuf, dsBuf, usBuf;

    template <typename Sample>
    class StereoMultiMixer
    {
        static_assert((channels / 2) * 2 == channels, "StereoMultiMixer must have an even number of channels");
        static_assert(channels >= 2, "StereoMultiMixer must have an even number of channels");
        static constexpr int hChannels = channels / 2;
        std::array<Sample, channels> coeffs;

    public:
        StereoMultiMixer()
        {
            coeffs[0] = 1;
            coeffs[1] = 0;
            for (int i = 1; i < hChannels; ++i)
            {
                double phase = MathConstants<float>::pi * i / channels;
                coeffs[2 * i] = std::cos(phase);
                coeffs[2 * i + 1] = std::sin(phase);
            }
        }

        void stereoToMulti(const Sample **input, Sample **output, int numSamples) const
        {
            for (int j = 0; j < numSamples; ++j)
            {
                output[0][j] = input[0][j];
                output[1][j] = input[1][j];
                for (int i = 2; i < channels; i += 2)
                {
                    output[i][j] = input[0][j] * coeffs[i] + input[1][j] * coeffs[i + 1];
                    output[i + 1][j] = input[1][j] * coeffs[i] - input[0][j] * coeffs[i + 1];
                }
            }
        }

        void multiToStereo(const Sample **input, Sample **output, int numSamples) const
        {
            for (int j = 0; j < numSamples; ++j)
            {
                output[0][j] = input[0][j];
                output[1][j] = input[1][j];
                for (int i = 2; i < channels; i += 2)
                {
                    output[0][j] += input[i][j] * coeffs[i] - input[i + 1][j] * coeffs[i + 1];
                    output[1][j] += input[i + 1][j] * coeffs[i] + input[i][j] * coeffs[i + 1];
                }
            }
        }

        /// Scaling factor for the downmix, if channels are phase-aligned
        static constexpr Sample scalingFactor1() { return 2 / Sample(channels); }

        /// Scaling factor for the downmix, if channels are independent
        static Sample scalingFactor2() { return std::sqrt(scalingFactor1()); }
    };

    StereoMultiMixer<Type> upMix;

    double erLevel = 1.0;

    int ratio = 1;

    std::vector<dsp::IIR::Filter<Type>> dsLP[2], usLP[2];

    /**
     * @param numSamples number of samples in the input buffer
     */
    void downsampleBuffer(const AudioBuffer<double> &buf, int numSamples)
    {
        auto in = buf.getArrayOfReadPointers();
        auto out = dsBuf.getArrayOfWritePointers();

        for (auto ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            for (auto i = 0; i < numSamples; ++i)
            {
                auto f = in[ch][i];
                for (auto &lp : dsLP[ch])
                    f = lp.processSample(f); // processes full SR
                auto n = i / ratio;
                out[ch][n] = f;
            }

            // copy L->R if the input is mono
            if (buf.getNumChannels() < dsBuf.getNumChannels())
                FloatVectorOperations::copy(dsBuf.getWritePointer(1), dsBuf.getReadPointer(0), numSamples / ratio);
        }
    }

    /**
     * @param numSamples number of samples in the output buffer
     */
    void upsampleBuffer(AudioBuffer<double> &outBuf, int numSamples)
    {
        auto out = outBuf.getArrayOfWritePointers();
        auto in = dsBuf.getArrayOfReadPointers();

        for (auto ch = 0; ch < outBuf.getNumChannels(); ++ch)
        {
            for (auto i = 0; i < numSamples / ratio; ++i)
            {
                auto n = i * ratio;
                out[ch][n] = in[ch][i]; // copy every even sample
                for (int j = n + 1; j < n + ratio; ++j)
                    out[ch][j] = 0.0;
            }

            for (auto i = 0; i < numSamples; ++i)
            {
                for (auto &lp : usLP[ch])
                    out[ch][i] = lp.processSample(out[ch][i]); // processes @ full SR
            }
        }

        FloatVectorOperations::multiply(out[0], 2.0, outBuf.getNumSamples());
        if (outBuf.getNumChannels() > 1)
            FloatVectorOperations::multiply(out[1], 2.0, outBuf.getNumSamples());
    }

    dsp::DryWetMixer<double> mix;

    int numChannels = 0;

public:
    /* our local reverb parameters */
    ReverbParams params;

    dsp::DelayLine<double, dsp::DelayLineInterpolationTypes::Thiran> preDelay{4410}; /* this needs to have a different interpolation (i.e. any interpolation at all) */

    Room(ReverbType initType)
    {
        for (size_t i = 0; i < 4; ++i)
            diff.emplace_back((int64_t)i);

        switch (initType)
        {
        case ReverbType::Off: /* just use Room as the default if initialized to Off */
            setReverbParams(ReverbParams{30.f, 0.65f, 0.5f, 0.23f, 3.f, 0.f});
            break;
        case ReverbType::Room:
            setReverbParams(ReverbParams{30.f, 0.65f, 0.5f, 0.23f, 3.f, 0.f});
            break;
        case ReverbType::Hall:
            setReverbParams(ReverbParams{75.0f, 2.0f, 0.5f, 1.0f, 5.0f, 0.f});
            break;
        }
    }

    Room(const ReverbParams &_params)
    {
        for (size_t i = 0; i < 4; ++i)
            diff.emplace_back((int64_t)i);
        setReverbParams(_params);
    }

    /**
     * method for setting all reverb parameters, useful for changing btw reverb types
     * @param init whether or not this is the initial call to setReverbParams. True by default, this defers setting up the diffuser delay lines until prepare(). If false, this will change the diffuser delay lines. Pass false if changing params after initialization*/
    void setReverbParams(const ReverbParams &newParams, bool init = true)
    {
        params = newParams;

        params.preDelay = newParams.preDelay;
        preDelay.setDelay(params.preDelay);

        params.roomSizeMs = jmax(params.roomSizeMs, params.rt60 * 10.f);

        auto diffusion = (params.roomSizeMs * 0.001);
        assert(diff.size() == 4);
        for (int i = 0; i < 4; ++i)
        {
            diffusion *= 0.5;
            diff[i].delayRange = diffusion;
            if (!init)
                diff[i].changeDelay();
        }

        erLevel = params.erLevel;

        feedback.changeDelay(params.roomSizeMs);

        double typicalLoopMs = params.roomSizeMs * 1.5;

        double loopsPerRt60 = params.rt60 / (typicalLoopMs * 0.001);

        double dbPerCycle = -60.0 / loopsPerRt60;

        feedback.decayGain = std::pow(10.0, dbPerCycle * 0.05);
        feedback.dampening = params.dampening;
        feedback.modFreq = params.modulation;
        if (!init)
            feedback.changeModFreq();
    }

    void setPredelay(float newPredelay)
    {
        params.preDelay = newPredelay;
        preDelay.setDelay(params.preDelay);
    }

    /** PROBLEM: We're not accounting for samplerates higher than 44.1kHz! We need to refactor it to hardcode a SR of 22kHz
     *  call this before Prepare! sets a ratio to downsample the internal processing, defaults to 1 */
    void setDownsampleRatio(int dsRatio) { ratio = dsRatio; }

    /* pass in the down-sampled specs */
    void prepare(const dsp::ProcessSpec &spec)
    {
        numChannels = spec.numChannels;

        usBuf.setSize(numChannels, spec.maximumBlockSize * ratio);
        splitBuf.setSize(channels, spec.maximumBlockSize);
        erBuf.setSize(channels, spec.maximumBlockSize);
        dsBuf.setSize(numChannels, spec.maximumBlockSize);

        preDelay.prepare(spec);

        for (auto &d : diff)
            d.prepare(spec);

        feedback.prepare(spec);

        auto coeffs = dsp::FilterDesign<double>::designIIRLowpassHighOrderButterworthMethod(spec.sampleRate / ratio, spec.sampleRate * (double)ratio, 8);

        dsLP[0].clear();
        dsLP[1].clear();
        usLP[0].clear();
        usLP[1].clear();

        for (auto &c : coeffs)
        {
            dsLP[0].emplace_back(dsp::IIR::Filter<double>(c));
            dsLP[1].emplace_back(dsp::IIR::Filter<double>(c));

            usLP[0].emplace_back(dsp::IIR::Filter<double>(c));
            usLP[1].emplace_back(dsp::IIR::Filter<double>(c));
        }

        // ratio *= spec.sampleRate / 44100.0;

        auto filterSpec = spec;
        filterSpec.sampleRate *= (double)ratio;
        filterSpec.maximumBlockSize *= ratio;

        for (auto &ch : dsLP)
            for (auto &f : ch)
                f.prepare(filterSpec);

        for (auto &ch : usLP)
            for (auto &f : ch)
                f.prepare(filterSpec);

        mix.prepare(dsp::ProcessSpec{spec.sampleRate * ratio, spec.maximumBlockSize * ratio, (uint32)numChannels});
        mix.setMixingRule(dsp::DryWetMixingRule::balanced);
    }

    void reset()
    {
        for (auto &d : diff)
            d.reset();

        preDelay.reset();
        feedback.reset();

        for (auto &ch : dsLP)
            for (auto &f : ch)
                f.reset();

        for (auto &ch : usLP)
            for (auto &f : ch)
                f.reset();

        usBuf.clear();
        splitBuf.clear();
        erBuf.clear();
        dsBuf.clear();

        mix.reset();
    }

    void process(AudioBuffer<double> &buf, float amt)
    {
        auto numSamples = buf.getNumSamples();
        auto hNumSamples = numSamples / ratio;

        if (numChannels > 1)
            mix.pushDrySamples(dsp::AudioBlock<double>(buf).getSubBlock(0, numSamples));
        else
            mix.pushDrySamples(dsp::AudioBlock<double>(buf).getSingleChannelBlock(0).getSubBlock(0, numSamples));

        dsBuf.clear();
        splitBuf.clear();

        downsampleBuffer(buf, numSamples);

        dsp::AudioBlock<double> dsBlock (dsBuf);
        // preDelay.processBlock(dsBlock);
        preDelay.process(dsp::ProcessContextReplacing<double>(dsBlock));

        upMix.stereoToMulti(dsBuf.getArrayOfReadPointers(), splitBuf.getArrayOfWritePointers(), hNumSamples);

        dsp::AudioBlock<double> block(splitBuf);

        if (splitBuf.getNumSamples() > hNumSamples)
            block = block.getSubBlock(0, hNumSamples);

        erBuf.clear();

        for (auto i = 0; i < diff.size(); ++i)
        {
            diff[i].process(block);
            auto r = i * 1.0 / diff.size();
            for (auto ch = 0; ch < channels; ++ch)
                erBuf.addFrom(ch, 0, block.getChannelPointer(ch), block.getNumSamples(), erLevel / std::pow(2.0, r));
        }

        feedback.process(block);

        block.add(dsp::AudioBlock<double>(erBuf).getSubBlock(0, hNumSamples));

        upMix.multiToStereo(splitBuf.getArrayOfReadPointers(), dsBuf.getArrayOfWritePointers(), hNumSamples);

        upsampleBuffer(usBuf, numSamples);

        usBuf.applyGain(upMix.scalingFactor1());

        mix.setWetMixProportion(amt);
        if (numChannels > 1)
            mix.mixWetSamples(dsp::AudioBlock<double>(usBuf).getSubBlock(0, numSamples));
        else
            mix.mixWetSamples(dsp::AudioBlock<double>(usBuf).getSingleChannelBlock(0).getSubBlock(0, numSamples));

        buf.makeCopyOf(usBuf, true);
    }
};

class ReverbManager : AudioProcessorValueTreeState::Listener
{
    AudioProcessorValueTreeState &apvts;
    Room<8, double> rev;
    std::atomic<bool> needsUpdate = false;

    enum reverb_flags
    {
        ANY,
        PREDELAY,
    };
    reverb_flags lastFlag;

    strix::ChoiceParameter *type;
    strix::FloatParameter *decay, *size, *predelay;

    double SR = 44100.0;

    int ratio = 1;

public:
    ReverbManager(AudioProcessorValueTreeState &v) : apvts(v), rev(ReverbParams{75.0f, 2.0f, 0.5f, 1.0f, 5.0f, 0.f})
    {
        type = (strix::ChoiceParameter *)apvts.getParameter("reverbType");
        decay = (strix::FloatParameter *)apvts.getParameter("reverbDecay");
        size = (strix::FloatParameter *)apvts.getParameter("reverbSize");
        predelay = (strix::FloatParameter *)apvts.getParameter("reverbPredelay");
        apvts.addParameterListener("reverbType", this);
        apvts.addParameterListener("reverbDecay", this);
        apvts.addParameterListener("reverbSize", this);
        apvts.addParameterListener("reverbPredelay", this);
    }

    ~ReverbManager()
    {
        apvts.removeParameterListener("reverbType", this);
        apvts.removeParameterListener("reverbDecay", this);
        apvts.removeParameterListener("reverbSize", this);
        apvts.removeParameterListener("reverbPredelay", this);
    }

    void setDownsampleRatio(int dsRatio)
    {
        rev.setDownsampleRatio(dsRatio);
        ratio = dsRatio;
    }

    void prepare(const dsp::ProcessSpec &spec)
    {
        auto hSpec = spec;
        hSpec.sampleRate /= ratio;
        hSpec.maximumBlockSize /= ratio;
        rev.prepare(hSpec);
        SR = hSpec.sampleRate;
    }

    void reset()
    {
        rev.reset();
    }

    void parameterChanged(const String &paramID, float) override
    {
        if (paramID == "reverbType" || paramID == "reverbDecay" || paramID == "reverbSize")
            lastFlag = ANY;
        else if (paramID == "reverbPredelay")
            lastFlag = PREDELAY;

        needsUpdate = true;
    }

    void manageUpdate(reverb_flags flags)
    {
        switch (flags)
        {
        case ANY:
        {
            float d = *decay;
            d = jmax(0.01f, d);
            float s = *size;
            s = jmax(0.01f, s);
            float p = *predelay * 0.001f * SR;
            switch ((ReverbType)type->getIndex())
            {
            case ReverbType::Room:
                rev.setReverbParams(ReverbParams{30.f * s, 0.65f * d, 0.5f, 0.23f, 3.f, p}, false);
                break;
            case ReverbType::Hall:
                rev.setReverbParams(ReverbParams{75.0f * s, 2.0f * d, 0.5f, 1.0f, 5.0f, p}, false);
                break;
            case ReverbType::Off:
                break;
            }
            break;
        }
        case PREDELAY:
            float p = *predelay * 0.001f * SR;
            rev.setPredelay(p);
            break;
        }
    }

    void process(AudioBuffer<double> &buffer, float amt)
    {
        if (needsUpdate)
        {
            manageUpdate(lastFlag);
            needsUpdate = false;
        }
        if (!needsUpdate && *type)
            rev.process(buffer, amt);
    }
};