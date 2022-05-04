/*
  ==============================================================================

    Processors.h
    Created: 11 Apr 2022 10:20:37am
    Author:  alexb

  ==============================================================================
*/

#pragma once

struct Guitar
{
    Guitar(AudioProcessorValueTreeState& a) : apvts(a)
    {
        inGain = apvts.getRawParameterValue("inputGain");
        outGain = apvts.getRawParameterValue("outputGain");
        p_comp = apvts.getRawParameterValue("comp");
        hiGain = apvts.getRawParameterValue("hiGain");
    }

    void prepare(const dsp::ProcessSpec& spec)
    {
        comp.prepare(spec);
        
        for (auto& t : avTriode)
            t.prepare(spec);

        toneStack.prepare(spec);
        gtrPre.prepare(spec);

        pentodes.prepare(spec);
    }

    void reset()
    {
        comp.reset();

        for (auto& t : avTriode)
            t.reset();

        gtrPre.reset();
        toneStack.reset();

        pentodes.reset();
    }

    /*0 = bass | 1 = mid | 2 = treble*/
    void setToneControl(int control, float newValue)
    {
        switch (control) {
        case 0:
            toneStack.setBass(newValue);
            break;
        case 1:
            toneStack.setMid(newValue);
            break;
        case 2:
            toneStack.setTreble(newValue);
            break;
        }
    }

    void processBuffer(AudioBuffer<float>& buffer)
    {
        float gain_raw = jmap(inGain->load(), 1.f, 8.f);
        float out_raw = jmap(outGain->load(), 1.f, 8.f);

        if (*p_comp > 0.f)
            comp.process(buffer, *p_comp);

        buffer.applyGain(gain_raw);

        avTriode[0].process(buffer, 0.5f, 1.f);

        gtrPre.process(buffer, *hiGain);

        avTriode[1].process(buffer, 0.5f, 1.f);
        avTriode[2].process(buffer, 0.5f, 1.f);

        toneStack.process(buffer);

        if (*hiGain)
            avTriode[3].process(buffer, 2.f, 2.f);

        buffer.applyGain(out_raw);
        pentodes.processBufferClassB(buffer, 1.f, 1.f);
    }
private:
    AudioProcessorValueTreeState& apvts;

    OptoComp comp{ OptoComp::Type::Guitar };
    GuitarPreFilter gtrPre;
    ToneStackNodal toneStack{ 0.25e-9f, 25e-9f, 22e-9f, 300e3f, 0.25e6f, 20e3f, 65e3f };
    std::array<AVTriode, 4> avTriode;
    Tube pentodes;

    std::atomic<float>* inGain, *outGain, *hiGain, *p_comp;
};

struct Bass
{
    Bass(AudioProcessorValueTreeState& a) : apvts(a)
    {
        inGain = apvts.getRawParameterValue("inputGain");
        outGain = apvts.getRawParameterValue("outputGain");
        p_comp = apvts.getRawParameterValue("comp");
        hiGain = apvts.getRawParameterValue("hiGain");
    }

    void prepare(const dsp::ProcessSpec& spec)
    {
        comp.prepare(spec);

        for (auto& t : avTriode)
            t.prepare(spec);

        toneStack.prepare(spec);

        pentodes.prepare(spec);
    }

    void reset()
    {
        for (auto& a : avTriode)
            a.reset();
        
        toneStack.reset();
        
        pentodes.reset();
    }

    /*0 = bass | 1 = mid | 2 = treble*/
    void setToneControl(int control, float newValue)
    {
        switch (control) {
        case 0:
            toneStack.setBass(newValue);
            break;
        case 1:
            toneStack.setMid(newValue);
            break;
        case 2:
            toneStack.setTreble(newValue);
            break;
        }
    }

    void processBuffer(AudioBuffer<float>& buffer)
    {
        float gain_raw = jmap(inGain->load(), 1.f, 8.f);
        float out_raw = jmap(outGain->load(), 1.f, 8.f);

        if (*p_comp > 0.f)
            comp.process(buffer, *p_comp);

        avTriode[0].process(buffer, 0.5, 1.0);

        buffer.applyGain(gain_raw);
        
        avTriode[1].process(buffer, 0.5, 1.0);
        if (*hiGain) {
            avTriode[2].process(buffer, 1.0, 1.5);
            avTriode[3].process(buffer, 1.0, 1.5);
        }

        toneStack.process(buffer);

        buffer.applyGain(out_raw);

        pentodes.processBufferClassB(buffer, 1.f, 1.f);
    }

private:

    AudioProcessorValueTreeState& apvts;

    std::array<AVTriode, 4> avTriode;
    ToneStackNodal toneStack{ 0.25e-9f, 22e-9f, 22e-9f, 300e3f, 0.5e6f, 30e3f, 56e3f };
    OptoComp comp {OptoComp::Type::Bass};
    Tube pentodes;

    std::atomic<float>* inGain, *outGain, *p_comp, *hiGain;
};

struct Channel
{
    Channel(AudioProcessorValueTreeState& a) : apvts(a)
    {
        inGain = apvts.getRawParameterValue("inputGain");
        outGain = apvts.getRawParameterValue("outputGain");
        p_comp = apvts.getRawParameterValue("comp");
        hiGain = apvts.getRawParameterValue("hiGain");
    }

    void prepare(const dsp::ProcessSpec& spec)
    {
        comp.prepare(spec);

        for (auto& t : avTriode)
            t.prepare(spec);

        pentodes.prepare(spec);
    }

    void reset()
    {
        comp.reset();

        for (auto& t : avTriode)
            t.reset();

        pentodes.reset();
    }

    void processBuffer(AudioBuffer<float>& buffer)
    {
        float gain_raw = jmap(inGain->load(), 1.f, 4.f);
        float out_raw = jmap(outGain->load(), 1.f, 4.f);

        if (*p_comp > 0.f)
            comp.process(buffer, *p_comp);

        buffer.applyGain(gain_raw);

        if (*inGain > 0.f) {
            avTriode[0].process(buffer, *inGain, 2.f * *inGain);
            if (*hiGain)
                avTriode[1].process(buffer, *inGain, 2.f);
        }

        buffer.applyGain(out_raw);

        if (*outGain > 0.f)
            pentodes.processBufferClassB(buffer, 1.f, 1.f);
    }

private:
    AudioProcessorValueTreeState& apvts;

    OptoComp comp{ OptoComp::Type::Channel };
    std::array<AVTriode, 2> avTriode;
    Tube pentodes;

    std::atomic<float>* inGain, *outGain, *p_comp, *hiGain;
};