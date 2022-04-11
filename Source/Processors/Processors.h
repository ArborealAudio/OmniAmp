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

    ~Guitar()
    {}

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
        float gain_raw = pow(10.f, (*inGain / 20.f));
        float out_raw = pow(10.f, (*outGain / 20.f));

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

    OptoComp comp;
    GuitarPreFilter gtrPre;
    ToneStackNodal toneStack{ 0.25e-9f, 25e-9f, 22e-9f, 300e3f, 0.25e6f, 20e3f, 65e3f };
    std::array<AVTriode, 4> avTriode;
    Tube pentodes;

    std::atomic<float>* inGain, *outGain, *hiGain, *p_comp;
};

struct Channel
{
    Channel() = default;

    void prepare(const dsp::ProcessSpec& spec)
    {

    }

    void reset()
    {

    }

    void processBuffer(AudioBuffer<float>& buffer)
    {

    }

private:
    OptoComp comp;
    std::array<AVTriode, 4> avTriode;
    Tube pentodes;
};