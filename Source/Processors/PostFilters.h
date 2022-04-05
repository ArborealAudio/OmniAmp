/*
  ==============================================================================

    PostFilters.h
    Created: 25 Jan 2022 7:50:08pm
    Author:  alexb

  ==============================================================================
*/

#pragma once

struct GuitarPostFilters
{
    GuitarPostFilters(){}

    void prepare(const dsp::ProcessSpec& spec)
    {
        SR = spec.sampleRate;

        hp.prepare(spec);
        peak1.prepare(spec);
        peak2.prepare(spec);
        peak3.prepare(spec);
        peak4.prepare(spec);
        lp.prepare(spec);

        *hp.coefficients = dsp::IIR::ArrayCoefficients<float>::makeHighPass(SR, 70.f);
        *peak1.coefficients = dsp::IIR::ArrayCoefficients<float>::makePeakFilter(SR, 191.f, 0.47f, 1.216);
        *peak2.coefficients = dsp::IIR::ArrayCoefficients<float>::makePeakFilter(SR, 607.f, 1.f, 0.432);
        *peak3.coefficients = dsp::IIR::ArrayCoefficients<float>::makePeakFilter(SR, 1466.f, 0.6f, 1.38);
        *peak4.coefficients = dsp::IIR::ArrayCoefficients<float>::makePeakFilter(SR, 4000.f, 1.f, 1.778);
        *lp.coefficients = dsp::IIR::ArrayCoefficients<float>::makeLowPass(SR, 8000.f);
    }

    void updateLowPass(bool hi)
    {
        if (hi)
            *lp.coefficients = dsp::IIR::ArrayCoefficients<float>::makeLowPass(SR, 10000.f);
        else
            *lp.coefficients = dsp::IIR::ArrayCoefficients<float>::makeLowPass(SR, 8000.f);
    }

    void process(AudioBuffer<float>& buffer, bool hi)
    {
        auto* in = buffer.getWritePointer(0);

        for (int i = 0; i < buffer.getNumSamples(); ++i)
        {
            in[i] = hp.processSample(in[i]);
            in[i] = peak1.processSample(in[i]);
            in[i] = peak2.processSample(in[i]);
            in[i] = peak3.processSample(in[i]);
            if (hi)
                in[i] = peak4.processSample(in[i]);
            in[i] = lp.processSample(in[i]);
        }

        buffer.copyFrom(1, 0, in, buffer.getNumSamples());
    }

private:

    dsp::IIR::Filter<float> hp, peak1, peak2, peak3, peak4, lp;

    double SR = 0.0;
};