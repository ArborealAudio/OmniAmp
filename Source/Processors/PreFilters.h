/*
  ==============================================================================

    PreFilters.h
    Created: 25 Jan 2022 6:26:03pm
    Author:  alexb

  ==============================================================================
*/

#pragma once

template <typename T>
struct GuitarPreFilter : PreampProcessor
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
    }

    void changeFilters()
    {
        switch (type)
        {
        case GammaRay:
            bp_coeffs = dsp::IIR::Coefficients<double>::makeHighPass(SR, 350.f);
            hs_coeffs = dsp::IIR::Coefficients<double>::makeHighShelf(SR, 350.f, 0.7f, 4.f);
            lp_coeffs = dsp::IIR::Coefficients<double>::makeLowPass(SR, 6200.f);
            break;
        case Sunbeam:
            bp_coeffs = dsp::IIR::Coefficients<double>::makeHighPass(SR, 350.f); /*unused*/
            hs_coeffs = dsp::IIR::Coefficients<double>::makeHighShelf(SR, 750.f, 0.7f, 2.f);
            lp_coeffs = dsp::IIR::Coefficients<double>::makeFirstOrderLowPass(SR, 7200.f);
            break;
        case Moonbeam:
            bp_coeffs = dsp::IIR::Coefficients<double>::makeFirstOrderHighPass(SR, 150.f);
            hs_coeffs = dsp::IIR::Coefficients<double>::makeHighShelf(SR, 175.f, 0.666f, 4.f);
            lp_coeffs = dsp::IIR::Coefficients<double>::makeLowPass(SR, 6200.f);
            break;
        case XRay:
            bp_coeffs = dsp::IIR::Coefficients<double>::makeFirstOrderHighPass(SR, 350.f);
            hs_coeffs = dsp::IIR::Coefficients<double>::makeHighShelf(SR, 175.f, 0.7f, 3.f);
            lp_coeffs = dsp::IIR::Coefficients<double>::makeLowPass(SR, 5500.f);
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
    void process(strix::AudioBlock<vec> &block) override
    {
        if (*hiGain)
        {
            for (int ch = 0; ch < block.getNumChannels(); ++ch)
            {
                auto in = block.getChannelPointer(ch);
                for (int i = 0; i < block.getNumSamples(); ++i)
                {
                    in[i] = hiShelf[ch].processSample(in[i]);
                    in[i] = sc_lp[ch].processSample(in[i]);
                }
            }
        }
        else
        {
            for (int ch = 0; ch < block.getNumChannels(); ++ch)
            {
                auto in = block.getChannelPointer(ch);
                for (int i = 0; i < block.getNumSamples(); ++i)
                {
                    in[i] = bandPass[ch].processSample(in[i]);
                }
            }
        }
    }
#else
    void process(dsp::AudioBlock<double> &block) override
    {
        if (*hiGain)
        {
            for (int ch = 0; ch < block.getNumChannels(); ++ch)
            {
                auto in = block.getChannelPointer(ch);
                for (int i = 0; i < block.getNumSamples(); ++i)
                {
                    in[i] = hiShelf[ch].processSample(in[i]);
                    in[i] = sc_lp[ch].processSample(in[i]);
                }
            }
        }
        else
        {
            for (int ch = 0; ch < block.getNumChannels(); ++ch)
            {
                auto in = block.getChannelPointer(ch);
                for (int i = 0; i < block.getNumSamples(); ++i)
                {
                    in[i] = bandPass[ch].processSample(in[i]);
                }
            }
        }
    }
#endif

    strix::BoolParameter* hiGain = nullptr;
    GuitarMode type;

private:
    std::array<dsp::IIR::Filter<T>, 2> bandPass, hiShelf, sc_lp;
    dsp::IIR::Coefficients<double>::Ptr bp_coeffs, hs_coeffs, lp_coeffs;

    double SR = 0.0;
};