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
        Moonbeam,
        XRay
    };

    enum BassMode
    {
        Cobalt,
        Emerald,
        Quartz
    };

    enum ChannelMode
    {
        A,
        B
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

        bool shouldBypass = false;
    };

    namespace clip
    {
        template <typename SampleType, typename T>
        inline void clip(SampleType *in, size_t numSamples, T pLim, T nLim)
        {
            for (size_t i = 0; i < numSamples; ++i)
            {
#if USE_SIMD
                in[i] = xsimd::select(in[i] > (SampleType)pLim, (SampleType)pLim, in[i]);
                in[i] = xsimd::select(in[i] < (SampleType)nLim, (SampleType)nLim, in[i]);
#else
                if (in[i] > pLim)
                    in[i] = pLim;
                else if (in[i] < nLim)
                    in[i] = nLim;
#endif
            }
        }

        template <typename Block, typename T>
        inline void clip(Block& block, T pLim, T nLim)
        {
            for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
                clip(block.getChannelPointer(ch), block.getNumSamples(), pLim, nLim);
        }
    }

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

        Preamp() = default;

#if USE_SIMD
        void process(strix::AudioBlock<vec> &block)
        {
            for (auto p : procs)
                if (!p->shouldBypass)
                    p->process(block);
        }
#else
        void process(dsp::AudioBlock<double> &block)
        {
            for (auto *p : procs)
                if (!p->shouldBypass)
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
        Processor(AudioProcessorValueTreeState &a, ProcessorType t, strix::VolumeMeterSource &s) : apvts(a),
                                                                                                   comp(t, s, a.getRawParameterValue("compPos"))
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

            pentode.reset();
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

            pentode.prepare(spec);

            simd.setInterleavedBlockSize(spec.numChannels, spec.maximumBlockSize);
        }

        AudioProcessorValueTreeState &apvts;

        OptoComp<double> comp;
#if USE_SIMD
        MXRDistWDF<vec> mxr;
        std::unique_ptr<ToneStackNodal<vec>> toneStack;
        std::vector<AVTriode<vec>> triode;
        Pentode<vec> pentode;
#else
        MXRDistWDF<double> mxr;
        std::unique_ptr<ToneStackNodal<double>> toneStack;
        std::vector<AVTriode<double>> triode;
        Pentode<double> pentode;
#endif
        Preamp preamp;
        strix::FloatParameter *inGain, *outGain, *p_comp, *dist;
        strix::BoolParameter *inGainAuto, *outGainAuto, *hiGain, *linked, *eqAutoGain;
        double lastInGain, lastOutGain;

        strix::SIMD<double, dsp::AudioBlock<double>, strix::AudioBlock<vec>> simd;

        double SR = 44100.0;
        int numSamples = 0, numChannels = 0;
        dsp::ProcessSpec lastSpec;
    };

    struct Guitar : Processor
    {
        Guitar(AudioProcessorValueTreeState &a, strix::VolumeMeterSource &s) : Processor(a, ProcessorType::Guitar, s)
        {
            guitarMode = dynamic_cast<strix::ChoiceParameter *>(apvts.getParameter("guitarMode"));
            currentType = static_cast<GuitarMode>(guitarMode->getIndex());
#if USE_SIMD
            toneStack = std::make_unique<ToneStackNodal<vec>>(eqCoeffs);
#else
            toneStack = std::make_unique<ToneStackNodal<double>>(eqCoeffs);
#endif
            gtrPre.hiGain = hiGain;
            gtrPre.type = currentType;
            triode.resize(4);
            setBias();
            setToneStack();
            setPreamp();
            apvts.addParameterListener("guitarMode", this);
        }

        ~Guitar()
        {
            apvts.removeParameterListener("guitarMode", this);
        }

        void prepare(const dsp::ProcessSpec &spec) override
        {
            lastSpec = spec;
            setPoweramp();
            defaultPrepare(spec);
            gtrPre.prepare(spec);
        }

        void parameterChanged(const String &parameterID, float newValue) override
        {
            if (parameterID == "guitarMode")
            {
                currentType = static_cast<GuitarMode>(newValue);
                ampChanged = true;
            }
        }

        void setToneStack()
        {
            switch (currentType)
            {
            case GammaRay:
                eqCoeffs = (ToneStackNodal<vec>::Coeffs((vec)0.25e-9, (vec)25e-9, (vec)22e-9, (vec)300e3, (vec)1e6, (vec)20e3, (vec)65e3));
                break;
            case Sunbeam:
                eqCoeffs = (ToneStackNodal<vec>::Coeffs((vec)0.25e-9, (vec)15e-9, (vec)250e-9, (vec)300e3, (vec)400e3, (vec)1e3, (vec)20e3));
                break;
            case Moonbeam:
                eqCoeffs = (ToneStackNodal<vec>::Coeffs((vec)0.25e-9, (vec)20e-9, (vec)50e-9, (vec)300e3, (vec)500e3, (vec)15e3, (vec)12e3));
                break;
            case XRay:
                eqCoeffs = (ToneStackNodal<vec>::Coeffs((vec)0.5e-9, (vec)22e-9, (vec)22e-9, (vec)250e3, (vec)1e6, (vec)25e3, (vec)33e3));
                break;
            }

            toneStack->changeCoeffs(eqCoeffs);
        }

        void setBias()
        {
            switch (currentType)
            {
            case GammaRay:
                triode[0].bias.first = 0.5;
                triode[0].bias.second = 1.0;
                triode[1].bias.first = 1.4;
                triode[1].bias.second = 2.4;
                triode[2].bias.first = 1.4;
                triode[2].bias.second = 2.4;
                triode[3].bias.first = 2.0;
                triode[3].bias.second = 4.0;
                break;
            case Sunbeam:
                triode[0].bias.first = 0.5;
                triode[0].bias.second = 0.5;
                triode[1].bias.first = 1.0;
                triode[1].bias.second = 2.0;
                triode[2].bias.first = 5.0;
                triode[2].bias.second = 5.0;
                break;
            case Moonbeam:
                triode[0].bias.first = 2.0;
                triode[0].bias.second = 2.5;
                triode[1].bias.first = 3.0;
                triode[1].bias.second = 4.0;
                triode[2].bias.first = 3.0;
                triode[2].bias.second = 4.0;
                triode[3].bias.first = 4.0;
                triode[3].bias.second = 5.0;
                break;
            case XRay:
                triode[0].bias.first = 0.5;
                triode[0].bias.second = 1.0;
                triode[1].bias.first = 1.0;
                triode[1].bias.second = 2.0;
                triode[2].bias.first = 3.0;
                triode[2].bias.second = 4.0;
                triode[3].bias.first = 2.0;
                triode[3].bias.second = 4.0;
                break;
            }
        }

        void setPreamp()
        {
            preamp.procs.clear();

            switch (currentType)
            {
            case GammaRay:
                preamp.procs.push_back(&triode[0]);
                preamp.procs.push_back(&gtrPre);
                preamp.procs.push_back(&triode[1]);
                preamp.procs.push_back(&triode[2]);
                preamp.procs.push_back(toneStack.get());
                preamp.procs.push_back(&triode[3]);
                break;
            case Sunbeam:
                preamp.procs.push_back(&gtrPre);
                preamp.procs.push_back(&triode[0]);
                preamp.procs.push_back(&triode[1]);
                preamp.procs.push_back(toneStack.get());
                preamp.procs.push_back(&triode[2]);
                break;
            case Moonbeam:
                preamp.procs.push_back(&gtrPre);
                preamp.procs.push_back(&triode[0]);
                preamp.procs.push_back(&triode[1]);
                preamp.procs.push_back(&triode[2]);
                preamp.procs.push_back(toneStack.get());
                preamp.procs.push_back(&triode[3]);
                break;
            case XRay:
                preamp.procs.push_back(&triode[0]);
                preamp.procs.push_back(&gtrPre);
                preamp.procs.push_back(&triode[1]);
                preamp.procs.push_back(&triode[2]);
                preamp.procs.push_back(toneStack.get());
                preamp.procs.push_back(&triode[3]);
                break;
            }
        }

        void updatePreamp()
        {
            setToneStack();
            gtrPre.type = currentType;
            gtrPre.changeFilters();
            setBias();
            setPreamp();
        }

        void setPoweramp()
        {
            switch (currentType)
            {
            case GammaRay:
                pentode.type = PentodeType::Nu;
                pentode.bias.first = 0.6;
                pentode.bias.second = 0.6;
                break;
            case Sunbeam:
                pentode.type = PentodeType::Classic;
                pentode.bias.first = 3.0;
                pentode.bias.second = 1.2;
                break;
            case Moonbeam:
                pentode.type = PentodeType::Nu;
                pentode.bias.first = 0.6;
                pentode.bias.second = 0.6;
                break;
            case XRay:
                pentode.type = PentodeType::Classic;
                pentode.bias.first = 2.0;
                pentode.bias.second = 1.2;
                break;
            }
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

            /* perform sync swap of preamp & poweramp if needed */
            if (ampChanged)
            {
                updatePreamp();
                setPoweramp();
                ampChanged = false;
            }
            triode.back().shouldBypass = !*hiGain;
            if (currentType == Sunbeam) // check for extra bypasses in Sunbeam
            {
                triode[2].shouldBypass = !*hiGain;
                gtrPre.shouldBypass = !*hiGain;
            }
            else
            {
                triode[2].shouldBypass = false;
                gtrPre.shouldBypass = false;
            }
            preamp.process(processBlock);

            processBlock.multiplyBy(out_raw);

            pentode.processBlockClassB(processBlock);

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
        strix::ChoiceParameter *guitarMode;
        std::atomic<bool> ampChanged = false;

        ToneStackNodal<vec>::Coeffs eqCoeffs;

#if USE_SIMD
        GuitarPreFilter<vec> gtrPre;
#else
        GuitarPreFilter<double> gtrPre;
#endif
    };

    struct Bass : Processor
    {
        Bass(AudioProcessorValueTreeState &a, strix::VolumeMeterSource &s) : Processor(a, ProcessorType::Bass, s)
        {
            bassMode = dynamic_cast<strix::ChoiceParameter *>(apvts.getParameter("bassMode"));
            currentType = static_cast<BassMode>(bassMode->getIndex());
#if USE_SIMD
            toneStack = std::make_unique<ToneStackNodal<vec>>(eqCoeffs);
#else
            toneStack = std::make_unique<ToneStackNodal<double>>(eqCoeffs);
#endif
            preFilter.hiGain = hiGain;
            preFilter.type = currentType;
            triode.resize(4);
            setBias();
            setToneStack();
            setPreamp();
            apvts.addParameterListener("bassMode", this);
        }

        ~Bass()
        {
            apvts.removeParameterListener("bassMode", this);
        }

        void prepare(const dsp::ProcessSpec &spec) override
        {
            lastSpec = spec;
            setPoweramp();
            defaultPrepare(spec);
            preFilter.prepare(spec);
        }

        void parameterChanged(const String &parameterID, float newValue) override
        {
            if (parameterID == "bassMode")
            {
                currentType = static_cast<BassMode>(newValue);
                ampChanged = true;
            }
        }

        void setToneStack()
        {
            switch (currentType)
            {
            case Cobalt:
                eqCoeffs = ToneStackNodal<vec>::Coeffs((vec)0.5e-9, (vec)10e-9, (vec)15e-9, (vec)250e3, (vec)0.5e6, (vec)100e3, (vec)200e3);
                break;
            case Emerald:
                eqCoeffs = ToneStackNodal<vec>::Coeffs((vec)0.45e-9, (vec)12e-9, (vec)10e-9, (vec)500e3, (vec)5e6, (vec)50e3, (vec)150e3);
                break;
            case Quartz:
                eqCoeffs = ToneStackNodal<vec>::Coeffs((vec)0.6e-9, (vec)8e-9, (vec)12e-9, (vec)250e3, (vec)0.5e6, (vec)100e3, (vec)200e3);
                break;
            }

            toneStack->changeCoeffs(eqCoeffs);
        }

        void setBias()
        {
            triode[0].bias.first = 1.0;
            triode[0].bias.second = 2.0;
            switch (currentType)
            {
            case Cobalt:
                triode[1].bias.first = 1.0;
                triode[1].bias.second = 2.0;
                triode[2].bias.first = 1.0;
                triode[2].bias.second = 2.0;
                triode[3].bias.first = 1.0;
                triode[3].bias.second = 2.0;
                break;
            case Emerald:
                triode[1].bias.first = 1.0;
                triode[1].bias.second = 2.0;
                triode[2].bias.first = 2.0;
                triode[2].bias.second = 2.0;
                triode[3].bias.first = 4.0;
                triode[3].bias.second = 3.0;
                break;
            case Quartz:
                triode[1].bias.first = 1.0;
                triode[1].bias.second = 2.0;
                triode[2].bias.first = 1.0;
                triode[2].bias.second = 2.0;
                triode[3].bias.first = 1.0;
                triode[3].bias.second = 2.0;
                break;
            }
        }

        void setPreamp()
        {
            preamp.procs.clear();

            switch (currentType)
            {
            case Cobalt:
                preamp.procs.push_back(&triode[1]);
                preamp.procs.push_back(&preFilter);
                preamp.procs.push_back(&triode[2]);
                preamp.procs.push_back(&triode[3]);
                preamp.procs.push_back(toneStack.get());
                break;
            case Emerald:
                preamp.procs.push_back(toneStack.get());
                preamp.procs.push_back(&triode[1]);
                preamp.procs.push_back(&triode[2]);
                preamp.procs.push_back(&triode[3]);
                preamp.procs.push_back(&preFilter);
                break;
            case Quartz:
                preamp.procs.push_back(&triode[1]);
                preamp.procs.push_back(&preFilter);
                preamp.procs.push_back(&triode[2]);
                preamp.procs.push_back(&triode[3]);
                preamp.procs.push_back(toneStack.get());
                break;
            }
        }

        void updatePreamp()
        {
            setToneStack();
            preFilter.type = currentType;
            preFilter.changeFilters();
            setBias();
            setPreamp();
        }

        void setPoweramp()
        {
            switch (currentType)
            {
            case Cobalt:
                pentode.type = PentodeType::Nu;
                pentode.bias.first = *hiGain ? 0.7 : 0.6;
                pentode.bias.second = *hiGain ? 0.7 : 0.6;
                break;
            case Emerald:
                pentode.type = PentodeType::Nu;
                pentode.bias.first = 0.8;
                pentode.bias.second = 1.0;
                break;
            case Quartz:
                pentode.type = PentodeType::Classic;
                pentode.bias.first = 2.5;
                pentode.bias.second = 2.0;
                break;
            }
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

            if (ampChanged)
            {
                updatePreamp();
                setPoweramp();
                ampChanged = false;
            }
            triode[2].shouldBypass = !*hiGain;
            triode[3].shouldBypass = !*hiGain;
            preamp.process(processBlock);

            processBlock.multiplyBy(out_raw);

            if (*outGainAuto)
                autoGain *= 1.0 / out_raw;

            if (!*hiGain)
                pentode.processBlockClassB(processBlock /* , 0.6, 0.6 */);
            else
                pentode.processBlockClassB(processBlock /* , 0.7, 0.7 */);

            processBlock.multiplyBy(autoGain);

#if USE_SIMD
            simd.deinterleaveBlock(processBlock);
#endif

            if (*apvts.getRawParameterValue("compPos"))
                comp.processBlock(block, *p_comp, *linked);
        }

    private:
        strix::ChoiceParameter *bassMode = nullptr;
        BassMode currentType;
#if USE_SIMD
        ToneStackNodal<vec>::Coeffs eqCoeffs;
        BassPreFilter<vec> preFilter;
#else
        ToneStackNodal<double>::Coeffs eqCoeffs;
#endif
        std::atomic<bool> ampChanged = false;
    };

    struct Channel : Processor
    {
        Channel(AudioProcessorValueTreeState &a, strix::VolumeMeterSource &s) : Processor(a, ProcessorType::Channel, s)
        {
            toneStack = nullptr;
            channelMode = dynamic_cast<strix::ChoiceParameter *>(apvts.getParameter("channelMode"));
            currentType = static_cast<ChannelMode>(channelMode->getIndex());
            triode.resize(2);
        }

        void prepare(const dsp::ProcessSpec &spec) override
        {
            SR = spec.sampleRate;

            defaultPrepare(spec);

            setPreamp(*inGain);
            setPoweramp(*outGain);

            low.prepare(spec);
            setFilters(0);

            mid.prepare(spec);
            setFilters(1);

            hi.prepare(spec);
            setFilters(2);
        }

        inline void setBias(size_t id, float newFirst, float newSecond)
        {
            triode[id].bias.first = newFirst;
            triode[id].bias.second = newSecond;
        }

        inline void setPreamp(float base)
        {
            auto gain_raw = jmap(base, 1.f, 4.f);
            auto pre_lim = jmap(base, 0.5f, 1.f);
            triode[0].type = currentType;
            triode[1].type = currentType;
            if (currentType == B)
                setBias(0, pre_lim, 0.5f * gain_raw);
            else
                setBias(0, pre_lim * 2.f, pre_lim * 2.f);
            if (*hiGain)
            {
                if (currentType == B)
                    setBias(1, pre_lim, gain_raw);
                else
                    setBias(1, gain_raw, gain_raw);
            }
        }

        inline void setPoweramp(float base)
        {
            float bias = jmap(base, 1.f, 4.f);
            if (currentType == A)
            {
                pentode.type = PentodeType::Nu;
                pentode.bias.first = bias;
                pentode.bias.second = bias;
            }
            else
            {
                pentode.type = PentodeType::Classic;
                pentode.bias.first = bias * 2.0;
                pentode.bias.second = bias * 2.0;
            }
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
            double gain_raw = jmap(inGain->get(), 1.f, 4.f);
            double out_raw = jmap(outGain->get(), 1.f, 4.f);
            currentType = (ChannelMode)channelMode->getIndex();

            double autoGain = 1.0;

            if (!*apvts.getRawParameterValue("compPos"))
                comp.processBlock(block, *p_comp, *linked);

#if USE_SIMD
            auto &&processBlock = simd.interleaveBlock(block);
#else
            auto &&processBlock = block;
#endif

            if (*dist > 0.f)
                mxr.processBlock(processBlock);

            if (*inGain > 0.f)
            {
                setPreamp(*inGain);
                strix::SmoothGain<vec>::applySmoothGain(processBlock, gain_raw, lastInGain);
                triode[0].process(processBlock);
                if (*hiGain)
                    triode[1].process(processBlock);
            }

            if (*inGainAuto)
                autoGain *= 1.0 / std::sqrt(std::sqrt(gain_raw * gain_raw * gain_raw));

            processFilters(processBlock);

            if (*eqAutoGain)
                autoGain *= getEQAutoGain();

            if (*outGain > 0.f)
            {
                setPoweramp(*outGain);
                strix::SmoothGain<vec>::applySmoothGain(processBlock, out_raw, lastOutGain);
                pentode.processBlockClassB(processBlock);
            }

            if (*outGainAuto)
                autoGain *= 1.0 / out_raw;

#if USE_SIMD
            processBlock.multiplyBy(autoGain);

            simd.deinterleaveBlock(processBlock);
#else
            processBlock.multiplyBy(autoGain);
#endif
            if (*apvts.getRawParameterValue("compPos"))
                comp.processBlock(block, *p_comp, *linked);
        }

    private:
        strix::ChoiceParameter *channelMode;
        ChannelMode currentType;
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