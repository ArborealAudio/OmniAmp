/**
 * Reverb.hpp
 * Manager & container classes for reverb
*/

/**
 * A struct for containing all reverb parameters, can be modified together or individually
 */
struct ReverbParams
{
    /* room size in Ms, use this in conjunction w/ rt60 to create bigger or larger rooms */
    float roomSizeMs;
    /* controls density of reverberation (basically the feedback) of the algo */
    float rt60;
    /* a gain value that sets the level of early reflections. Tapers logarithmically from this
    value to something lower. */
    float erLevel;
    /* Set the level of dampening, as a fraction of Nyquist, in the feedback path */
    float dampening;
    /* Sets the max frequency of the modulation. Each delay line will have a mod rate that
    logarithmically ranges from zero to this parameter */
    float modulation;
    /* Pre-delay in samples */
    float preDelay;
};

#pragma once
#include "MixMatrix.h"
#include "Diffuser.h"
#include "MixedFeedback.h"
#include "StereoMultiMixer.h"

#define DOWNSAMPLE_REVERB 0

enum class ReverbType
{
    Off,
    Room,
    Hall
};

template <int channels, typename Type>
class Room
{
public:
    Room(ReverbType initType)
    {
        switch (initType)
        {
        case ReverbType::Off: /* just use Room as the default if initialized to Off */
            setReverbParams(ReverbParams{30.f, 0.65f, 0.5f, 0.23f, 3.f, 0.f});
            break;
        case ReverbType::Room:
            setReverbParams(ReverbParams{30.f, 0.65f, 0.5f, 0.23f, 3.f, 0.f});
            break;
        case ReverbType::Hall:
            setReverbParams(ReverbParams{75.0f, 2.0f, 0.5f, 1.f, 5.f, 0.f});
            break;
        }
    }

    Room(const ReverbParams &_params)
    {
        setReverbParams(_params);
    }

    /**
     * method for setting all reverb parameters, useful for changing btw reverb types
     * @param init whether or not this is the initial call to setReverbParams. True by default, this defers setting up the diffuser delay lines until prepare(). If false, this will change the diffuser delay lines. Pass false if changing params after initialization*/
    void setReverbParams(const ReverbParams &newParams, bool init = true)
    {
        params = newParams;

        params.preDelay = newParams.preDelay;
        preDelay.setDelay(params.preDelay);

        params.roomSizeMs = jmax(params.roomSizeMs, params.rt60 * 10.f);

        auto diffusion = (params.roomSizeMs * 0.001);
        assert(diff.size() == 4);
        for (int i = 0; i < 4; ++i)
        {
            diffusion *= 0.5;
            diff[i].delayRange = diffusion;
            if (!init)
                diff[i].changeDelay();
        }

        erLevel = params.erLevel;

        feedback.updateParams(params);
    }

    /* change parameters for reverb size. call this if changing decay, too, but just use decay mutliplier first */
    void setSize(float newRoomSizeMs, float newRT60, float newERLevel)
    {
        newRoomSizeMs = jmax(newRoomSizeMs, newRT60 * 10.f);

        auto diffusion = (newRoomSizeMs * 0.001);
        assert(diff.size() == 4);
        for (int i = 0; i < 4; ++i)
        {
            diffusion *= 0.5;
            diff[i].updateDelay(diffusion);
        }
        erLevel = newERLevel;
        feedback.updateDelayAndDecay(newRoomSizeMs, newRT60);
    }

    void setPredelay(float newPredelay)
    {
        sm_predelay.setTargetValue(newPredelay);
        if (!sm_predelay.isSmoothing())
            preDelay.setDelay(newPredelay);
    }

    /* pass in the down-sampled specs */
    void prepare(const dsp::ProcessSpec &spec)
    {
        numChannels = spec.numChannels;

        usBuf.setSize(2, spec.maximumBlockSize * ratio);
        splitBuf.setSize(channels, spec.maximumBlockSize);
        erBuf.setSize(channels, spec.maximumBlockSize);
        dsBuf.setSize(2, spec.maximumBlockSize);

        preDelay.prepare(spec);
        preDelay.setMaximumDelayInSamples(spec.sampleRate);

        for (auto &d : diff)
            d.prepare(spec);

        feedback.prepare(spec);

        auto coeffs = dsp::FilterDesign<double>::designIIRLowpassHighOrderButterworthMethod(11025.0, spec.sampleRate * ratio, 8);

        dsLP[0].clear();
        dsLP[1].clear();
        usLP[0].clear();
        usLP[1].clear();

        for (auto &c : coeffs)
        {
            dsLP[0].emplace_back(dsp::IIR::Filter<double>(c));
            dsLP[1].emplace_back(dsp::IIR::Filter<double>(c));

            usLP[0].emplace_back(dsp::IIR::Filter<double>(c));
            usLP[1].emplace_back(dsp::IIR::Filter<double>(c));
        }

        auto filterSpec = spec;
        filterSpec.sampleRate *= (double)ratio;
        filterSpec.maximumBlockSize *= ratio;

        for (auto &ch : dsLP)
            for (auto &f : ch)
                f.prepare(filterSpec);

        for (auto &ch : usLP)
            for (auto &f : ch)
                f.prepare(filterSpec);

        // setup mixer w/ fully sampled specs
        mix.prepare(dsp::ProcessSpec{spec.sampleRate * ratio, spec.maximumBlockSize * ratio, (uint32)numChannels});
        mix.setMixingRule(dsp::DryWetMixingRule::balanced);

        sm_predelay.reset(128);
    }

    void reset()
    {
        for (auto &d : diff)
            d.reset();

        preDelay.reset();
        feedback.reset();

        for (auto &ch : dsLP)
            for (auto &f : ch)
                f.reset();

        for (auto &ch : usLP)
            for (auto &f : ch)
                f.reset();

        usBuf.clear();
        splitBuf.clear();
        erBuf.clear();
        dsBuf.clear();

        mix.reset();
    }

    void process(AudioBuffer<double> &buf, float amt)
    {
        auto numSamples = buf.getNumSamples();
#if DOWNSAMPLE_REVERB
        const auto hNumSamples = numSamples > 1 ? numSamples / ratio : 1;
#else
        const auto hNumSamples = numSamples;
#endif

        if (numChannels > 1)
            mix.pushDrySamples(dsp::AudioBlock<double>(buf).getSubBlock(0, numSamples));
        else
            mix.pushDrySamples(dsp::AudioBlock<double>(buf).getSingleChannelBlock(0).getSubBlock(0, numSamples));

        dsBuf.clear();
        splitBuf.clear();

#if DOWNSAMPLE_REVERB
        if (ratio > 1)
            downsampleBuffer(buf, numSamples);
        else
#endif
          for (size_t ch = 0; ch < buf.getNumChannels(); ++ch)
            dsBuf.copyFrom(ch, 0, buf, ch, 0, numSamples);

        dsp::AudioBlock<double> dsBlock (dsBuf);
        if (numChannels > 1)
            dsBlock = dsBlock.getSubBlock(0, hNumSamples);
        else
            dsBlock = dsBlock.getSingleChannelBlock(0).getSubBlock(0, hNumSamples);

        if (sm_predelay.isSmoothing())
            processSmoothPredelay(dsBlock);
        else
            preDelay.process(dsp::ProcessContextReplacing<double>(dsBlock));

        upMix.stereoToMulti(dsBuf.getArrayOfReadPointers(), splitBuf.getArrayOfWritePointers(), hNumSamples);

        dsp::AudioBlock<double> block(splitBuf);
        block = block.getSubBlock(0, hNumSamples);

        erBuf.clear();

        for (auto i = 0; i < diff.size(); ++i)
        {
            diff[i].process(block);
            auto r = i * 1.0 / diff.size();
            for (auto ch = 0; ch < channels; ++ch)
                erBuf.addFrom(ch, 0, block.getChannelPointer(ch), hNumSamples, erLevel / std::pow(2.0, r));
        }

        feedback.process(block);

        block.add(dsp::AudioBlock<double>(erBuf).getSubBlock(0, hNumSamples));

        upMix.multiToStereo(splitBuf.getArrayOfReadPointers(), dsBuf.getArrayOfWritePointers(), hNumSamples);

#if DOWNSAMPLE_REVERB
        // NOTE: Is usBuf even needed?
        if (ratio > 1)
            upsampleBuffer(usBuf, numSamples);
        else
#endif
          for (size_t ch = 0; ch < buf.getNumChannels(); ++ch)
            usBuf.copyFrom(ch, 0, dsBuf, ch, 0, numSamples);

        usBuf.applyGain(upMix.scalingFactor1());

        mix.setWetMixProportion(amt);
        if (numChannels > 1)
            mix.mixWetSamples(dsp::AudioBlock<double>(usBuf).getSubBlock(0, numSamples));
        else
            mix.mixWetSamples(dsp::AudioBlock<double>(usBuf).getSingleChannelBlock(0).getSubBlock(0, numSamples));

        for (size_t ch = 0; ch < buf.getNumChannels(); ++ch)
            FloatVectorOperations::copy(buf.getWritePointer(ch), usBuf.getReadPointer(ch), numSamples);
    }
    int ratio = 1;
private:
    ReverbParams params;
    std::array<Diffuser<Type, channels>, 4> diff {Diffuser<Type, channels>(0), Diffuser<Type, channels>(1), Diffuser<Type, channels>(2), Diffuser<Type, channels>(3)};
    MixedFeedback<Type, channels> feedback;
    AudioBuffer<double> splitBuf, erBuf, dsBuf, usBuf;
    StereoMultiMixer<Type, channels> upMix;
    dsp::DelayLine<double, dsp::DelayLineInterpolationTypes::Thiran> preDelay{44100};
    SmoothedValue<float> sm_predelay;
    double erLevel = 1.0;
    int numChannels = 0;
    std::vector<dsp::IIR::Filter<Type>> dsLP[2], usLP[2];
    dsp::DryWetMixer<double> mix;

    void processSmoothPredelay(dsp::AudioBlock<double> &block)
    {
        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            float delay_ = sm_predelay.getNextValue();
            for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
            {
                auto *in = block.getChannelPointer(ch);
                preDelay.pushSample(ch, in[i]);
                in[i] = preDelay.popSample(ch, delay_);
            }
        }
    }

    /**
     * @param numSamples number of samples in the input buffer
     */
    void downsampleBuffer(const AudioBuffer<double> &buf, int numSamples)
    {
        auto in = buf.getArrayOfReadPointers();
        auto out = dsBuf.getArrayOfWritePointers();

        for (auto ch = 0; ch < buf.getNumChannels(); ++ch)
        {
            for (auto i = 0; i < numSamples; ++i)
            {
                auto f = in[ch][i];
                for (auto &lp : dsLP[ch])
                    f = lp.processSample(f); // processes full SR
                auto n = i / ratio;
                out[ch][n] = f;
            }

            // copy L->R if the input is mono
            if (buf.getNumChannels() < dsBuf.getNumChannels())
                FloatVectorOperations::copy(dsBuf.getWritePointer(1), dsBuf.getReadPointer(0), numSamples / ratio);
        }
    }

    /**
     * @param numSamples number of samples in the output buffer
     */
    void upsampleBuffer(AudioBuffer<double> &outBuf, int numSamples)
    {
        auto out = outBuf.getArrayOfWritePointers();
        auto in = dsBuf.getArrayOfReadPointers();

        for (auto ch = 0; ch < outBuf.getNumChannels(); ++ch)
        {
            for (auto i = 0; i < numSamples / ratio; ++i)
            {
                auto n = i * ratio;
                out[ch][n] = in[ch][i]; // copy every even sample
                for (int j = n + 1; j < n + ratio; ++j)
                    out[ch][j] = 0.0; // zero-pad odd samples
            }

            for (auto i = 0; i < numSamples; ++i)
            {
                for (auto &lp : usLP[ch])
                    out[ch][i] = lp.processSample(out[ch][i]); // processes @ full SR
            }
        }

        FloatVectorOperations::multiply(out[0], 2.0, numSamples);
        if (outBuf.getNumChannels() > 1)
            FloatVectorOperations::multiply(out[1], 2.0, numSamples);
    }

};

class ReverbManager
{
    AudioProcessorValueTreeState &apvts;
    std::unique_ptr<Room<8, double>> currentRev, newRev;

    enum ReverbState
    {
        Bypassed,
        ProcessCurrentReverb,
        ProcessFadeToWet,
        ProcessFadeToDry,
        ProcessFadeBetween
    };
    ReverbState state;

    AudioBuffer<double> tmp;
    strix::Crossfade fade;

    strix::ChoiceParameter *type;
    uint8 lastType;
    strix::FloatParameter *decay, *size, *predelay;
    float lastDecay, lastSize, lastPredelay;

public:

    double reverb_samplerate = 44100.0;

    ReverbManager(AudioProcessorValueTreeState &v) : apvts(v)
    {
        type = (strix::ChoiceParameter *)apvts.getParameter("reverbType");
        lastType = type->getIndex();
        decay = (strix::FloatParameter *)apvts.getParameter("reverbDecay");
        size = (strix::FloatParameter *)apvts.getParameter("reverbSize");
        predelay = (strix::FloatParameter *)apvts.getParameter("reverbPredelay");

        ReverbParams params;
        float d = *decay;
        lastDecay = d;
        float s = *size;
        lastSize = s;
        float p = *predelay;
        lastPredelay = p;
        float ref_mod = s * 0.5f;
        s *= d;
        s = jmax(0.05f, s);
        if (type->getIndex() < 2)
            params = ReverbParams{30.f * s, 0.65f * s, 1.f * (1.f - ref_mod), 0.3f, 3.f, p};
        else
            params = ReverbParams{75.f * s, 2.f * s, 1.f * (1.f - ref_mod), 1.f, 5.f, p};

        currentRev = std::make_unique<Room<8, double>>(params);
        newRev = std::make_unique<Room<8, double>>(params);
    }

    void prepare(const dsp::ProcessSpec &spec)
    {
        auto hSpec = spec;
#if DOWNSAMPLE_REVERB
        if ((int)spec.sampleRate % 44100 == 0)
            reverb_samplerate = 44100.0;
        else if ((int)spec.sampleRate % 48000 == 0)
            reverb_samplerate = 48000.0;
        DBG("reverb SR: " << reverb_samplerate);
        auto ratio = spec.sampleRate / reverb_samplerate;
        DBG("ratio: " << ratio);
        if (ratio < 1.0)
            ratio = 1.0;
        currentRev->ratio = ratio;
        newRev->ratio = ratio;
        hSpec.sampleRate = reverb_samplerate;
        hSpec.maximumBlockSize = spec.maximumBlockSize / ratio;
#else
        reverb_samplerate = spec.sampleRate;
#endif
        currentRev->prepare(hSpec);
        newRev->prepare(hSpec);

        state = type->getIndex() ? ProcessCurrentReverb : Bypassed;

        tmp.setSize(spec.numChannels, spec.maximumBlockSize, false, true, false);
    }

    void reset()
    {
        currentRev->reset();
        newRev->reset();
    }

    void manageUpdate(bool changingPredelay)
    {
        float d = *decay;
        float s = *size;
        lastDecay = d;
        lastSize = s;
        float ref_mod = s * 0.5f; // btw 0 - 1 w/ midpoint @ 0.5
        s *= d;
        s = jmax(0.05f, s);
        float p = *predelay * 0.001f * reverb_samplerate;

        if (!changingPredelay)
        {
            switch ((ReverbType)type->getIndex())
            {
            case ReverbType::Room:
                newRev->reset();
                newRev->setReverbParams(ReverbParams{30.f * s, 0.65f * s, 1.f * (1.f - ref_mod), 0.3f, 3.f, p}, false);
                if (state == Bypassed)
                    state = ProcessFadeToWet;
                else // switching types/params
                    state = ProcessFadeBetween;
                break;
            case ReverbType::Hall:
                newRev->reset();
                newRev->setReverbParams(ReverbParams{75.f * s, 2.f * s, 1.f * (1.f - ref_mod), 1.f, 5.f, p}, false);
                if (state == Bypassed) // previously bypassed
                    state = ProcessFadeToWet;
                else // switching types/params
                    state = ProcessFadeBetween;
                break;
            case ReverbType::Off:
                state = ProcessFadeToDry;
                break;
            }
            // fade incoming, set fade time & flag
            fade.setFadeTime(reverb_samplerate, 0.5f);
        }
        else
        {
            currentRev->setPredelay(p);
            lastPredelay = p;
        }
    }

    void process(AudioBuffer<double> &buffer, float amt)
    {
        auto t = type->getIndex();
        
        if (state == Bypassed || state == ProcessCurrentReverb) // check for param changes if no crossfade
        {
            if (lastDecay != *decay && state != Bypassed)
                manageUpdate(false);
            if (lastSize != *size && state != Bypassed)
                manageUpdate(false);
            if (lastType != t)
                manageUpdate(false);
            if (lastPredelay != *predelay)
                manageUpdate(true);
        }

        switch (state)
        {
        case Bypassed:
            lastType = t;
            return;
            break;
        case ProcessCurrentReverb:
            currentRev->process(buffer, amt);
            lastType = t;
            break;
        case ProcessFadeToWet:
            tmp.makeCopyOf(buffer, true);
            newRev->process(buffer, amt);
            fade.processWithState(tmp, buffer, buffer.getNumSamples());
            if (fade.complete)
            {
                newRev.swap(currentRev);
                lastType = t;
                state = ProcessCurrentReverb;
            }
            break;
        case ProcessFadeToDry:
            tmp.makeCopyOf(buffer, true);
            currentRev->process(tmp, amt);
            fade.processWithState(tmp, buffer, buffer.getNumSamples());
            if (fade.complete)
            {
                lastType = t;
                state = Bypassed;
            }
            break;
        case ProcessFadeBetween:
            tmp.makeCopyOf(buffer, true);
            currentRev->process(tmp, amt);
            newRev->process(buffer, amt);
            fade.processWithState(tmp, buffer, buffer.getNumSamples());
            if (fade.complete)
            {
                currentRev.swap(newRev);
                lastType = t;
                state = ProcessCurrentReverb;
            }
            break;
        }
    }
};