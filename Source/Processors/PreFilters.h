/*
  ==============================================================================

    PreFilters.h
    Created: 25 Jan 2022 6:26:03pm
    Author:  alexb

  ==============================================================================
*/

#pragma once

struct GuitarPreFilter
{
    GuitarPreFilter(){}

    void prepare(const dsp::ProcessSpec& spec)
    {
        lastSampleRate = spec.sampleRate;

        for (auto& b : bandPass) {
            b.prepare(spec);
            //*b.coefficients = dsp::IIR::ArrayCoefficients<float>::makeBandPass(spec.sampleRate, 1300.f, 0.15f);
            *b.coefficients = dsp::IIR::ArrayCoefficients<float>::makeHighPass(spec.sampleRate, 350.f);
        }
        for (auto& h : hiShelf) {
            h.prepare(spec);
            *h.coefficients = dsp::IIR::ArrayCoefficients<float>::makeHighShelf(spec.sampleRate, 350.f, 0.7f, 4.f);
        }
        for (auto& l : sc_lp) {
            l.prepare(spec);
            *l.coefficients = dsp::IIR::ArrayCoefficients<float>::makeLowPass(spec.sampleRate, 6200.f);
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

private:
    std::array<dsp::IIR::Filter<float>,2> bandPass, hiShelf, sc_lp;

    double lastSampleRate = 0.0;
};