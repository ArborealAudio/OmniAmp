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
                pentode.prepare(lastSpec);
                break;
            case Sunbeam:
                pentode.type = PentodeType::Classic;
                pentode.bias.first = 3.0;
                pentode.bias.second = 1.2;
                pentode.prepare(lastSpec);
                break;
            case Moonbeam:
                pentode.type = PentodeType::Nu;
                pentode.bias.first = 0.6;
                pentode.bias.second = 0.6;
                pentode.prepare(lastSpec);
                break;
            case XRay:
                pentode.type = PentodeType::Classic;
                pentode.bias.first = 2.0;
                pentode.bias.second = 1.2;
                pentode.prepare(lastSpec);
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
#if USE_SIMD
            toneStack = std::make_unique<ToneStackNodal<vec>>(eqCoeffs);
#else
            toneStack = std::make_unique<ToneStackNodal<double>>(eqCoeffs);
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
                    pentode.processBlockClassB(processBlock /* , 0.4f, 0.4f */);
                else
                    pentode.processBlockClassB(processBlock /* , 0.6f, 0.6f */);
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