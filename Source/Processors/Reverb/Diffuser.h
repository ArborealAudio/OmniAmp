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