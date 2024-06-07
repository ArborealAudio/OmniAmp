/*
    PreFilters.h
*/

#pragma once

template <typename T> struct GuitarPreFilter : PreampProcessor
{
    GuitarPreFilter() = default;

    void prepare(const dsp::ProcessSpec &spec)
    {
        SR = spec.sampleRate;

        changeFilters();

        for (auto &b : bandPass)
            b.prepare(spec);
        for (auto &h : hiShelf)
            h.prepare(spec);
        for (auto &l : sc_lp)
            l.prepare(spec);

        dynHP.prepare(spec);
        dynHP.setType(strix::FilterType::firstOrderHighpass);
    }

    void changeFilters()
    {
        switch (type) {
        case GammaRay:
            bp_coeffs = dsp::IIR::Coefficients<double>::makeHighPass(SR, 350.f);
            hs_coeffs = dsp::IIR::Coefficients<double>::makeHighShelf(
                SR, 350.f, 0.7f, 4.f);
            lp_coeffs = dsp::IIR::Coefficients<double>::makeLowPass(
                SR, 6200.f > SR * 0.5 ? SR * 0.5 : 6200.f);
            dynHP.setCutoffFreq(350.0);
            break;
        case Sunbeam:
            bp_coeffs = dsp::IIR::Coefficients<double>::makeHighPass(
                SR, 350.f); /*unused*/
            hs_coeffs = dsp::IIR::Coefficients<double>::makeHighShelf(
                SR, 750.f, 0.7f, 2.f);
            lp_coeffs = dsp::IIR::Coefficients<double>::makeFirstOrderLowPass(
                SR, 7200.f > SR * 0.5 ? SR * 0.5 : 7200.f);
            dynHP.setCutoffFreq(1200.0);
            break;
        case Moonbeam:
            bp_coeffs = dsp::IIR::Coefficients<double>::makeFirstOrderHighPass(
                SR, 150.f);
            hs_coeffs = dsp::IIR::Coefficients<double>::makeHighShelf(
                SR, 175.f, 0.666f, 4.f);
            lp_coeffs = dsp::IIR::Coefficients<double>::makeLowPass(
                SR, 7500.f > SR * 0.5 ? SR * 0.5 : 7500.f);
            dynHP.setCutoffFreq(350.0);
            break;
        case XRay:
            bp_coeffs = dsp::IIR::Coefficients<double>::makeHighPass(SR, 450.f);
            hs_coeffs = dsp::IIR::Coefficients<double>::makeHighShelf(
                SR, 350.f, 0.7f, 3.f);
            lp_coeffs = dsp::IIR::Coefficients<double>::makeLowPass(SR, 5500.f);
            dynHP.setCutoffFreq(500.0);
            break;
        }

        for (auto &b : bandPass)
            b.coefficients = bp_coeffs;
        for (auto &h : hiShelf)
            h.coefficients = hs_coeffs;
        for (auto &l : sc_lp)
            l.coefficients = lp_coeffs;
    }

    void reset()
    {
        for (auto &b : bandPass)
            b.reset();
        for (auto &b : hiShelf)
            b.reset();
        for (auto &b : sc_lp)
            b.reset();
    }

#if USE_SIMD
    void process(strix::AudioBlock<vec> &block)
    {
        auto dynHPGain = 1.f / jmax(inGain, 1.f);
        if (*hiGain) {
            for (int ch = 0; ch < block.getNumChannels(); ++ch) {
                auto in = block.getChannelPointer(ch);
                for (int i = 0; i < block.getNumSamples(); ++i) {
                    in[i] = hiShelf[ch].processSample(in[i]);
                    in[i] = sc_lp[ch].processSample(in[i]);
                    in[i] += dynHPGain * dynHP.processSample(ch, in[i]);
                }
            }
        } else {
            for (int ch = 0; ch < block.getNumChannels(); ++ch) {
                auto in = block.getChannelPointer(ch);
                for (int i = 0; i < block.getNumSamples(); ++i) {
                    in[i] = bandPass[ch].processSample(in[i]);
                    in[i] += dynHPGain * dynHP.processSample(ch, in[i]);
                }
            }
        }
    }
#else
    void process(dsp::AudioBlock<double> &block)
    {
        auto dynHPGain = 1.f / jmax(inGain, 1.f);
        if (*hiGain) {
            for (int ch = 0; ch < block.getNumChannels(); ++ch) {
                auto in = block.getChannelPointer(ch);
                for (int i = 0; i < block.getNumSamples(); ++i) {
                    in[i] = hiShelf[ch].processSample(in[i]);
                    in[i] = sc_lp[ch].processSample(in[i]);
                    in[i] += dynHPGain * dynHP.processSample(ch, in[i]);
                }
            }
        } else {
            for (int ch = 0; ch < block.getNumChannels(); ++ch) {
                auto in = block.getChannelPointer(ch);
                for (int i = 0; i < block.getNumSamples(); ++i) {
                    in[i] = bandPass[ch].processSample(in[i]);
                    in[i] += dynHPGain * dynHP.processSample(ch, in[i]);
                }
            }
        }
    }
#endif

    strix::BoolParameter *hiGain = nullptr;
    GuitarMode type;
    // the input gain parameter, used for dynHP
    float inGain = 1.f;

  private:
    std::array<dsp::IIR::Filter<T>, 2> bandPass, hiShelf, sc_lp;
    dsp::IIR::Coefficients<double>::Ptr bp_coeffs, hs_coeffs, lp_coeffs;
    strix::SVTFilter<T> dynHP;

    double SR = 44100.0;
};

template <typename T> struct BassPreFilter : PreampProcessor
{
    BassPreFilter() = default;

    void prepare(const dsp::ProcessSpec &spec)
    {
        SR = spec.sampleRate;
        filter.resize(spec.numChannels);
        for (auto &f : filter)
            f.prepare(spec);
        dynHP.prepare(spec);
        dynHP.setType(strix::FilterType::firstOrderHighpass);
        changeFilters();
    }

    void reset()
    {
        for (auto &f : filter)
            f.reset();
    }

    void changeFilters()
    {
        switch (type) {
        case Cobalt:
            for (auto &f : filter)
                *f.coefficients =
                    dsp::IIR::ArrayCoefficients<double>::makePeakFilter(
                        SR, 950.0, 0.7, 0.3);
            dynHP.setCutoffFreq(600.0);
            break;
        case Emerald:
            for (auto &f : filter)
                *f.coefficients =
                    dsp::IIR::ArrayCoefficients<double>::makePeakFilter(
                        SR, 1000.0, 0.7, 0.5);
            dynHP.setCutoffFreq(900.0);
            break;
        case Quartz:
            for (auto &f : filter)
                *f.coefficients =
                    dsp::IIR::ArrayCoefficients<double>::makePeakFilter(
                        SR, 1500.0, 0.5, 1.2);
            dynHP.setCutoffFreq(1200.0);
            break;
        }
    }
#if USE_SIMD
    void process(strix::AudioBlock<vec> &block) override
    {
        float dynHPGain = 1.f / jmax(inGain, 1.f);
        for (size_t ch = 0; ch < block.getNumChannels(); ++ch) {
            auto in = block.getChannelPointer(ch);
            for (size_t i = 0; i < block.getNumSamples(); ++i) {
                in[i] = filter[ch].processSample(in[i]);
                in[i] += dynHPGain * dynHP.processSample(ch, in[i]);
            }
        }
    }
#else
    void process(dsp::AudioBlock<double> &block) override
    {
        float dynHPGain = 1.f / jmax(inGain, 1.f);
        for (size_t ch = 0; ch < block.getNumChannels(); ++ch) {
            auto in = block.getChannelPointer(ch);
            for (size_t i = 0; i < block.getNumSamples(); ++i) {
                in[i] = filter[ch].processSample(in[i]);
                in[i] += dynHPGain * dynHP.processSample(ch, in[i]);
            }
        }
    }
#endif
    strix::BoolParameter *hiGain = nullptr;
    float inGain = 1.f;
    BassMode type;

  private:
    std::vector<dsp::IIR::Filter<T>> filter;
    strix::SVTFilter<T> dynHP;

    double SR = 44100.0;
};
