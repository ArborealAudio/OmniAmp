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
        Modern,
        Vintage
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

#include "Tube.h"
#include "PreFilters.h"
#include "EmphasisFilters.h"
#include "ToneStack.h"
#include "Comp.h"
#include "Enhancer.h"
#include "Cab.h"
#include "DistPlus.h"
#include "Reverb/Reverb.hpp"

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
            ampAutoGain = dynamic_cast<strix::BoolParameter *>(apvts.getParameter("ampAutoGain"));
            outGain = dynamic_cast<strix::FloatParameter *>(apvts.getParameter("powerampGain"));
            lastOutGain = *outGain;
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

        void parameterChanged(const String &parameterID, float) override
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

        OptoComp<double> comp;

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

#if USE_SIMD
        MXRDistWDF<vec> mxr;
        std::unique_ptr<ToneStack<vec>> toneStack;
        std::vector<AVTriode<vec>> triode;
        Pentode<vec> pentode;
#else
        MXRDistWDF<double> mxr;
        std::unique_ptr<ToneStack<double>> toneStack;
        std::vector<AVTriode<double>> triode;
        Pentode<double> pentode;
#endif
        Preamp preamp;

        double fudgeGain = 1.f; // dumb thing we need to keep diff. amp types in similar ballpark

        strix::FloatParameter *inGain, *outGain, *p_comp, *dist;
        strix::BoolParameter *ampAutoGain, *hiGain, *linked;
        double lastInGain, lastOutGain;

        strix::SIMD<double, dsp::AudioBlock<double>, strix::AudioBlock<vec>> simd;

        double SR = 44100.0;
        int numSamples = 0, numChannels = 0;
        dsp::ProcessSpec lastSpec;
    };

    template <typename T>
    struct Guitar : Processor
    {
        Guitar(AudioProcessorValueTreeState &a, strix::VolumeMeterSource &s) : Processor(a, ProcessorType::Guitar, s)
        {
            guitarMode = dynamic_cast<strix::ChoiceParameter *>(apvts.getParameter("guitarMode"));
            currentType = static_cast<GuitarMode>(guitarMode->getIndex());
#if USE_SIMD
            toneStack = std::make_unique<ToneStack<vec>>(ToneStack<vec>::Type::Nodal);
#else
            toneStack = std::make_unique<ToneStack<double>>(ToneStack<double>::Type::Nodal);
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
            Processor::parameterChanged(parameterID, newValue);
        }

        void setToneStack()
        {
            switch (currentType)
            {
            case GammaRay:
                toneStack->setNodalCoeffs((T)0.25e-9, (T)25e-9, (T)22e-9, (T)300e3, (T)1e6, (T)20e3, (T)65e3);
                break;
            case Sunbeam:
                toneStack->setNodalCoeffs((T)0.25e-9, (T)15e-9, (T)250e-9, (T)300e3, (T)400e3, (T)1e3, (T)20e3);
                break;
            case Moonbeam:
                toneStack->setNodalCoeffs((T)0.25e-9, (T)20e-9, (T)50e-9, (T)300e3, (T)500e3, (T)5e3, (T)12e3);
                break;
            case XRay:
                toneStack->setNodalCoeffs((T)0.25e-9, (T)22e-9, (T)20e-9, (T)270e3, (T)1e6, (T)50e3, (T)65e3);
                break;
            }
        }

        void setBias()
        {
            switch (currentType)
            {
            case GammaRay:
                for (auto &t : triode)
                    t.type = TriodeType::VintageTube;
                triode[0].bias.first = 1.0;
                triode[0].bias.second = 1.0;
                triode[1].bias.first = 1.4;
                triode[1].bias.second = 2.4;
                triode[2].bias.first = 1.4;
                triode[2].bias.second = 0.5;
                triode[3].bias.first = 1.0;
                triode[3].bias.second = .5;
                break;
            case Sunbeam:
                for (auto &t : triode)
                    t.type = TriodeType::ModernTube;
                triode[0].bias.first = 1.5;
                triode[1].bias.first = 0.55;
                triode[2].bias.first = 5.0;
                break;
            case Moonbeam:
                for (auto &t : triode)
                    t.type = TriodeType::ModernTube;
                triode[0].bias.first = 1.0;
                triode[1].bias.first = 2.18;
                triode[2].bias.first = 4.0;
                triode[3].bias.first = 5.0;
                break;
            case XRay:
                for (auto &t : triode)
                    t.type = TriodeType::VintageTube;
                triode[0].bias.first = 1.0;
                triode[0].bias.second = 2.0;
                triode[1].bias.first = 2.0;
                triode[1].bias.second = 3.0;
                triode[2].bias.first = 2.0;
                triode[2].bias.second = 3.0;
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
                preamp.procs.push_back(toneStack.get());
                preamp.procs.push_back(&triode[2]);
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
                pentode.bias.first = 0.85;
                // pentode.bias.second = 2.0;
                break;
            case Sunbeam:
                pentode.type = PentodeType::Classic;
                pentode.bias.first = 5.0;
                pentode.bias.second = 5.0;
                break;
            case Moonbeam:
                pentode.type = PentodeType::Nu;
                pentode.bias.first = 1.5;
                // pentode.bias.second = 2.0;
                break;
            case XRay:
                pentode.type = PentodeType::Classic;
                pentode.bias.first = 5.0;
                pentode.bias.second = 1.2;
                break;
            }
        }

        template <typename FloatType>
        void processBlock(dsp::AudioBlock<FloatType> &block)
        {
            FloatType gain_raw = jmap(inGain->get(), 1.f, 12.f);
            FloatType out_raw = jmap(outGain->get(), 1.f, 12.f);

            gtrPre.inGain = gain_raw;
            pentode.inGain = *outGain;

            FloatType autoGain = 1.0;
            bool ampAutoGain_ = *ampAutoGain;

#if USE_SIMD
            auto simdBlock = simd.interleaveBlock(block);
            auto &&processBlock = simdBlock;
#else
            auto &&processBlock = block;
#endif
            if (*dist > 0.f)
                mxr.processBlock(processBlock);
            else
                mxr.setInit(true);

            strix::SmoothGain<T>::applySmoothGain(processBlock, gain_raw, lastInGain);
            // if (ampAutoGain_)
            //     autoGain *= 1.0 / gain_raw;

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

            strix::SmoothGain<T>::applySmoothGain(processBlock, out_raw, lastOutGain);
            pentode.processBlockClassB(processBlock);

            // if (ampAutoGain_)
            //     autoGain *= 1.0 / out_raw;

            // processBlock.multiplyBy(autoGain);

#if USE_SIMD
            simd.deinterleaveBlock(processBlock);
#endif
        }

        GuitarMode currentType = GammaRay;

    private:
        strix::ChoiceParameter *guitarMode;
        std::atomic<bool> ampChanged = false;

        GuitarPreFilter<T> gtrPre;
    };

    template <typename T>
    struct Bass : Processor
    {
        Bass(AudioProcessorValueTreeState &a, strix::VolumeMeterSource &s) : Processor(a, ProcessorType::Bass, s)
        {
            bassMode = dynamic_cast<strix::ChoiceParameter *>(apvts.getParameter("bassMode"));
            currentType = static_cast<BassMode>(bassMode->getIndex());
#if USE_SIMD
            toneStack = std::make_unique<ToneStack<vec>>(ToneStack<vec>::Type::Nodal);
#else
            toneStack = std::make_unique<ToneStack<double>>(ToneStack<double>::Type::Nodal);
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
            Processor::parameterChanged(parameterID, newValue);
        }

        void setToneStack()
        {
            switch (currentType)
            {
            case Cobalt:
                toneStack->setNodalCoeffs((T)0.25e-9, (T)25e-9, (T)15e-9, (T)250e3, (T)500e3, (T)50e3, (T)200e3);
                break;
            case Emerald:
                toneStack->setBiquadFreqs(300.0, 900.0, 200.0, 1000.0, 2300.0);
                break;
            case Quartz:
                toneStack->setNodalCoeffs((T)0.25e-9, (T)8e-9, (T)12e-9, (T)250e3, (T)750e3, (T)100e3, (T)300e3);
                break;
            }
        }

        void setBias()
        {
            triode[0].bias.first = 1.0;
            triode[0].bias.second = 2.0;
            switch (currentType)
            {
            case Cobalt:
                for (auto &t : triode)
                    t.type = TriodeType::VintageTube;
                triode[1].bias.first = 4.0;
                triode[1].bias.second = 4.0;
                triode[2].bias.first = 2.0;
                triode[2].bias.second = 2.0;
                triode[3].bias.first = 10.0;
                triode[3].bias.second = 15.0;
                break;
            case Emerald:
                for (auto &t : triode)
                    t.type = TriodeType::ModernTube;
                triode[1].bias.first = 12.0;
                triode[2].bias.first = 3.0;
                triode[3].bias.first = 6.0;
                break;
            case Quartz:
                for (auto &t : triode)
                    t.type = TriodeType::ModernTube;
                triode[1].bias.first = 5.0;
                triode[2].bias.first = 25.0;
                triode[3].bias.first = 25.0;
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
                fudgeGain = 2.0;
                break;
            case Emerald:
                preamp.procs.push_back(toneStack.get());
                preamp.procs.push_back(&triode[1]);
                preamp.procs.push_back(&triode[2]);
                preamp.procs.push_back(&triode[3]);
                preamp.procs.push_back(&preFilter);
                fudgeGain = 1.0;
                break;
            case Quartz:
                preamp.procs.push_back(&triode[1]);
                preamp.procs.push_back(toneStack.get());
                preamp.procs.push_back(&preFilter);
                preamp.procs.push_back(&triode[2]);
                preamp.procs.push_back(&triode[3]);
                fudgeGain = 2.0;
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
                pentode.bias.first = 0.7;
                break;
            case Emerald:
                pentode.type = PentodeType::Nu;
                pentode.bias.first = 0.8;
                break;
            case Quartz:
                pentode.type = PentodeType::Classic;
                pentode.bias.first = 1.6;
                pentode.bias.second = 1.0;
                break;
            }
        }

        template <typename FloatType>
        void processBlock(dsp::AudioBlock<FloatType> &block)
        {
            FloatType gain_raw = jmap(inGain->get(), 1.f, 8.f);
            FloatType out_raw = jmap(outGain->get(), 1.f, 8.f);

            preFilter.inGain = gain_raw;
            pentode.inGain = *outGain;

            FloatType autoGain = 1.0;
            bool ampAutoGain_ = *ampAutoGain;

#if USE_SIMD
            auto simdBlock = simd.interleaveBlock(block);
            auto &&processBlock = simdBlock;
#else
            auto &&processBlock = block;
#endif
            if (*dist > 0.f)
                mxr.processBlock(processBlock);
            else
                mxr.setInit(true);

            triode[0].process(processBlock);

            strix::SmoothGain<T>::applySmoothGain(processBlock, gain_raw, lastInGain);

            // if (ampAutoGain_)
            //     autoGain *= 1.0 / gain_raw;

            if (*hiGain)
            {
                processBlock.multiplyBy(2.f);
                if (ampAutoGain_)
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

            strix::SmoothGain<T>::applySmoothGain(processBlock, out_raw, lastOutGain);

            // if (ampAutoGain_)
            //     autoGain *= 1.0 / out_raw;

            pentode.processBlockClassB(processBlock);

            processBlock *= fudgeGain;

            // processBlock.multiplyBy(autoGain);

#if USE_SIMD
            simd.deinterleaveBlock(processBlock);
#endif
        }

    private:
        strix::ChoiceParameter *bassMode = nullptr;
        BassMode currentType;
        BassPreFilter<T> preFilter;
        std::atomic<bool> ampChanged = false;
    };

    template <typename T>
    struct Channel : Processor
    {
        Channel(AudioProcessorValueTreeState &a, strix::VolumeMeterSource &s) : Processor(a, ProcessorType::Channel, s)
        {
            toneStack = nullptr;
            channelMode = dynamic_cast<strix::ChoiceParameter *>(apvts.getParameter("channelMode"));
            currentType = static_cast<ChannelMode>(channelMode->getIndex());
            triode.resize(2);
            for (auto &t : triode)
                t.type = TriodeType::ChannelTube;
            apvts.addParameterListener("channelMode", this);
        }

        ~Channel()
        {
            apvts.removeParameterListener("channelMode", this);
        }

        void parameterChanged(const String &paramID, float newValue) override
        {
            if (paramID == "channelMode")
            {
                currentType = (ChannelMode)channelMode->getIndex();
                updateFilters = true;
            }
            Processor::parameterChanged(paramID, newValue);
        }

        void prepare(const dsp::ProcessSpec &spec) override
        {
            SR = spec.sampleRate;

            defaultPrepare(spec);

            setPreamp(*inGain);
            setPoweramp();

            low.prepare(spec);
            setFilters(0);

            mid.prepare(spec);
            setFilters(1);

            hi.prepare(spec);
            setFilters(2);

            sm_low.reset(SR, 0.01f);
            sm_low.setCurrentAndTargetValue(lowGain);
            sm_mid.reset(SR, 0.01f);
            sm_mid.setCurrentAndTargetValue(midGain);
            sm_hi.reset(SR, 0.01f);
            sm_hi.setCurrentAndTargetValue(trebGain);

            tmp = (T**)malloc(sizeof(T*) * spec.numChannels);
            for (size_t ch = 0; ch < spec.numChannels; ++ch)
                tmp[ch] = (T*)malloc(sizeof(T) * spec.maximumBlockSize);
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
            if (currentType == Vintage)
            {
                triode[0].type = TriodeType::ChannelTube;
                triode[1].type = TriodeType::ChannelTube;
                setBias(0, pre_lim, 0.5f * gain_raw); // p: 1 - 2 n: 0.5 - 2
            }
            else
            {
                triode[0].type = TriodeType::ModernTube;
                triode[1].type = TriodeType::ModernTube;
                setBias(0, pre_lim * 1.5f, pre_lim * 1.5f); // p & n: 1 - 2
            }
            if (*hiGain)
            {
                if (currentType == Vintage)
                    setBias(1, pre_lim, gain_raw); // p: 0.5 - 1 n: 1 - 4
                else
                    setBias(1, gain_raw, gain_raw); // p & n: 1 - 4
            }
#if 0
            setBias(0, JUCE_LIVE_CONSTANT(pre_lim),
            JUCE_LIVE_CONSTANT(0.5f * gain_raw));
            setBias(1, JUCE_LIVE_CONSTANT(pre_lim),
            JUCE_LIVE_CONSTANT(gain_raw));
#endif
        }

        inline void setPoweramp(/* float base */)
        {
            if (currentType == Modern)
            {
                pentode.setType(PentodeType::Nu);
                pentode.bias.first = 1.2;
                pentode.bias.second = 1.2;
            }
            else
            {
                pentode.setType(PentodeType::Classic);
                pentode.bias.first = 10.0;
                pentode.bias.second = 10.0;
            }
#if 0
            pentode.bias.first = JUCE_LIVE_CONSTANT(bias);
            pentode.bias.second = JUCE_LIVE_CONSTANT(bias);
#endif
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
                lowGain = newValue;
                if (currentType == Modern)
                    *low.coefficients = dsp::IIR::ArrayCoefficients<double>::makeLowShelf(SR, 250.0, 1.0, gain);
                else
                    *low.coefficients = dsp::IIR::ArrayCoefficients<double>::makeLowShelf(SR, 300.0, 0.66, gain);
                break;
            case 1:
            {
                midGain = newValue;
                double Q = 0.707;
                Q *= 1.0 / gain;
                if (currentType == Modern)
                    *mid.coefficients = dsp::IIR::ArrayCoefficients<double>::makePeakFilter(SR, 900.0, Q, gain);
                else
                    *mid.coefficients = dsp::IIR::ArrayCoefficients<double>::makePeakFilter(SR, 800.0, Q, gain);
            }
            break;
            case 2:
                trebGain = newValue;
                if (currentType == Modern)
                    *hi.coefficients = dsp::IIR::ArrayCoefficients<double>::makeHighShelf(SR, 5000.0, 0.8, gain);
                else {
                    auto freq = 6500.0;
                    if (freq > SR * 0.5)
                        freq = SR * 0.5;
                    *hi.coefficients = dsp::IIR::ArrayCoefficients<double>::makeHighShelf(SR, freq, 0.5, gain);
                }
                break;
            }
        }

        template <typename FloatType>
        void processBlock(dsp::AudioBlock<FloatType> &block)
        {
            auto inGain_ = inGain->get();
            auto outGain_ = outGain->get();
            FloatType gain_raw = jmap(inGain_, 1.f, 4.f);
            FloatType out_raw = jmap(outGain_, 1.f, 4.f);

            if (inGain_ > 0.f && lastInGain> 1.f)
                preampTubeState = ProcessNormal;
            else if (inGain_ <= 0.f && lastInGain <= 1.f)
                preampTubeState = Bypassed;
            else if (inGain_ <= 0.f && lastInGain >= 1.f)
                preampTubeState = ProcessRampOff;
            else if (inGain_ >= 0.f && lastInGain <= 1.f)
                preampTubeState = ProcessRampOn;

            if (outGain_ > 0.f && lastOutGain> 1.f)
                powerampTubeState = ProcessNormal;
            else if (outGain_ <= 0.f && lastOutGain <= 1.f)
                powerampTubeState = Bypassed;
            else if (outGain_ <= 0.f && lastOutGain >= 1.f)
                powerampTubeState = ProcessRampOff;
            else if (outGain_ >= 0.f && lastOutGain <= 1.f)
                powerampTubeState = ProcessRampOn;

            pentode.inGain = outGain_;

            FloatType autoGain = 1.0;
            bool ampAutoGain_ = *ampAutoGain;

#if USE_SIMD
            auto &&processBlock = simd.interleaveBlock(block);
#else
            auto &&processBlock = block;
#endif

            if (*dist > 0.f)
                mxr.processBlock(processBlock);
            else
                mxr.setInit(true);

            switch (preampTubeState)
            {
            case Bypassed:
                break;
            case ProcessNormal:
                setPreamp(inGain_);
                strix::SmoothGain<T>::applySmoothGain(processBlock, gain_raw, lastInGain);
                triode[0].process(processBlock);
                if (*hiGain)
                    triode[1].process(processBlock);
                break;
            case ProcessRampOn:
            {
                for (size_t ch = 0; ch < processBlock.getNumChannels(); ++ch)
                {
                    auto in = processBlock.getChannelPointer(ch);
                    for (size_t i = 0; i < processBlock.getNumSamples(); ++i)
                        tmp[ch][i] = in[i];
                }
                auto tmpBlock = strix::AudioBlock<vec> (tmp, processBlock.getNumChannels(), processBlock.getNumSamples());
                setPreamp(inGain_);
                strix::SmoothGain<T>::applySmoothGain(processBlock, gain_raw, lastInGain);
                triode[0].process(processBlock);
                if (*hiGain)
                    triode[1].process(processBlock);
                strix::Crossfade::process(tmpBlock, processBlock, processBlock.getNumSamples());
                } break;
            case ProcessRampOff:
            {
                for (size_t ch = 0; ch < processBlock.getNumChannels(); ++ch)
                {
                    auto in = processBlock.getChannelPointer(ch);
                    for (size_t i = 0; i < processBlock.getNumSamples(); ++i)
                        tmp[ch][i] = in[i];
                }
                auto tmpBlock = strix::AudioBlock<vec> (tmp, processBlock.getNumChannels(), processBlock.getNumSamples());
                setPreamp(inGain_);
                strix::SmoothGain<T>::applySmoothGain(processBlock, gain_raw, lastInGain);
                triode[0].process(tmpBlock);
                if (*hiGain)
                    triode[1].process(tmpBlock);
                strix::Crossfade::process(tmpBlock, processBlock, processBlock.getNumSamples());
                } break;
            }

            if (ampAutoGain_)
                autoGain *= 1.0 / std::sqrt(std::sqrt(gain_raw * gain_raw * gain_raw));

            processFilters(processBlock);

            if (ampAutoGain_)
                autoGain *= autoGain_m;

            // if (outGain_ > 0.f)
            // {
            //     setPoweramp();
            //     strix::SmoothGain<T>::applySmoothGain(processBlock, out_raw, lastOutGain);
            //     pentode.processBlockClassB(processBlock);
            // }
            switch (powerampTubeState)
            {
            case Bypassed:
                break;
            case ProcessNormal:
                setPoweramp();
                strix::SmoothGain<T>::applySmoothGain(processBlock, out_raw, lastOutGain);
                pentode.processBlockClassB(processBlock);
                break;
            case ProcessRampOn:
            {
                for (size_t ch = 0; ch < processBlock.getNumChannels(); ++ch)
                {
                    auto in = processBlock.getChannelPointer(ch);
                    for (size_t i = 0; i < processBlock.getNumSamples(); ++i)
                        tmp[ch][i] = in[i];
                }
                auto tmpBlock = strix::AudioBlock<vec> (tmp, processBlock.getNumChannels(), processBlock.getNumSamples());
                setPoweramp();
                strix::SmoothGain<T>::applySmoothGain(processBlock, out_raw, lastOutGain);
                pentode.processBlockClassB(processBlock);
                strix::Crossfade::process(tmpBlock, processBlock, processBlock.getNumSamples());
                } break;
            case ProcessRampOff:
            {
                for (size_t ch = 0; ch < processBlock.getNumChannels(); ++ch)
                {
                    auto in = processBlock.getChannelPointer(ch);
                    for (size_t i = 0; i < processBlock.getNumSamples(); ++i)
                        tmp[ch][i] = in[i];
                }
                auto tmpBlock = strix::AudioBlock<vec> (tmp, processBlock.getNumChannels(), processBlock.getNumSamples());
                setPoweramp();
                strix::SmoothGain<T>::applySmoothGain(processBlock, out_raw, lastOutGain);
                pentode.processBlockClassB(tmpBlock);
                strix::Crossfade::process(tmpBlock, processBlock, processBlock.getNumSamples());
                } break;
            }

            if (ampAutoGain_)
                autoGain *= 1.0 / out_raw;

            strix::SmoothGain<T>::applySmoothGain(processBlock, autoGain, lastAutoGain);
#if USE_SIMD
            simd.deinterleaveBlock(processBlock);
#endif
        }

        SmoothedValue<float> sm_low, sm_mid, sm_hi;

    private:
        strix::ChoiceParameter *channelMode;
        ChannelMode currentType;
        dsp::IIR::Filter<T> low, mid, hi;
        double lowGain = 0.0, midGain = 0.0, trebGain = 0.0;

        std::atomic<bool> updateFilters = false;

        double autoGain_m = 1.0, lastAutoGain = 1.0;
        T **tmp;

        enum TubeState
        {
            Bypassed,
            ProcessNormal,
            ProcessRampOn,
            ProcessRampOff
        };
        TubeState preampTubeState, powerampTubeState;

        template <class Block>
        void processFilters(Block &block)
        {
            if (updateFilters) // if channel mode changed
            {
                setFilters(0, lowGain);
                setFilters(1, midGain);
                setFilters(2, trebGain);
                updateFilters = false;
            }

            SmoothedValue<float> *smoothers [3] { &sm_low, &sm_mid, &sm_hi };
            uint8 bitmask = 0;
            for (size_t i = 0; i < 3; ++i)
            {
                if (smoothers[i]->isSmoothing())
                    bitmask |= 1 << i;
            }

            if (bitmask > 0)
            {
                for (size_t i = 0; i < block.getNumSamples(); ++i)
                {
                    // set all filters that need update
                    int bit = 1;
                    for (int n = 0; n < 3; ++n)
                    {
                        if (bit & bitmask)
                            setFilters(n, smoothers[n]->getNextValue());

                        bit <<= 1;
                    }

                    for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
                    {
                        auto in = block.getChannelPointer(ch);

                        in[i] = low.processSample(in[i]);
                        in[i] = mid.processSample(in[i]);
                        in[i] = hi.processSample(in[i]);
                    }
                }
                setEQAutoGain();
                return;
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
            setEQAutoGain();
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
    };

} // namespace Processors