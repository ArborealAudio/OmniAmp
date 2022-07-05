// Cab.h

#pragma once

#define FILTER_ORDER 50

struct CabTest
{
    CabTest()
    {
        Random rand;

        for (int i = 0; i < FILTER_ORDER; ++i)
        {
            auto plus = rand.nextInt(1);
            d0[i] = rand.nextFloat() * rand.nextFloat() + plus;
            d1[i] = rand.nextFloat() * rand.nextFloat() - plus;
            a1[i] = rand.nextFloat() * rand.nextFloat() + plus;
            a2[i] = rand.nextFloat() * rand.nextFloat() - plus;
        }
    }

    void processBlock(dsp::AudioBlock<float>& block)
    {
        for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto x = block.getChannelPointer(ch);

            for (size_t i = 0; i < block.getNumSamples(); i++)
            {
                float out = 0.f;
                for (int j = 0; j < FILTER_ORDER; ++j)
                {
                    auto p1 = d0[j] * (1.f / (1.f + (a1[j] * z1[ch]) + (a2[j] * z2[ch])));
                    auto p2 = p1 * z1[ch] * d1[j];

                    out += p1 + p2;
                }

                x[i] = out / (float)FILTER_ORDER;

                z2[ch] = z1[ch];
                z1[ch] = x[i];
            }
        }
    }

private:
    float a1[FILTER_ORDER] {0.f}, a2[FILTER_ORDER]{0.f};
    float z1[2] {0.f}, z2[2] {0.f};
    float d0[FILTER_ORDER]{0.f}, d1[FILTER_ORDER]{0.f};
};

struct ConvoCab
{
    ConvoCab() : convo(dsp::Convolution::Latency{0})
    {}

    void prepare(const dsp::ProcessSpec& spec)
    {
        convo.prepare(spec);
    }

    void reset()
    {
        convo.reset();
    }

    void changeIR(const ProcessorType newType)
    {
        switch (newType)
        {
        case ProcessorType::Guitar:
            convo.loadImpulseResponse(BinaryData::Fender_Bassman_SM57_wav, BinaryData::Fender_Bassman_SM57_wavSize, dsp::Convolution::Stereo::no, dsp::Convolution::Trim::yes, 44100);
            break;
        case ProcessorType::Bass:
            convo.loadImpulseResponse(BinaryData::grafton_bass_deluxe_wav, BinaryData::grafton_bass_deluxe_wavSize, dsp::Convolution::Stereo::yes, dsp::Convolution::Trim::no, 0);
            break;
        case ProcessorType::Channel:
            break;
        }
    }

    void processBlock(dsp::AudioBlock<float>& block)
    {
        convo.process(dsp::ProcessContextReplacing<float>(block));
    }

private:
    dsp::Convolution convo;
};
