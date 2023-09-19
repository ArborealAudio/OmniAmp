/**
 * MixedFeedback.h
 * Class for processing multi-channel feedback
 */

template <typename T, int channels> struct MixedFeedback
{
    void prepare(const dsp::ProcessSpec &spec)
    {
        SR = spec.sampleRate;

        double delaySamplesBase = delayMs * 0.001 * spec.sampleRate;

        for (int ch = 0; ch < channels; ++ch) {
            double r = ch * 1.0 / channels;
            delaySamples[ch] = std::pow(2.0, r) * delaySamplesBase;
            delays[ch].prepare(spec);
            delays[ch].setMaximumDelayInSamples(SR);

            osc[ch].initialise([](double x) { return std::sin(x); });
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

    /* update all internal parameters */
    void updateParams(const ReverbParams params)
    {
        delayMs = params.roomSizeMs;
        rt60 = params.rt60;
        double delaySamplesBase = delayMs * 0.001 * SR;
        for (int ch = 0; ch < channels; ++ch) {
            double r = ch * 1.0 / channels;
            delaySamples[ch] = std::pow(2.0, r) * delaySamplesBase;
        }
        double typicalLoopMs = params.roomSizeMs * 1.5;
        double loopsPerRt60 = params.rt60 / (typicalLoopMs * 0.001);
        double dbPerCycle = -60.0 / loopsPerRt60;
        decayGain = std::pow(10.0, dbPerCycle * 0.05);
        dampening = params.dampening;
        modFreq = params.modulation;
        changeModFreq();
    }

    /* update delay and rt60 times */
    void updateDelayAndDecay(float newDelay, float newRT60)
    {
        delayMs = newDelay;
        rt60 = newRT60;
        changeDelayAndDecay();
    }

    void changeDelayAndDecay()
    {
        double delaySamplesBase = delayMs * 0.001 * SR;
        for (int ch = 0; ch < channels; ++ch) {
            double r = ch * 1.0 / channels;
            delaySamples[ch] = std::pow(2.0, r) * delaySamplesBase;
        }
        double typicalLoopMs = delayMs * 1.5;
        double loopsPerRt60 = rt60 / (typicalLoopMs * 0.001);
        double dbPerCycle = -60.0 / loopsPerRt60;
        decayGain = std::pow(10.0, dbPerCycle * 0.05);
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

    template <typename Block> void process(Block &block)
    {
        for (int i = 0; i < block.getNumSamples(); ++i) {
            for (int ch = 0; ch < channels; ++ch) {
                auto mod = osc[ch].processSample(delaySamples[ch]);
                auto dtime = delaySamples[ch] - (0.2 * mod);
                auto d = delays[ch].popSample(0, dtime);
                d = lp.processSample(ch, d);
                d = hp.processSample(ch, d);
                delayed[ch] = d;
            }

            MixMatrix<channels>::processHouseholder(delayed.data());

            for (int ch = 0; ch < channels; ++ch) {
                auto in = block.getChannelPointer(ch)[i];
                auto sum = in + delayed[ch] * decayGain;
                delays[ch].pushSample(0, sum);

                block.setSample(ch, i, delayed[ch]);
            }
        }
    }

    template <typename Block> void processSmoothed(Block &block)
    {
        for (int i = 0; i < block.getNumSamples(); ++i) {
            changeDelayAndDecay();

            for (int ch = 0; ch < channels; ++ch) {
                auto mod = osc[ch].processSample(delaySamples[ch]);
                auto dtime = delaySamples[ch] - (0.2 * mod);
                auto d = delays[ch].popSample(0, dtime);
                d = lp.processSample(ch, d);
                d = hp.processSample(ch, d);
                delayed[ch] = d;
            }

            MixMatrix<channels>::processHouseholder(delayed.data());

            for (int ch = 0; ch < channels; ++ch) {
                auto in = block.getChannelPointer(ch)[i];
                auto sum = in + delayed[ch] * decayGain;
                delays[ch].pushSample(0, sum);

                block.setSample(ch, i, delayed[ch]);
            }
        }
    }

    T decayGain = 0.85;
    /*A fraction of Nyquist, where the lowpass will be placed in the feedback
     * path*/
    T dampening = 1.0;
    T modFreq = 1.0;

  private:
    T delayMs = 150.0;
    T rt60 = 0.0;
    std::atomic<bool> needUpdate = false;
    std::array<int, channels> delaySamples;
    std::array<dsp::DelayLine<T, dsp::DelayLineInterpolationTypes::Linear>,
               channels>
        delays;
    // container for N number of delayed & filtered samples
    std::array<T, channels> delayed;
    strix::SVTFilter<T> lp, hp;
    std::array<dsp::Oscillator<T>, channels> osc;
    double SR = 44100.0;
};