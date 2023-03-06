/**
 * EmphasisFilters.h
 * for managing emphasis filters
 */

enum EmphasisFilterType
{
    Low,
    High
};

template <typename T, EmphasisFilterType type = Low>
struct EmphasisFilter
{
    EmphasisFilter(strix::FloatParameter *_amount, strix::FloatParameter *_freq) : amount(_amount), freq(_freq)
    {
        lastFreq = *freq;
    }

    void prepare(const dsp::ProcessSpec &spec)
    {
        SR = spec.sampleRate;
        float freq_ = *freq;
        if (*freq >= SR * 0.5)
            freq_ = SR * 0.49;
        float amount_ = Decibels::decibelsToGain(amount->get());
        float namount_ = Decibels::decibelsToGain(-amount->get());
        for (size_t i = 0; i < 2; ++i)
        {
            fIn[i].prepare(spec);
            fOut[i].prepare(spec);
            if (type == Low)
            {
                fIn[i].coefficients = dsp::IIR::Coefficients<double>::makeLowShelf(SR, freq_, 0.707, amount_);
                fOut[i].coefficients = dsp::IIR::Coefficients<double>::makeLowShelf(SR, freq_, 0.707, namount_);
            }
            else
            {
                fIn[i].coefficients = dsp::IIR::Coefficients<double>::makeHighShelf(SR, freq_, 0.707, amount_);
                fOut[i].coefficients = dsp::IIR::Coefficients<double>::makeHighShelf(SR, freq_, 0.707, namount_);
            }
        }
    }

    void updateFilters()
    {
        float amount_ = Decibels::decibelsToGain(amount->get());
        float namount_ = Decibels::decibelsToGain(-amount->get());
        float freq_ = *freq;
        for (size_t i = 0; i < 2; ++i)
        {
            if (type == Low)
            {
                *fIn[i].coefficients = dsp::IIR::ArrayCoefficients<double>::makeLowShelf(SR, freq_, 0.707, amount_);
                *fOut[i].coefficients = dsp::IIR::ArrayCoefficients<double>::makeLowShelf(SR, freq_, 0.707, namount_);
            }
            else
            {
                *fIn[i].coefficients = dsp::IIR::ArrayCoefficients<double>::makeHighShelf(SR, freq_, 0.707, amount_);
                *fOut[i].coefficients = dsp::IIR::ArrayCoefficients<double>::makeHighShelf(SR, freq_, 0.707, namount_);
            }
        }
    }

    void reset()
    {
        fIn[0].reset();
        fIn[1].reset();
        fOut[0].reset();
        fOut[1].reset();
    }

    template <typename Block>
    void processIn(Block &block)
    {
        if (lastFreq != *freq || lastAmount != *amount)
            updateFilters();
        for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto *in = block.getChannelPointer(ch);
            for (size_t i = 0; i < block.getNumSamples(); ++i)
            {
                in[i] = fIn[ch].processSample(in[i]);
            }
        }
    }

    template <typename Block>
    void processOut(Block &block)
    {
        for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto *in = block.getChannelPointer(ch);
            for (size_t i = 0; i < block.getNumSamples(); ++i)
            {
                in[i] = fOut[ch].processSample(in[i]);
            }
        }
        lastFreq = *freq;
        lastAmount = *amount;
    }

    strix::FloatParameter *amount, *freq;

private:
    dsp::IIR::Filter<T> fIn[2], fOut [2];
    double SR = 44100.0;
    float lastFreq = 1.f, lastAmount = 1.f;
};