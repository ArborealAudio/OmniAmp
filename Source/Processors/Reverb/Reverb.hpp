/**
 * Reverb.hpp
 * Manager & container classes for reverb
 */

/**
 * A struct for containing all reverb parameters, can be modified together or
 * individually
 */
struct ReverbParams
{
    /* room size in Ms, use this in conjunction w/ rt60 to create bigger or
     * larger rooms */
    float roomSizeMs;
    /* controls density of reverberation (basically the feedback) of the algo */
    float rt60;
    /* a gain value that sets the level of early reflections. Tapers
    logarithmically from this value to something lower. */
    float erLevel;
    /* Set the level of dampening, as a fraction of Nyquist, in the feedback
     * path */
    float dampening;
    /* Sets the max frequency of the modulation. Each delay line will have a mod
    rate that logarithmically ranges from zero to this parameter */
    float modulation;
    /* Pre-delay in samples */
    float preDelay;
    /* brightness */
    bool bright;
};

#pragma once
#include "Diffuser.h"
#include "MixMatrix.h"
#include "MixedFeedback.h"
#include "StereoMultiMixer.h"

enum class ReverbType
{
    Off,
    Room,
    Hall
};

template <int channels, typename Type> class Room
{
  public:
    Room(ReverbType initType)
    {
        switch (initType) {
        case ReverbType::Off: /* just use Room as the default if initialized to
                                 Off */
            setReverbParams(
                ReverbParams{30.f, 0.65f, 0.5f, 0.23f, 3.f, 0.f, false});
            break;
        case ReverbType::Room:
            setReverbParams(
                ReverbParams{30.f, 0.65f, 0.5f, 0.23f, 3.f, 0.f, false});
            break;
        case ReverbType::Hall:
            setReverbParams(
                ReverbParams{75.0f, 2.0f, 0.5f, 1.f, 5.f, 0.f, false});
            break;
        }
    }

    Room(const ReverbParams &_params) { setReverbParams(_params); }

    /**
     * method for setting all reverb parameters, useful for changing btw reverb
     * types
     * @param init whether or not this is the initial call to setReverbParams.
     * True by default, this defers setting up the diffuser delay lines until
     * prepare(). If false, this will change the diffuser delay lines. Pass
     * false if changing params after initialization*/
    void setReverbParams(const ReverbParams &newParams, bool init = true)
    {
        params = newParams;

        preDelay.setDelay(params.preDelay);

        params.roomSizeMs = jmax(params.roomSizeMs, params.rt60 * 10.f);

        auto diffusion = (params.roomSizeMs * 0.001);
        assert(diff.size() == 4);
        for (int i = 0; i < 4; ++i) {
            diffusion *= 0.5;
            diff[i].delayRange = diffusion;
            if (!init)
                diff[i].changeDelay();
        }

        feedback.updateParams(params);
    }

    /* change parameters for reverb size. call this if changing decay, too, but
     * just use decay mutliplier first */
    void setSize(float newRoomSizeMs, float newRT60, float newERLevel)
    {
        newRoomSizeMs = jmax(newRoomSizeMs, newRT60 * 10.f);

        auto diffusion = (newRoomSizeMs * 0.001);
        assert(diff.size() == 4);
        for (int i = 0; i < 4; ++i) {
            diffusion *= 0.5;
            diff[i].updateDelay(diffusion);
        }
        params.erLevel = newERLevel;
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

        splitBuf.setSize(channels, spec.maximumBlockSize);
        erBuf.setSize(channels, spec.maximumBlockSize);
        wetBuf.setSize(2, spec.maximumBlockSize);

        preDelay.prepare(spec);
        preDelay.setMaximumDelayInSamples(spec.sampleRate);

        for (auto &d : diff)
            d.prepare(spec);

        feedback.prepare(spec);

        auto coeffs = dsp::FilterDesign<double>::
            designIIRLowpassHighOrderButterworthMethod(
                8500.0 < spec.sampleRate * 0.5f
                    ? 8500.0
                    : spec.sampleRate * 0.5f * 0.995f,
                spec.sampleRate, 2);

        lp[0].clear();
        lp[1].clear();

        for (auto &c : coeffs) {
            lp[0].emplace_back(dsp::IIR::Filter<double>(c));
            lp[1].emplace_back(dsp::IIR::Filter<double>(c));
        }

        for (auto &ch : lp)
            for (auto &f : ch)
                f.prepare(spec);

        // setup mixer w/ fully sampled specs
        mix.prepare(spec);
        mix.setMixingRule(dsp::DryWetMixingRule::balanced);

        sm_predelay.reset(128);
    }

    void reset()
    {
        for (auto &d : diff)
            d.reset();

        preDelay.reset();
        feedback.reset();

        for (auto &ch : lp)
            for (auto &f : ch)
                f.reset();

        splitBuf.clear();
        erBuf.clear();
        wetBuf.clear();

        mix.reset();
    }

    void process(AudioBuffer<double> &buf, float amt)
    {
        const auto numSamples = buf.getNumSamples();

        if (numChannels > 1)
            mix.pushDrySamples(
                dsp::AudioBlock<double>(buf).getSubBlock(0, numSamples));
        else
            mix.pushDrySamples(dsp::AudioBlock<double>(buf)
                                   .getSingleChannelBlock(0)
                                   .getSubBlock(0, numSamples));

        splitBuf.clear();
        for (size_t ch = 0; ch < buf.getNumChannels(); ++ch)
            FloatVectorOperations::copy(wetBuf.getWritePointer(ch),
                                        buf.getReadPointer(ch), numSamples);

        // need a sub-buffer which is numSamples-sized (wetBuf may be larger)
        // it also must be stereo to accomadate the actual reverb algorithm
        AudioBuffer<double> wetSubBuf(wetBuf.getArrayOfWritePointers(), 2,
                                      numSamples);
        // copy L->R if input buffer is mono
        if (buf.getNumChannels() < 2)
            wetSubBuf.copyFrom(1, 0, wetSubBuf.getReadPointer(0), numSamples);

        if (!params.bright)
            dampenBuffer(wetSubBuf);

        dsp::AudioBlock<double> dsBlock(wetSubBuf);
        if (numChannels > 1)
            dsBlock = dsBlock.getSubBlock(0, numSamples);
        else
            dsBlock =
                dsBlock.getSingleChannelBlock(0).getSubBlock(0, numSamples);

        if (sm_predelay.isSmoothing())
            processSmoothPredelay(dsBlock);
        else
            preDelay.process(dsp::ProcessContextReplacing<double>(dsBlock));

        upMix.stereoToMulti(wetSubBuf.getArrayOfReadPointers(),
                            splitBuf.getArrayOfWritePointers(), numSamples);

        dsp::AudioBlock<double> block(splitBuf);
        block = block.getSubBlock(0, numSamples);

        erBuf.clear();

        for (auto i = 0; i < diff.size(); ++i) {
            diff[i].process(block);
            auto r = i * 1.0 / diff.size();
            for (auto ch = 0; ch < channels; ++ch)
                erBuf.addFrom(ch, 0, block.getChannelPointer(ch), numSamples,
                              params.erLevel / std::pow(2.0, r));
        }

        feedback.process(block);

        block.add(dsp::AudioBlock<double>(erBuf).getSubBlock(0, numSamples));

        upMix.multiToStereo(splitBuf.getArrayOfReadPointers(),
                            wetSubBuf.getArrayOfWritePointers(), numSamples);

        wetSubBuf.applyGain(upMix.scalingFactor1());

        mix.setWetMixProportion(amt);
        if (numChannels > 1)
            mix.mixWetSamples(
                dsp::AudioBlock<double>(wetSubBuf).getSubBlock(0, numSamples));
        else
            mix.mixWetSamples(dsp::AudioBlock<double>(wetSubBuf)
                                  .getSingleChannelBlock(0)
                                  .getSubBlock(0, numSamples));

        for (size_t ch = 0; ch < buf.getNumChannels(); ++ch)
            FloatVectorOperations::copy(buf.getWritePointer(ch),
                                        wetSubBuf.getReadPointer(ch),
                                        numSamples);
    }

  private:
    ReverbParams params;
    std::array<Diffuser<Type, channels>, 4> diff{
        Diffuser<Type, channels>(0), Diffuser<Type, channels>(1),
        Diffuser<Type, channels>(2), Diffuser<Type, channels>(3)};
    MixedFeedback<Type, channels> feedback;
    AudioBuffer<double> splitBuf, erBuf, wetBuf;
    StereoMultiMixer<Type, channels> upMix;
    dsp::DelayLine<double, dsp::DelayLineInterpolationTypes::Thiran> preDelay{
        44100};
    SmoothedValue<float> sm_predelay;
    int numChannels = 0;
    std::vector<dsp::IIR::Filter<Type>> lp[2];
    dsp::DryWetMixer<double> mix;

    void processSmoothPredelay(dsp::AudioBlock<double> &block)
    {
        for (size_t i = 0; i < block.getNumSamples(); ++i) {
            float delay_ = sm_predelay.getNextValue();
            for (size_t ch = 0; ch < block.getNumChannels(); ++ch) {
                auto *in = block.getChannelPointer(ch);
                preDelay.pushSample(ch, in[i]);
                in[i] = preDelay.popSample(ch, delay_);
            }
        }
    }

    // process Butterworth LP
    void dampenBuffer(AudioBuffer<double> &buf)
    {
        auto in = buf.getArrayOfWritePointers();

        for (auto ch = 0; ch < buf.getNumChannels(); ++ch) {
            for (auto i = 0; i < buf.getNumSamples(); ++i) {
                for (auto &f : lp[ch])
                    in[ch][i] = f.processSample(in[ch][i]);
            }
        }
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
    strix::BoolParameter *bright;
    float lastDecay, lastSize, lastPredelay;
    bool lastBright;

  public:
    double reverb_samplerate = 44100.0;

    ReverbManager(AudioProcessorValueTreeState &v) : apvts(v)
    {
        type = (strix::ChoiceParameter *)apvts.getParameter("reverbType");
        lastType = type->getIndex();
        decay = (strix::FloatParameter *)apvts.getParameter("reverbDecay");
        size = (strix::FloatParameter *)apvts.getParameter("reverbSize");
        predelay =
            (strix::FloatParameter *)apvts.getParameter("reverbPredelay");
        bright = (strix::BoolParameter *)apvts.getParameter("reverbBright");

        ReverbParams params;
        float d = *decay;
        lastDecay = d;
        float s = *size;
        lastSize = s;
        float p = *predelay;
        lastPredelay = p;
        bool b = bright->get();
        lastBright = b;
        float ref_mod = s * 0.5f;
        s *= d;
        s = jmax(0.05f, s);
        if (type->getIndex() < 2)
            params = ReverbParams{
                30.f * s, 0.65f * s, 1.f * (1.f - ref_mod), 0.3f, 3.f, p, b};
        else
            params = ReverbParams{
                75.f * s, 2.f * s, 1.f * (1.f - ref_mod), 1.f, 5.f, p, b};

        currentRev = std::make_unique<Room<8, double>>(params);
        newRev = std::make_unique<Room<8, double>>(params);
    }

    void prepare(const dsp::ProcessSpec &spec)
    {
        reverb_samplerate = spec.sampleRate;
        currentRev->prepare(spec);
        newRev->prepare(spec);

        state = type->getIndex() ? ProcessCurrentReverb : Bypassed;

        tmp.setSize(spec.numChannels, spec.maximumBlockSize, false, true,
                    false);
    }

    void reset()
    {
        currentRev->reset();
        newRev->reset();
    }

    void manageUpdate(bool changingPredelay)
    {
        float p = *predelay * 0.001f * reverb_samplerate;

        if (!changingPredelay) {
            float d = *decay;
            float s = *size;
            lastDecay = d;
            lastSize = s;
            float ref_mod = s * 0.5f; // btw 0 - 1 w/ midpoint @ 0.5
            s *= d;
            s = jmax(0.05f, s);
            bool b = bright->get();
            lastBright = b;

            switch ((ReverbType)type->getIndex()) {
            case ReverbType::Room:
                newRev->reset();
                newRev->setReverbParams(ReverbParams{30.f * s, 0.65f * s,
                                                     1.f * (1.f - ref_mod),
                                                     0.3f, 3.f, p, b},
                                        false);
                if (state == Bypassed)
                    state = ProcessFadeToWet;
                else // switching types/params
                    state = ProcessFadeBetween;
                break;
            case ReverbType::Hall:
                newRev->reset();
                newRev->setReverbParams(ReverbParams{75.f * s, 2.f * s,
                                                     1.f * (1.f - ref_mod), 1.f,
                                                     5.f, p, b},
                                        false);
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
        } else {
            currentRev->setPredelay(p);
            lastPredelay = p;
        }
    }

    void process(AudioBuffer<double> &buffer, float amt)
    {
        auto t = type->getIndex();

        if (state == Bypassed ||
            state ==
                ProcessCurrentReverb) // check for param changes if no crossfade
        {
            if (lastDecay != *decay && state != Bypassed)
                manageUpdate(false);
            if (lastSize != *size && state != Bypassed)
                manageUpdate(false);
            if (lastType != t)
                manageUpdate(false);
            if (lastPredelay != *predelay)
                manageUpdate(true);
            if (lastBright != bright->get())
                manageUpdate(false);
        }

        switch (state) {
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
            if (fade.complete) {
                newRev.swap(currentRev);
                lastType = t;
                state = ProcessCurrentReverb;
            }
            break;
        case ProcessFadeToDry:
            tmp.makeCopyOf(buffer, true);
            currentRev->process(tmp, amt);
            fade.processWithState(tmp, buffer, buffer.getNumSamples());
            if (fade.complete) {
                lastType = t;
                state = Bypassed;
            }
            break;
        case ProcessFadeBetween:
            tmp.makeCopyOf(buffer, true);
            currentRev->process(tmp, amt);
            newRev->process(buffer, amt);
            fade.processWithState(tmp, buffer, buffer.getNumSamples());
            if (fade.complete) {
                currentRev.swap(newRev);
                lastType = t;
                state = ProcessCurrentReverb;
            }
            break;
        }
    }
};