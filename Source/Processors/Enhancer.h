// Enhancer.h

#pragma once
#include "dsp_filters/dsp_filters.h"

template <typename Type>
struct HFEnhancer
{
    HFEnhancer(){}
    ~HFEnhancer(){}

    void prepare(const dsp::ProcessSpec& spec)
    {
        hp1.setup(1, spec.sampleRate, 7500.0);
        hp2.setup(2, spec.sampleRate, 7500.0);

        tube.prepare(spec);

        wetBuffer.setSize(spec.numChannels, spec.maximumBlockSize);
    }

    void reset()
    {
        hp1.reset();
        hp2.reset();
        tube.reset();
    }

    void processBuffer(AudioBuffer<Type>& buffer, const Type enhance)
    {
        wetBuffer.makeCopyOf(buffer, true);

        hp1.process(wetBuffer.getNumSamples(), wetBuffer.getArrayOfWritePointers());

        // subtract(buffer);

        wetBuffer.applyGain(jmap(enhance, 1.f, 4.f));

        tube.process(wetBuffer, 1.0, 0.5);

        hp2.process(wetBuffer.getNumSamples(), wetBuffer.getArrayOfWritePointers());

        buffer.addFrom(0, 0, wetBuffer.getReadPointer(0), wetBuffer.getNumSamples(), enhance);
        if (buffer.getNumChannels() > 1)
            buffer.addFrom(1, 0, wetBuffer.getReadPointer(1), wetBuffer.getNumSamples(), enhance);
    }

private:

    void subtract(AudioBuffer<Type>& from, AudioBuffer<Type>& sub)
    {
        auto lp = from.getArrayOfWritePointers();
        auto hp = sub.getArrayOfWritePointers();

        for (size_t i = 0; i < from.getNumSamples(); i++)
        {
            lp[0][i] -= hp[0][i];
            lp[1][i] -= hp[1][i];
        }
    }

    Dsp::SimpleFilter<Dsp::Bessel::HighPass<4>, 2> hp1, hp2;

    AudioBuffer<Type> wetBuffer;

    AVTriode tube;
};
