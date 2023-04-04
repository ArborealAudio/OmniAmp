/**
 * Diffuser.h
 * Class for processing multi-channel diffusion
 */

template <typename T, int channels>
struct Diffuser
{
    /**
     * @param seed_ seed for delay time RNG
     */
    Diffuser(int64_t seed_) : seed(seed_)
    {
    }

    /**
     * @param delayRangeinS you guessed it, delay range in seconds
     * @param seed_ seed for delay time RNG
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
            d.setMaximumDelayInSamples(SR);
            d.setDelay((minDelay + maxDelay) / 2.0); // just use the average!
            invert.push_back(rand.nextInt() % 2 == 0);
            ++i;
        }
    }

    /* change delay ranges after changing main delayRange */
    void changeDelay()
    {
        invert.clear();

        Random rand(seed);

        auto delayRangeSamples = delayRange * SR;
        for (size_t i = 0; i < delay.size(); ++i)
        {
            double minDelay = delayRangeSamples * i / channels;
            double maxDelay = delayRangeSamples * (i + 1) / channels;
            auto nDelay = (minDelay + maxDelay) / 2.0;
            delay[i].setDelay(nDelay); // just use the average!
            invert.push_back(rand.nextInt() % 2 == 0);
        }
    }

    /* post an updated delay parameter for smooth changes */
    void updateDelay(float newDelay)
    {
        delayRange = newDelay;
        changeDelay();
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
        // if (needUpdate)
        // {
        //     processSmooth(block);
        //     needUpdate = false;
        //     return;
        // }
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

    template <typename Block>
    void processSmooth(Block &block)
    {
        for (auto i = 0; i < block.getNumSamples(); ++i)
        {
            changeDelay();

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
    std::array<dsp::DelayLine<T, dsp::DelayLineInterpolationTypes::Linear>, channels> delay;
    std::array<int, channels> randDelay;
    const int64_t seed;
    std::vector<bool> invert;
    double SR = 44100.0;
    std::atomic<bool> needUpdate = false;
};