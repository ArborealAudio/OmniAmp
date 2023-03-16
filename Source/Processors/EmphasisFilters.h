/**
 * EmphasisFilters.h
 * for managing emphasis filters
 */

#include <JuceHeader.h>

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
    }

    void prepare(const dsp::ProcessSpec &spec)
    {
        SR = spec.sampleRate;
        float freq_ = *freq;
        if (freq_ >= SR * 0.5)
            freq_ = SR * 0.49;
        fIn.prepare(spec);
        fOut.prepare(spec);
        fIn.setCutoffFreq(freq_);
        fOut.setCutoffFreq(freq_);

        if (type == Low)
        {
            fIn.setType(strix::FilterType::firstOrderLowpass);
            fOut.setType(strix::FilterType::firstOrderLowpass);
            // fIn.coefficients = dsp::IIR::Coefficients<double>::makeLowShelf(SR, freq_, 0.707, amount_);
            // fOut.coefficients = dsp::IIR::Coefficients<double>::makeLowShelf(SR, freq_, 0.707, namount_);
        }
        else
        {
            fIn.setType(strix::FilterType::firstOrderHighpass);
            fOut.setType(strix::FilterType::firstOrderHighpass);
            // fIn.coefficients = dsp::IIR::Coefficients<double>::makeHighShelf(SR, freq_, 0.707, amount_);
            // fOut.coefficients = dsp::IIR::Coefficients<double>::makeHighShelf(SR, freq_, 0.707, namount_);
        }
    }

    void updateFilters()
    {
        float freq_ = *freq;
        fIn.setCutoffFreq(freq_);
        fOut.setCutoffFreq(freq_);
    }

    void reset()
    {
        fIn.reset();
        fOut.reset();
    }

    template <typename Block>
    void processIn(Block &block)
    {
        auto amtDB = amount->get();
        auto sign = amtDB >= 0.f;
        float amt = sign ? Decibels::decibelsToGain(amtDB) - 1.f : (Decibels::decibelsToGain(-amtDB) - 1.f) * -1.f;
        if (lastFreq != *freq)
            updateFilters();
        for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto *in = block.getChannelPointer(ch);
            for (size_t i = 0; i < block.getNumSamples(); ++i)
            {
                in[i] += amt * fIn.processSample(ch, in[i]);
            }
        }
    }

    template <typename Block>
    void processOut(Block &block)
    {
        auto amtDB = amount->get();
        auto sign = amtDB >= 0.f;
        float amt = sign ? (Decibels::decibelsToGain(-amtDB) - 1.f) * -1.f : Decibels::decibelsToGain(amtDB) - 1.f;
        for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto *in = block.getChannelPointer(ch);
            for (size_t i = 0; i < block.getNumSamples(); ++i)
            {
                in[i] += amt * fOut.processSample(ch, in[i]);
            }
        }
        lastFreq = *freq;
    }

    strix::FloatParameter *amount, *freq;

private:
    strix::SVTFilter<T> fIn, fOut;
    double SR = 44100.0;
    float lastFreq = 1.f;
};