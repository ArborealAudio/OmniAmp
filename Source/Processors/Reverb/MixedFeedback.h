/**
 * MixedFeedback.h
 * Class for processing multi-channel feedback
*/

template <typename T, int channels>
struct MixedFeedback
{
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