// Cab.h

#pragma once

#define FILTER_ORDER 50;

struct CabTest
{
    CabTest()
    {
        Random rand;

        for (int i = 0; i < 50; ++i)
        {
            auto plus = rand.nextInt(1);
            d0[i] = rand.nextFloat() * rand.nextFloat() + plus;
            d1[i] = rand.nextFloat() * rand.nextFloat() - plus;
            a1[i] = rand.nextFloat() * rand.nextFloat() + plus;
            a2[i] = rand.nextFloat() * rand.nextFloat() - plus;
        }
    }
    ~CabTest(){}

    void processBlock(dsp::AudioBlock<float>& block)
    {
        for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
        {
            auto x = block.getChannelPointer(ch);

            for (size_t i = 0; i < block.getNumSamples(); i++)
            {
                float out = 0.f;
                for (int j = 0; j < 50; ++j)
                {
                    auto p1 = d0[j] * (1.f / (1.f + (a1[j] * z1[ch]) + (a2[j] * z2[ch])));
                    auto p2 = p1 * z1[ch] * d1[j];

                    out += p1 + p2;
                }

                x[i] = out / 50.f;

                z2[ch] = z1[ch];
                z1[ch] = x[i];
            }
        }
    }

private:
    float a1[50] {0.f}, a2[50]{0.f};
    float z1[2] {0.f}, z2[2] {0.f};
    float d0[50]{0.f}, d1[50]{0.f};
};

struct OtherCab
{
    OtherCab() : convo(dsp::Convolution::Latency{0})
    {}
    ~OtherCab(){}

    void prepare(const dsp::ProcessSpec& spec)
    {
        File ir{"/Users/Shared/Audio Assault/Shibalba EX/IRs/SC-BonUbrk412-BBM75-49-Bright.wav"};
        convo.loadImpulseResponse(ir, dsp::Convolution::Stereo::no, dsp::Convolution::Trim::no, (size_t)48000);
        convo.prepare(spec);
    }

    void processBlock(dsp::AudioBlock<float>& block)
    {
        convo.process(dsp::ProcessContextReplacing<float>(block));
    }

private:
    dsp::Convolution convo;
};
