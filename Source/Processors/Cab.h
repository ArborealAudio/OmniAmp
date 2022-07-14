// Cab.h

#pragma once

#define FILTER_ORDER 50

// struct CabTest
// {
//     CabTest()
//     {
//         Random rand;

//         for (int i = 0; i < FILTER_ORDER; ++i)
//         {
//             auto plus = rand.nextInt(1);
//             d0[i] = rand.nextFloat() * rand.nextFloat() + plus;
//             d1[i] = rand.nextFloat() * rand.nextFloat() - plus;
//             a1[i] = rand.nextFloat() * rand.nextFloat() + plus;
//             a2[i] = rand.nextFloat() * rand.nextFloat() - plus;
//         }
//     }

//     void processBlock(dsp::AudioBlock<float>& block)
//     {
//         for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
//         {
//             auto x = block.getChannelPointer(ch);

//             for (size_t i = 0; i < block.getNumSamples(); i++)
//             {
//                 float out = 0.f;
//                 for (int j = 0; j < FILTER_ORDER; ++j)
//                 {
//                     auto p1 = d0[j] * (1.f / (1.f + (a1[j] * z1[ch]) + (a2[j] * z2[ch])));
//                     auto p2 = p1 * z1[ch] * d1[j];

//                     out += p1 + p2;
//                 }

//                 x[i] = out / (float)FILTER_ORDER;

//                 z2[ch] = z1[ch];
//                 z1[ch] = x[i];
//             }
//         }
//     }

// private:
//     float a1[FILTER_ORDER] {0.f}, a2[FILTER_ORDER]{0.f};
//     float z1[2] {0.f}, z2[2] {0.f};
//     float d0[FILTER_ORDER]{0.f}, d1[FILTER_ORDER]{0.f};
// };

template <typename T>
struct ConvoCab
{
    ConvoCab() : convo(dsp::Convolution::Latency{0})
    {
    }

    void prepare(const dsp::ProcessSpec &spec)
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

    void processBlock(dsp::AudioBlock<T> &block)
    {
        convo.process(dsp::ProcessContextReplacing<T>(block));
    }

private:
    dsp::Convolution convo;
};

class FDNCab
{
    struct FDN
    {
        FDN() = default;
    #ifndef ORDER
        #define ORDER 4
    #endif //  ORDER

        void prepare(const dsp::ProcessSpec &spec)
        {
            for (auto &d : delay) {
                d.prepare(spec);
                d.setMaximumDelayInSamples(44100);
            }

            double i = 1.0;
            for (auto &f : lp) {
                f.prepare(spec);
                f.setType(dsp::StateVariableTPTFilterType::lowpass);
                f.setCutoffFrequency(10000.0 / i);
                f.setResonance(0.7);
                ++i;
            }

            i = 1.0;
            for (auto &f : hp)
            {
                f.prepare(spec);
                f.setType(dsp::StateVariableTPTFilterType::highpass);
                f.setCutoffFrequency(20.0 * i);
                f.setResonance(1.0);
                ++i;
            }
        }

        void reset()
        {
            for (auto& d : delay)
                d.reset();
            for (auto& f : lp)
                f.reset();
            for (auto& f : hp)
                f.reset();
            for (auto& f : fdbk)
                f = 0.0;
            for (auto& ch : z)
                for (auto& s : ch)
                    s = 0.0;
        }

        void processBlock(dsp::AudioBlock<double>& block)
        {
            auto left = block.getChannelPointer(0);
            auto right = block.getChannelPointer(1);

            for (auto i = 0; i < block.getNumSamples(); ++i)
            {
                double out_l, out_r;

                for (auto n = 0; n < ORDER; ++n)
                {
                    fdbk[n] = 0.3 / (double)(n + 1.0);

                    double dtime = fma((double)n, 100.0, 2.3 * (n + 1.0));
                    auto d_l = delay[n].popSample(0, dtime);
                    auto d_r = delay[n].popSample(1, dtime);

                    out_l = d_l + left[i] * -fdbk[n];
                    out_r = d_r + right[i] * -fdbk[n];

                    z[0][n] = out_l;
                    z[1][n] = out_r;

                    auto f_l = z[0][n] * fdbk[n] + left[i];
                    auto f_r = z[1][n] * fdbk[n] + right[i];

                    f_l = hp[n].processSample(0, f_l);
                    f_r = hp[n].processSample(1, f_r);

                    f_l = lp[n].processSample(0, f_l);
                    f_r = lp[n].processSample(1, f_r);

                    delay[n].pushSample(0, f_l);
                    delay[n].pushSample(1, f_r);
                }

                left[i] = out_l;
                right[i] = out_r;
            }
        }

    private:
        dsp::DelayLine<double> delay[ORDER];

        double fdbk[ORDER]{0.0};
        double z[2][ORDER]{0.0};

        dsp::StateVariableTPTFilter<double> lp[ORDER], hp[ORDER];
    };

    dsp::StateVariableTPTFilter<double> hp;
    dsp::StateVariableTPTFilter<double> lp1, lp2;

    FDN fdn;

public:
    FDNCab() = default;

    void prepare(const dsp::ProcessSpec &spec)
    {
        hp.prepare(spec);
        hp.setType(dsp::StateVariableTPTFilterType::highpass);
        hp.setCutoffFrequency(90.0);
        hp.setResonance(2.0);

        lp1.prepare(spec);
        lp2.prepare(spec);
        lp1.setType(dsp::StateVariableTPTFilterType::lowpass);
        lp2.setType(dsp::StateVariableTPTFilterType::lowpass);
        lp1.setCutoffFrequency(3500.0);
        lp2.setCutoffFrequency(1500.0);
        lp1.setResonance(2.0);
        lp2.setResonance(0.7);

        fdn.prepare(spec);
    }

    void reset()
    {
        fdn.reset();
        hp.reset();
        lp1.reset();
        lp2.reset();
    }

    void processBlock(dsp::AudioBlock<double> &block)
    {
        fdn.processBlock(block);
        hp.process(dsp::ProcessContextReplacing<double>(block));
        lp1.process(dsp::ProcessContextReplacing<double>(block));
        lp2.process(dsp::ProcessContextReplacing<double>(block));
    }
};
