/*
  ==============================================================================

    Processors.h
    Created: 11 Apr 2022 10:20:37am
    Author:  alexb

  ==============================================================================
*/

#pragma once

namespace Processors
{
    enum class ProcessorType
    {
        Guitar,
        Bass,
        Channel
    };

    enum GuitarMode
    {
        GammaRay,
        Sunbeam,
        Moonbeam
    };

    /**
     * Generic preamp processor
     */
    struct PreampProcessor
    {
#if USE_SIMD
        virtual void process(strix::AudioBlock<vec> &block) = 0;
#else
        virtual void process(dsp::AudioBlock<double> &block) = 0;
#endif
    };

    typedef struct
    {
        double first = 1.0;
        double second = 1.0;
    } bias_t;

#include "Tube.h"
#include "PreFilters.h"
#include "ToneStack.h"
#include "Comp.h"
#include "Enhancer.h"
#include "Cab.h"
#include "DistPlus.h"
#include "Room.h"

    /**
     * A struct which contains a vector of processors and calls their respective process methods
     */
    struct Preamp
    {
        std::vector<PreampProcessor *> procs;
#if USE_SIMD
        void process(strix::AudioBlock<vec> &block)
        {
            for (auto p : procs)
                p->process(block);
        }
#else
        void process(dsp::AudioBlock<double> &block)
        {
            for (auto p : procs)
                p->process(block);
        }
#endif
    };

    /**
     * @brief Generic processor class
     * @param t Type of processor to init
     * @param s Source for gain reduction meter
     */
    struct Processor : AudioProcessorValueTreeState::Listener
    {
        Processor(AudioProcessorValueTreeState &a, ProcessorType t, strix::VolumeMeterSource &s) : apvts(a), comp(t, s, a.getRawParameterValue("compPos"))
        {
            inGain = dynamic_cast<strix::FloatParameter *>(apvts.getParameter("preampGain"));
            lastInGain = *inGain;
            inGainAuto = dynamic_cast<strix::BoolParameter *>(apvts.getParameter("preampAutoGain"));
            outGain = dynamic_cast<strix::FloatParameter *>(apvts.getParameter("powerampGain"));
            lastOutGain = *outGain;
            outGainAuto = dynamic_cast<strix::BoolParameter *>(apvts.getParameter("powerampAutoGain"));
            eqAutoGain = dynamic_cast<strix::BoolParameter *>(apvts.getParameter("eqAutoGain"));
            p_comp = dynamic_cast<strix::FloatParameter *>(apvts.getParameter("comp"));
            linked = dynamic_cast<strix::BoolParameter *>(apvts.getParameter("compLink"));
            hiGain = dynamic_cast<strix::BoolParameter *>(apvts.getParameter("hiGain"));
            dist = dynamic_cast<strix::FloatParameter *>(apvts.getParameter("dist"));

            apvts.addParameterListener("comp", this);
            apvts.addParameterListener("compPos", this);
        }

        ~Processor()
        {
            apvts.removeParameterListener("comp", this);
            apvts.removeParameterListener("compPos", this);
        }

        void parameterChanged(const String &parameterID, float newValue) override
        {
            if (parameterID == "comp" || parameterID == "compPos")
                comp.setComp(*p_comp);
        }

        virtual void prepare(const dsp::ProcessSpec &spec) = 0;

        virtual void reset()
        {
            comp.reset();

            for (auto &t : triode)
                t.reset();

            if (toneStack != nullptr)
                toneStack->reset();

            pentode->reset();
        }

        virtual void setDistParam(float newValue)
        {
            mxr.setParams(1.f - newValue);
        }

        /*0 = bass | 1 = mid | 2 = treble*/
        virtual void setToneControl(int control, float newValue)
        {
            if (toneStack == nullptr)
                return;

            switch (control)
            {
            case 0:
                toneStack->setBass(newValue);
                break;
            case 1:
                toneStack->setMid(newValue);
                break;
            case 2:
                toneStack->setTreble(newValue);
                break;
            }
        }

        virtual strix::VolumeMeterSource &getActiveGRSource() { return comp.getGRSource(); }

    protected:
        void defaultPrepare(const dsp::ProcessSpec &spec)
        {
            SR = spec.sampleRate;
            numSamples = spec.maximumBlockSize;
            numChannels = spec.numChannels;

            comp.prepare(spec);

            mxr.prepare(spec);

            for (auto &t : triode)
                t.prepare(spec);

            if (toneStack != nullptr)
                toneStack->prepare(spec);

            pentode->prepare(spec);

            simd.setInterleavedBlockSize(spec.numChannels, spec.maximumBlockSize);
        }

        AudioProcessorValueTreeState &apvts;

        OptoComp<double> comp;
#if USE_SIMD
        MXRDistWDF<vec> mxr;
        std::unique_ptr<ToneStackNodal<vec>> toneStack;
        std::vector<AVTriode<vec>> triode;
        std::unique_ptr<Preamp> preamp;
        std::unique_ptr<Pentode<vec>> pentode;
#else
        MXRDistWDF<double> mxr;
        std::unique_ptr<ToneStackNodal<double>> toneStack;
        std::vector<AVTriode<double>> triode;
        std::unique_ptr<Preamp> preamp;
        std::unique_ptr<Pentode<double>> pentode;
#endif

        // std::atomic<float> *inGain, *inGainAuto, *outGain, *outGainAuto, *hiGain, *p_comp, *linked, *dist, *eqAutoGain;
        strix::FloatParameter *inGain, *outGain, *p_comp, *dist;
        strix::BoolParameter *inGainAuto, *outGainAuto, *hiGain, *linked, *eqAutoGain;
        double lastInGain, lastOutGain;

        strix::SIMD<double, dsp::AudioBlock<double>, strix::AudioBlock<vec>> simd;

        double SR = 44100.0;
        int numSamples = 0, numChannels = 0;
    };

    struct Guitar : Processor,
                    private AudioProcessorValueTreeState::Listener
    {
        Guitar(AudioProcessorValueTreeState &a, strix::VolumeMeterSource &s) : Processor(a, ProcessorType::Guitar, s)
        {
#if USE_SIMD
            toneStack = std::make_unique<ToneStackNodal<vec>>(eqCoeffs);
            gtrPre = std::make_unique<GuitarPreFilter<vec>>(currentType);
            pentode = std::make_unique<Pentode<vec>>();
#else
            toneStack = std::make_unique<ToneStackNodal<double>>(eqCoeffs);
            gtrPre = std::make_unique<GuitarPreFilter<double>>(currentType);
            pentode = std::make_unique<Pentode<double>>();
#endif
            guitarMode = dynamic_cast<strix::ChoiceParameter *>(apvts.getParameter("guitarMode"));
            apvts.addParameterListener("guitarMode", this);
            gtrPre->hiGain = hiGain;
            preamp = std::make_unique<Preamp>();
            triode.resize(4);
            bias.resize(4);
        }

        ~Guitar()
        {
            apvts.removeParameterListener("guitarMode", this);
        }

        void prepare(const dsp::ProcessSpec &spec) override
        {
            defaultPrepare(spec);
            gtrPre->prepare(spec);
            setBias();
            setPreamp();
        }

        void parameterChanged(const String& parameterID, float newValue) override
        {
            if (parameterID == "guitarMode")
            {
                currentType = static_cast<GuitarMode>(guitarMode->getIndex());
                setBias();
            }
        }

        void setPreamp()
        {
            if (!preamp->procs.empty())
                return;
            preamp->procs.push_back(&triode[0]);
            preamp->procs.push_back(gtrPre.get());
            preamp->procs.push_back(&triode[1]);
            preamp->procs.push_back(&triode[2]);
            preamp->procs.push_back(toneStack.get());
            preamp->procs.push_back(&triode[3]);
        }

        void setBias()
        {
            switch (currentType)
            {
            case GammaRay:
                assert(bias.size() == (size_t)4);
                bias[0].first = 0.5;
                bias[0].second = 1.0;
                bias[1].first = 1.4;
                bias[1].second = 2.4;
                bias[2].first = 1.4;
                bias[2].second = 2.4;
                bias[3].first = 2.0;
                bias[3].second = 4.0;
                break;
            case Sunbeam:
                assert(bias.size() == (size_t)3);
                bias[0].first = 0.5;
                bias[0].second = 0.5;
                bias[1].first = 0.5;
                bias[1].second = 0.5;
                bias[2].first = 2.0;
                bias[2].second = 2.0;
                break;
            case Moonbeam:
                assert(bias.size() == (size_t)4);
                bias[0].first = 2.0;
                bias[0].second = 2.5;
                bias[1].first = 3.0;
                bias[1].second = 4.0;
                bias[2].first = 3.0;
                bias[2].second = 4.0;
                bias[3].first = 4.0;
                bias[3].second = 5.0;
                break;
            }

            for (size_t i = 0; i < triode.size(); ++i)
                triode[i].bias = bias[i];
        }

        template <typename T>
        void processBlock(dsp::AudioBlock<T> &block)
        {
            T gain_raw = jmap(inGain->get(), 1.f, 8.f);
            T out_raw = jmap(outGain->get(), 1.f, 8.f);

            T autoGain = 1.0;

            if (!*apvts.getRawParameterValue("compPos"))
                comp.processBlock(block, *p_comp, *linked);

#if USE_SIMD
            auto simdBlock = simd.interleaveBlock(block);
            auto &&processBlock = simdBlock;
#else
            auto &&processBlock = block;
#endif
            if (*dist > 0.f)
                mxr.processBlock(processBlock);

            processBlock.multiplyBy(gain_raw);

            preamp->process(processBlock);

            processBlock.multiplyBy(out_raw * 6.0);

            pentode->processBlockClassB(processBlock, 0.6, 0.6);

            if (*outGainAuto)
                autoGain *= 1.0 / out_raw;

            processBlock.multiplyBy(autoGain);

#if USE_SIMD
            simd.deinterleaveBlock(processBlock);
#endif

            if (*apvts.getRawParameterValue("compPos"))
                comp.processBlock(block, *p_comp, *linked);
        }

        GuitarMode currentType = GammaRay;

    private:
        std::vector<bias_t> bias;

        strix::ChoiceParameter *guitarMode;

#if USE_SIMD
        std::unique_ptr<GuitarPreFilter<vec>> gtrPre;
        ToneStackNodal<vec>::Coeffs eqCoeffs{(vec)0.25e-9, (vec)25e-9, (vec)22e-9, (vec)300e3, (vec)1e6, (vec)20e3, (vec)65e3};
#else
        std::unique_ptr<GuitarPreFilter<double>> gtrPre;
        ToneStackNodal<double>::Coeffs eqCoeffs{0.25e-9, 25e-9, 22e-9, 300e3, 1e6, 20e3, 65e3};
#endif
    };

    struct Bass : Processor
    {
        Bass(AudioProcessorValueTreeState &a, strix::VolumeMeterSource &s) : Processor(a, ProcessorType::Bass, s)
        {
#if USE_SIMD
            toneStack = std::make_unique<ToneStackNodal<vec>>(eqCoeffs);
            pentode = std::make_unique<Pentode<vec>>();
#else
            toneStack = std::make_unique<ToneStackNodal<double>>(eqCoeffs);
            pentode = std::make_unique<Pentode<double>>();
#endif
            triode.resize(4);
            bias.resize(4);
        }

        void prepare(const dsp::ProcessSpec &spec) override
        {
            defaultPrepare(spec);

            scoop.prepare(spec);
            scoop.coefficients = dsp::IIR::Coefficients<double>::makePeakFilter(spec.sampleRate, 650.0, 1.0, 0.5);
            setBias();
        }

        void setBias()
        {
            bias[0].first = 1.0;
            bias[0].second = 2.0;
            bias[1].first = 1.0;
            bias[1].second = 2.0;
            bias[2].first = 1.0;
            bias[2].second = 2.0;
            bias[3].first = 1.0;
            bias[3].second = 2.0;

            for (size_t i = 0; i < triode.size(); ++i)
                triode[i].bias = bias[i];
        }

        template <typename T>
        void processBlock(dsp::AudioBlock<T> &block)
        {
            T gain_raw = jmap(inGain->get(), 1.f, 8.f);
            T out_raw = jmap(outGain->get(), 1.f, 8.f);

            T autoGain = 1.0;

            if (!*apvts.getRawParameterValue("compPos"))
                comp.processBlock(block, *p_comp, *linked);

#if USE_SIMD
            auto simdBlock = simd.interleaveBlock(block);
            auto &&processBlock = simdBlock;
#else
            auto &&processBlock = block;
#endif

            if (*dist > 0.f)
                mxr.processBlock(processBlock);

            triode[0].process(processBlock);

            processBlock.multiplyBy(gain_raw);

            if (*inGainAuto)
                autoGain *= 1.0 / gain_raw;

            if (*hiGain)
            {
                processBlock.multiplyBy(2.f);
                if (*inGainAuto)
                    autoGain *= 0.5;
            }

            triode[1].process(processBlock);

            for (int ch = 0; ch < processBlock.getNumChannels(); ++ch)
            {
                auto dest = processBlock.getChannelPointer(ch);
                for (int i = 0; i < processBlock.getNumSamples(); ++i)
                    dest[i] = scoop.processSample(dest[i]);
            }

            if (*hiGain)
            {
                triode[2].process(processBlock);
                triode[3].process(processBlock);
            }

            toneStack->process(processBlock);

            processBlock.multiplyBy(out_raw * 6.0);

            if (*outGainAuto)
                autoGain *= 1.0 / out_raw;

            if (!*hiGain)
                pentode->processBlockClassB(processBlock, 0.6, 0.6);
            else
                pentode->processBlockClassB(processBlock, 0.7, 0.7);

            processBlock.multiplyBy(autoGain);

#if USE_SIMD
            simd.deinterleaveBlock(processBlock);
#endif

            if (*apvts.getRawParameterValue("compPos"))
                comp.processBlock(block, *p_comp, *linked);
        }

    private:
        std::vector<bias_t> bias;

#if USE_SIMD
        dsp::IIR::Filter<vec> scoop;
        ToneStackNodal<vec>::Coeffs eqCoeffs{(vec)0.5e-9, (vec)10e-9, (vec)10e-9, (vec)250e3, (vec)0.5e6, (vec)100e3, (vec)200e3};
#else
        dsp::IIR::Filter<double> scoop;
        ToneStackNodal<double>::Coeffs eqCoeffs{0.5e-9, 10e-9, 10e-9, 250e3, 0.5e6, 100e3, 200e3};
#endif
    };

    struct Channel : Processor
    {
        Channel(AudioProcessorValueTreeState &a, strix::VolumeMeterSource &s) : Processor(a, ProcessorType::Channel, s)
        {
            toneStack = nullptr;
#if USE_SIMD
            pentode = std::make_unique<Pentode<vec>>();
#else
            pentode = std::make_unique<Pentode<double>>();
#endif
            triode.resize(2);
        }

        void prepare(const dsp::ProcessSpec &spec) override
        {
            SR = spec.sampleRate;

            defaultPrepare(spec);

            low.prepare(spec);
            setFilters(0);

            mid.prepare(spec);
            setFilters(1);

            hi.prepare(spec);
            setFilters(2);
        }

        void setBias(size_t id, float newFirst, float newSecond)
        {
            triode[id].bias.first = newFirst;
            triode[id].bias.second = newSecond;
        }

        void update(const dsp::ProcessSpec &spec, const float lowGain = 0.5f, const float midGain = 0.5f, const float trebleGain = 0.5f)
        {
            SR = spec.sampleRate;

            defaultPrepare(spec);

            this->lowGain = lowGain;
            this->midGain = midGain;
            this->trebGain = trebleGain;
            updateFilters = true;
        }

        void setFilters(int index, float newValue = 0.5f)
        {
            auto gaindB = jmap(newValue, -6.f, 6.f);
            auto gain = Decibels::decibelsToGain(gaindB);
            switch (index)
            {
            case 0:
                *low.coefficients = *dsp::IIR::Coefficients<double>::makeLowShelf(SR, 250.0, 1.0, gain);
                break;
            case 1:
            {
                double Q = 0.707;
                Q *= 1.0 / gain;
                *mid.coefficients = *dsp::IIR::Coefficients<double>::makePeakFilter(SR, 900.0, Q, gain);
            }
            break;
            case 2:
                *hi.coefficients = *dsp::IIR::Coefficients<double>::makeHighShelf(SR, 5000.0, 0.5, gain);
                break;
            default:
                break;
            }

            setEQAutoGain();
        }

        template <typename T>
        void processBlock(dsp::AudioBlock<T> &block)
        {
            T gain_raw = jmap(inGain->get(), 1.f, 4.f);
            T out_raw = jmap(outGain->get(), 1.f, 4.f);

#if USE_SIMD
            vec autoGain = 1.0;
#else
            double autoGain = 1.0;
#endif

            if (!*apvts.getRawParameterValue("compPos"))
                comp.processBlock(block, *p_comp, *linked);

#if USE_SIMD
            auto &&processBlock = simd.interleaveBlock(block);
#else
            auto &&processBlock = block;
#endif

            if (*dist > 0.f)
                mxr.processBlock(processBlock);

            processBlock.multiplyBy(gain_raw);

            if (*inGain > 0.f)
            {
                double pre_lim = jmap(inGain->get(), 0.5f, 1.f);
                setBias(0, pre_lim, 0.5 * gain_raw);
                triode[0].process(processBlock);
                if (*hiGain)
                {
                    setBias(1, pre_lim, gain_raw);
                    triode[1].process(processBlock);
                }
            }

            if (*inGainAuto)
                autoGain *= 1.0 / std::sqrt(std::sqrt(gain_raw * gain_raw * gain_raw));

            processFilters(processBlock);

            if (*eqAutoGain)
                autoGain *= getEQAutoGain();

            if (*outGain > 0.f)
            {
                processBlock.multiplyBy(6.0 * out_raw);

                if (!*hiGain)
                    pentode->processBlockClassB(processBlock, 0.4f, 0.4f);
                else
                    pentode->processBlockClassB(processBlock, 0.6f, 0.6f);
            }

            if (*outGainAuto)
                autoGain *= 1.0 / out_raw;

#if USE_SIMD
            processBlock.multiplyBy(xsimd::reduce_max(autoGain));

            simd.deinterleaveBlock(processBlock);
#else
            processBlock.multiplyBy(autoGain);
#endif
            if (*apvts.getRawParameterValue("compPos"))
                comp.processBlock(block, *p_comp, *linked);
        }

    private:
#if USE_SIMD
        dsp::IIR::Filter<vec> low, mid, hi;
#else
        dsp::IIR::Filter<double> low, mid, hi;
#endif
        double lowGain = 0.0, midGain = 0.0, trebGain = 0.0;

        std::atomic<bool> updateFilters = false;

        double autoGain_m = 1.0;

        template <class Block>
        void processFilters(Block &block)
        {
            if (updateFilters)
            {
                setFilters(0, lowGain);
                setFilters(1, midGain);
                setFilters(2, trebGain);
                updateFilters = false;
            }

            for (auto ch = 0; ch < block.getNumChannels(); ++ch)
            {
                auto in = block.getChannelPointer(ch);

                for (auto i = 0; i < block.getNumSamples(); ++i)
                {
                    in[i] = low.processSample(in[i]);
                    in[i] = mid.processSample(in[i]);
                    in[i] = hi.processSample(in[i]);
                }
            }
        }

        // get magnitude at some specific frequencies and take the reciprocal
        void setEQAutoGain()
        {
            autoGain_m = 1.0;

            auto l_mag = low.coefficients->getMagnitudeForFrequency(300.0, SR);

            auto m_mag = mid.coefficients->getMagnitudeForFrequency(2500.0, SR);

            auto h_mag = hi.coefficients->getMagnitudeForFrequency(2500.0, SR);

            if (l_mag || m_mag || h_mag)
            {
                autoGain_m *= 1.0 / l_mag;
                autoGain_m *= 1.0 / m_mag;
                autoGain_m *= 1.0 / h_mag;
            }
        }

        inline double getEQAutoGain() noexcept { return autoGain_m; }
    };

} // namespace Processors