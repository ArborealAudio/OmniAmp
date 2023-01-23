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

    /** PROBLEM: We're not accounting for samplerates higher than 44.1kHz! We need to refactor it to hardcode a SR of 22kHz
     *  call this before Prepare! sets a ratio to downsample the internal processing, defaults to 1 */
    void setDownsampleRatio(int dsRatio) { ratio = dsRatio; }

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

        auto coeffs = dsp::FilterDesign<double>::designIIRLowpassHighOrderButterworthMethod(spec.sampleRate / ratio, spec.sampleRate * (double)ratio, 8);

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

        // ratio *= spec.sampleRate / 44100.0;

        auto filterSpec = spec;
        filterSpec.sampleRate *= (double)ratio;
        filterSpec.maximumBlockSize *= ratio;

        for (auto &ch : dsLP)
            for (auto &f : ch)
                f.prepare(filterSpec);

        for (auto &ch : usLP)
            for (auto &f : ch)
                f.prepare(filterSpec);

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
        auto hNumSamples = numSamples / ratio;

        if (numChannels > 1)
            mix.pushDrySamples(dsp::AudioBlock<double>(buf).getSubBlock(0, numSamples));
        else
            mix.pushDrySamples(dsp::AudioBlock<double>(buf).getSingleChannelBlock(0).getSubBlock(0, numSamples));

        dsBuf.clear();
        splitBuf.clear();

        downsampleBuffer(buf, numSamples);

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

        if (splitBuf.getNumSamples() > hNumSamples)
            block = block.getSubBlock(0, hNumSamples);

        erBuf.clear();

        for (auto i = 0; i < diff.size(); ++i)
        {
            diff[i].process(block);
            auto r = i * 1.0 / diff.size();
            for (auto ch = 0; ch < channels; ++ch)
                erBuf.addFrom(ch, 0, block.getChannelPointer(ch), block.getNumSamples(), erLevel / std::pow(2.0, r));
        }

        feedback.process(block);

        block.add(dsp::AudioBlock<double>(erBuf).getSubBlock(0, hNumSamples));

        upMix.multiToStereo(splitBuf.getArrayOfReadPointers(), dsBuf.getArrayOfWritePointers(), hNumSamples);

        upsampleBuffer(usBuf, numSamples);

        usBuf.applyGain(upMix.scalingFactor1());

        mix.setWetMixProportion(amt);
        if (numChannels > 1)
            mix.mixWetSamples(dsp::AudioBlock<double>(usBuf).getSubBlock(0, numSamples));
        else
            mix.mixWetSamples(dsp::AudioBlock<double>(usBuf).getSingleChannelBlock(0).getSubBlock(0, numSamples));

        buf.makeCopyOf(usBuf, true);
    }
private:
    ReverbParams params;
    std::array<Diffuser<Type, channels>, 4> diff {Diffuser<Type, channels>(0), Diffuser<Type, channels>(1), Diffuser<Type, channels>(2), Diffuser<Type, channels>(3)};
    MixedFeedback<Type, channels> feedback;
    AudioBuffer<double> splitBuf, erBuf, dsBuf, usBuf;
    StereoMultiMixer<Type, channels> upMix;
    dsp::DelayLine<double, dsp::DelayLineInterpolationTypes::Thiran> preDelay{44100};
    SmoothedValue<float> sm_predelay;
    double erLevel = 1.0;
    int ratio = 1;
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
                    out[ch][j] = 0.0;
            }

            for (auto i = 0; i < numSamples; ++i)
            {
                for (auto &lp : usLP[ch])
                    out[ch][i] = lp.processSample(out[ch][i]); // processes @ full SR
            }
        }

        FloatVectorOperations::multiply(out[0], 2.0, outBuf.getNumSamples());
        if (outBuf.getNumChannels() > 1)
            FloatVectorOperations::multiply(out[1], 2.0, outBuf.getNumSamples());
    }

};

class ReverbManager : AudioProcessorValueTreeState::Listener, Timer
{
    AudioProcessorValueTreeState &apvts;
    std::unique_ptr<Room<8, double>> currentRev, newRev;
    dsp::ProcessSpec memSpec;

    enum reverb_flags
    {
        TYPE,
        DECAY,
        SIZE,
        PREDELAY,
    };
    std::queue<reverb_flags> msgs;
    std::mutex mutex;
    std::atomic<bool> reverbReady = false, processingAudio = false;

    AudioBuffer<double> tmp1, tmp2;
    strix::Crossfade fade;

    strix::ChoiceParameter *type;
    uint8 lastType;
    strix::FloatParameter *decay, *size, *predelay;
    float lastDecay, lastSize, lastPredelay;

    double SR = 44100.0;
    int ratio = 1;

public:
    ReverbManager(AudioProcessorValueTreeState &v) : apvts(v)
    {
        type = (strix::ChoiceParameter *)apvts.getParameter("reverbType");
        decay = (strix::FloatParameter *)apvts.getParameter("reverbDecay");
        size = (strix::FloatParameter *)apvts.getParameter("reverbSize");
        predelay = (strix::FloatParameter *)apvts.getParameter("reverbPredelay");
        // apvts.addParameterListener("reverbType", this);
        // apvts.addParameterListener("reverbDecay", this);
        // apvts.addParameterListener("reverbSize", this);
        // apvts.addParameterListener("reverbPredelay", this);

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

        startTimerHz(10);
    }

    ~ReverbManager() override
    {
        // apvts.removeParameterListener("reverbType", this);
        // apvts.removeParameterListener("reverbDecay", this);
        // apvts.removeParameterListener("reverbSize", this);
        // apvts.removeParameterListener("reverbPredelay", this);
        stopTimer();
    }

    void setDownsampleRatio(int dsRatio)
    {
        currentRev->setDownsampleRatio(dsRatio);
        newRev->setDownsampleRatio(dsRatio);
        ratio = dsRatio;
    }

    void prepare(const dsp::ProcessSpec &spec)
    {
        auto hSpec = spec;
        hSpec.sampleRate /= ratio;
        hSpec.maximumBlockSize /= ratio;
        memSpec = hSpec;
        currentRev->prepare(hSpec);
        newRev->prepare(hSpec);
        SR = hSpec.sampleRate;

        tmp1.setSize(spec.numChannels, spec.maximumBlockSize);
        tmp2.setSize(spec.numChannels, spec.maximumBlockSize);
    }

    void reset()
    {
        currentRev->reset();
        newRev->reset();
    }

    void parameterChanged(const String &paramID, float ) override
    {
        std::unique_lock<std::mutex> lock(mutex);
        reverb_flags flag;
        if (paramID == "reverbType")
            flag = TYPE;
        else if (paramID == "reverbDecay")
        {
            flag = DECAY;
        }
        else if (paramID == "reverbSize")
        {
            flag = SIZE;
        }
        else if (paramID == "reverbPredelay")
            flag = PREDELAY;
        else
            return;

        msgs.emplace(flag);
    }

    void timerCallback() override
    {
        // if (msgs.size() >= 10)
        //     return;
        // std::unique_lock<std::mutex> lock(mutex);
        // if (lastDecay != *decay)
        //     msgs.emplace(DECAY);
        // if (lastSize != *size)
        //     msgs.emplace(SIZE);
        // if (lastType != type->getIndex())
        //     msgs.emplace(TYPE);
        // if (lastPredelay != *predelay)
        //     msgs.emplace(PREDELAY);
    }

    void manageUpdate(reverb_flags flag)
    {
        float d = *decay;
        float s = *size;
        lastDecay = d;
        lastSize = s;
        float ref_mod = s * 0.5f; // btw 0 - 1 w/ midpoint @ 0.5
        s *= d;
        s = jmax(0.05f, s);
        float p = *predelay * 0.001f * SR;

        switch (flag)
        {
        case DECAY:
        case SIZE:
            DBG("Changing params");
            reverbReady = false;
            switch ((ReverbType)type->getIndex())
            {
            case ReverbType::Room:
                newRev->setReverbParams(ReverbParams{30.f * s, 0.65f * s, 1.f * (1.f - ref_mod), 0.3f, 3.f, p}, false);
                break;
            case ReverbType::Hall:
                newRev->setReverbParams(ReverbParams{75.0f * s, 2.0f * s, 1.f * (1.f - ref_mod), 1.0f, 5.0f, p}, false);
                break;
            case ReverbType::Off:
                break;
            }
            // fade incoming, set fade time & flag
            fade.setFadeTime(SR * ratio, 0.5f);
            reverbReady = true;
            break;
        case TYPE:
        {
            DBG("Changing params");
            reverbReady = false;
            switch ((ReverbType)type->getIndex())
            {
            case ReverbType::Room:
                newRev->setReverbParams(ReverbParams{30.f * s, 0.65f * s, 1.f * (1.f - ref_mod), 0.3f, 3.f, p}, false);
                break;
            case ReverbType::Hall:
                newRev->setReverbParams(ReverbParams{75.0f * s, 2.0f * s, 1.f * (1.f - ref_mod), 1.0f, 5.0f, p}, false);
                break;
            case ReverbType::Off:
                break;
            }
            lastType = type->getIndex();
            // fade incoming, set fade time & flag
            fade.setFadeTime(SR * ratio, 0.5f);
            reverbReady = true;
            break;
        }
        case PREDELAY:
            currentRev->setPredelay(p);
            lastPredelay = p;
            break;
        }
    }

    void process(AudioBuffer<double> &buffer, float amt)
    {
        if (!processingAudio)
        {
            if (lastDecay != *decay)
                manageUpdate(DECAY);
            if (lastSize != *size)
                manageUpdate(SIZE);
            if (lastType != type->getIndex())
                manageUpdate(TYPE);
            if (lastPredelay != *predelay)
                manageUpdate(PREDELAY);
        }

        if (*type && !reverbReady)
        {
            DBG("processing currentRev");
            currentRev->process(buffer, amt);
        }
        else if (*type && reverbReady && !fade.complete)
        {
            processingAudio = true;
            DBG("Processing crossfade");
            tmp1.makeCopyOf(buffer, true);
            tmp2.makeCopyOf(buffer, true);
            currentRev->process(tmp1, amt);
            newRev->process(tmp2, amt);
            fade.processWithState(tmp1, tmp2, jmin(jmin(buffer.getNumSamples(), tmp1.getNumSamples()), tmp2.getNumSamples()));
            buffer.makeCopyOf(tmp2, true);
            if (fade.complete)
            {
                DBG("fade complete");
                currentRev.swap(newRev);
                // newRev.reset(nullptr);
                processingAudio = false; // don't allow param changes to dispatch until crossfade from last change is done
                reverbReady = false;
            }
        }
    }
};