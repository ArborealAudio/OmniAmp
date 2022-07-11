/*
  ==============================================================================

    PreFilters.h
    Created: 25 Jan 2022 6:26:03pm
    Author:  alexb

  ==============================================================================
*/

#pragma once

template <typename T>
struct GuitarPreFilter
{
    GuitarPreFilter() = default;

    void prepare(const dsp::ProcessSpec& spec)
    {
        lastSampleRate = spec.sampleRate;

        hs_coeffs = dsp::IIR::Coefficients<double>::makeHighShelf(spec.sampleRate, 350.f, 0.7f, 4.f);
        bp_coeffs = dsp::IIR::Coefficients<double>::makeHighPass(spec.sampleRate, 350.f);
        lp_coeffs = dsp::IIR::Coefficients<double>::makeLowPass(spec.sampleRate, 6200.f);

        for (auto& b : bandPass) {
            b.prepare(spec);
            b.coefficients = bp_coeffs;
        }
        for (auto& h : hiShelf) {
            h.prepare(spec);
            h.coefficients = hs_coeffs;
        }
        for (auto& l : sc_lp) {
            l.prepare(spec);
            l.coefficients = lp_coeffs;
        }
    }

    void reset()
    {
        for (auto& b : bandPass)
            b.reset();
        for (auto& b : hiShelf)
            b.reset();
        for (auto& b : sc_lp)
            b.reset();
    }

    void process(AudioBuffer<float>& buffer, bool hi)
    {
        if (hi) {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                auto in = buffer.getWritePointer(ch);
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                {
                    in[i] = hiShelf[ch].processSample(in[i]);
                    in[i] = sc_lp[ch].processSample(in[i]);
                }
            }
        }
        else {
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            {
                auto in = buffer.getWritePointer(ch);
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                {
                    in[i] = bandPass[ch].processSample(in[i]);
                }
            }
        }
    }

    template <typename Block>
    void processBlock(Block& block, bool hi)
    {
        if (hi) {
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
        else {
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

private:
    std::array<dsp::IIR::Filter<T>,2> bandPass, hiShelf, sc_lp;
    dsp::IIR::Coefficients<double>::Ptr bp_coeffs, hs_coeffs, lp_coeffs;

    double lastSampleRate = 0.0;
};