/*
  ==============================================================================

    This file contains the basic framework code for a JUCE plugin processor.

  ==============================================================================
*/

#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
GammaAudioProcessor::GammaAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : AudioProcessor(BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
                         ),
      apvts(*this, nullptr, "Parameters", createParams()),
      guitar(apvts, meterSource), bass(apvts, meterSource), channel(apvts, meterSource),
      cab(apvts, currentCab)
#endif
{
    inGain = apvts.getRawParameterValue("inputGain");
    outGain = apvts.getRawParameterValue("outputGain");
    gate = apvts.getRawParameterValue("gate");
    hiGain = apvts.getRawParameterValue("hiGain");
    autoGain = apvts.getRawParameterValue("autoGain");
    hfEnhance = apvts.getRawParameterValue("hfEnhance");
    lfEnhance = apvts.getRawParameterValue("lfEnhance");

    apvts.addParameterListener("gate", this);
    apvts.addParameterListener("treble", this);
    apvts.addParameterListener("mid", this);
    apvts.addParameterListener("bass", this);
    apvts.addParameterListener("mode", this);
    apvts.addParameterListener("dist", this);
    apvts.addParameterListener("cabType", this);
    apvts.addParameterListener("reverbType", this);
}

GammaAudioProcessor::~GammaAudioProcessor()
{
    apvts.removeParameterListener("gate", this);
    apvts.removeParameterListener("treble", this);
    apvts.removeParameterListener("mid", this);
    apvts.removeParameterListener("bass", this);
    apvts.removeParameterListener("mode", this);
    apvts.removeParameterListener("dist", this);
    apvts.removeParameterListener("cabType", this);
    apvts.removeParameterListener("reverbType", this);
}

//==============================================================================
const juce::String GammaAudioProcessor::getName() const
{
    return JucePlugin_Name;
}

bool GammaAudioProcessor::acceptsMidi() const
{
#if JucePlugin_WantsMidiInput
    return true;
#else
    return false;
#endif
}

bool GammaAudioProcessor::producesMidi() const
{
#if JucePlugin_ProducesMidiOutput
    return true;
#else
    return false;
#endif
}

bool GammaAudioProcessor::isMidiEffect() const
{
#if JucePlugin_IsMidiEffect
    return true;
#else
    return false;
#endif
}

double GammaAudioProcessor::getTailLengthSeconds() const
{
    return 1.0;
}

int GammaAudioProcessor::getNumPrograms()
{
    return 1; // NB: some hosts don't cope very well if you tell them there are 0 programs,
              // so this should be at least 1, even if you're not really implementing programs.
}

int GammaAudioProcessor::getCurrentProgram()
{
    return 0;
}

void GammaAudioProcessor::setCurrentProgram(int index)
{
}

const juce::String GammaAudioProcessor::getProgramName(int index)
{
    return "Default";
}

void GammaAudioProcessor::changeProgramName(int index, const juce::String &newName)
{
}

//==============================================================================
void GammaAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    oversample.initProcessing(samplesPerBlock);

    lastSampleRate = sampleRate * oversample.getOversamplingFactor();

    dsp::ProcessSpec osSpec{lastSampleRate, static_cast<uint32>(samplesPerBlock * oversample.getOversamplingFactor()), (uint32)getTotalNumInputChannels()};
    dsp::ProcessSpec spec{sampleRate, (uint32)samplesPerBlock, (uint32)getTotalNumInputChannels()};

    gateProc.prepare(spec);
    gateProc.setAttack(100.0);
    gateProc.setRelease(500.0);
    gateProc.setRatio(4.0);
    gateProc.setThreshold(*gate);

    guitar.prepare(osSpec);
    bass.prepare(osSpec);
    channel.prepare(osSpec);

    guitar.setToneControl(0, std::pow(*apvts.getRawParameterValue("bass"), 3.f));
    guitar.setToneControl(1, *apvts.getRawParameterValue("mid"));
    guitar.setToneControl(2, *apvts.getRawParameterValue("treble"));

    bass.setToneControl(0, std::pow(*apvts.getRawParameterValue("bass"), 3.f));
    bass.setToneControl(1, *apvts.getRawParameterValue("mid"));
    bass.setToneControl(2, *apvts.getRawParameterValue("treble"));

    channel.setFilters(0, *apvts.getRawParameterValue("bass"));
    channel.setFilters(1, *apvts.getRawParameterValue("mid"));
    channel.setFilters(2, *apvts.getRawParameterValue("treble"));

    lfEnhancer.setMode((Processors::ProcessorType)currentMode);
    lfEnhancer.prepare(osSpec);
    hfEnhancer.prepare(osSpec);

    cab.prepare(spec);
    cab.setCabType((Processors::CabType)apvts.getRawParameterValue("cabType")->load());

    reverb.setDownsampleRatio(2);
    reverb.prepare(spec);
    reverb.changeRoomType((Processors::ReverbType)apvts.getRawParameterValue("reverbType")->load());

    audioSource.prepare(spec);

    doubleBuffer.setSize(2, samplesPerBlock);

    simd.setInterleavedBlockSize(spec.numChannels, samplesPerBlock);
}

void GammaAudioProcessor::releaseResources()
{
    guitar.reset();
    bass.reset();
    channel.reset();
    hfEnhancer.reset();
    lfEnhancer.reset();
    cab.reset();
    reverb.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool GammaAudioProcessor::isBusesLayoutSupported(const BusesLayout &layouts) const
{
    return (layouts.getMainOutputChannels() <= 2 &&
            layouts.getMainInputChannels() <= 2 &&
            layouts.getMainInputChannels() <= layouts.getMainOutputChannels());
}
#endif

void GammaAudioProcessor::parameterChanged(const String &parameterID, float newValue)
{
    if (parameterID.contains("mode"))
    {
        currentMode = (Mode)newValue;
        lfEnhancer.setMode((Processors::ProcessorType)currentMode);
        lfEnhancer.updateFilters();
    }
    else if (parameterID.contains("bass"))
    {
        auto logval = newValue * newValue * newValue;
        guitar.setToneControl(0, logval);
        bass.setToneControl(0, logval);
        channel.setFilters(0, newValue);
    }
    else if (parameterID.contains("mid"))
    {
        guitar.setToneControl(1, newValue);
        bass.setToneControl(1, newValue);
        channel.setFilters(1, newValue);
    }
    else if (parameterID.contains("treble"))
    {
        guitar.setToneControl(2, newValue);
        bass.setToneControl(2, newValue);
        channel.setFilters(2, newValue);
    }
    else if (parameterID.contains("dist"))
    {
        auto logval = std::tanh(3.0 * newValue);
        guitar.setDistParam(logval);
        bass.setDistParam(logval);
        channel.setDistParam(logval);
    }
    else if (parameterID.contains("cabType"))
        cab.setCabType((Processors::CabType)newValue);
    else if (parameterID.contains("reverbType"))
        reverb.changeRoomType((Processors::ReverbType)newValue);
    else if (parameterID.contains("gate"))
        gateProc.setThreshold(*gate);
}

void GammaAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer, juce::MidiBuffer &midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    doubleBuffer.makeCopyOf(buffer, true);

    processDoubleBuffer(doubleBuffer, totalNumInputChannels < 2);

    auto L = doubleBuffer.getReadPointer(0);
    auto outL = buffer.getWritePointer(0);
    for (auto i = 0; i < buffer.getNumSamples(); ++i)
        outL[i] = static_cast<float>(L[i]);

    if (totalNumOutputChannels > 1)
    {
        auto R = doubleBuffer.getReadPointer(1);
        auto outR = buffer.getWritePointer(1);
        if (totalNumInputChannels < totalNumOutputChannels)
        {
            for (auto i = 0; i < buffer.getNumSamples(); ++i)
                outR[i] = static_cast<float>(L[i]);
        }
        else
        {
            for (auto i = 0; i < buffer.getNumSamples(); ++i)
                outR[i] = static_cast<float>(R[i]);
        }
    }

    audioSource.getBufferRMS(buffer);
}

void GammaAudioProcessor::processBlock(juce::AudioBuffer<double> &buffer, juce::MidiBuffer &midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    processDoubleBuffer(buffer, totalNumInputChannels < 2);

    if (totalNumInputChannels < totalNumOutputChannels)
        buffer.copyFrom(1, 0, buffer.getReadPointer(0), buffer.getNumSamples());

    audioSource.getBufferRMS(buffer);
}

//==============================================================================
bool GammaAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor *GammaAudioProcessor::createEditor()
{
    return new GammaAudioProcessorEditor(*this);
}

//==============================================================================
void GammaAudioProcessor::getStateInformation(juce::MemoryBlock &destData)
{
    auto state = apvts.copyState();
    auto xml = state.createXml();
    copyXmlToBinary(*xml, destData);
}

void GammaAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml != nullptr)
        apvts.replaceState(ValueTree::fromXml(*xml));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new GammaAudioProcessor();
}

AudioProcessorValueTreeState::ParameterLayout GammaAudioProcessor::createParams()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("inputGain", 1), "Input Gain", -12.f, 12.f, 0.f));
    params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("outputGain", 1), "Output Gain", -12.f, 12.f, 0.f));
    params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("gate", 1), "Gate Thresh", -96.f, -20.f, -96.f));
    params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("preampGain", 1), "Preamp Gain", 0.f, 1.f, 0.f));
    params.emplace_back(std::make_unique<AudioParameterBool>(ParameterID("preampAutoGain", 1), "Preamp Auto Gain", false));
    params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("powerampGain", 1), "Power Amp Gain", 0.f, 1.f, 0.f));
    params.emplace_back(std::make_unique<AudioParameterBool>(ParameterID("powerampAutoGain", 1), "Power Amp Auto Gain", false));
    params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("comp", 1), "Compression", 0.f, 1.f, 0.f));
    params.emplace_back(std::make_unique<AudioParameterBool>(ParameterID("compLink", 1), "Compression Stereo Link", true));
    params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("dist", 1), "Pedal Distortion", 0.f, 1.f, 0.f));
    params.emplace_back(std::make_unique<AudioParameterBool>(ParameterID("hiGain", 1), "High Gain", false));
    params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("bass", 1), "Bass", 0.f, 1.f, 0.5f));
    params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("mid", 1), "Mid", 0.f, 1.f, 0.5f));
    params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("treble", 1), "Treble", 0.f, 1.f, 0.5f));
    params.emplace_back(std::make_unique<AudioParameterBool>(ParameterID("eqAutoGain", 1), "EQ Auto Gain", false));
    params.emplace_back(std::make_unique<AudioParameterChoice>(ParameterID("mode", 1), "Mode", StringArray{"Guitar", "Bass", "Channel"}, 2));
    params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("hfEnhance", 1), "HF Enhancer", 0.f, 1.f, 0.f));
    params.emplace_back(std::make_unique<AudioParameterBool>(ParameterID("hfEnhanceAuto", 1), "HF Enhancer Auto Gain", false));
    params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("lfEnhance", 1), "LF Enhancer", 0.f, 1.f, 0.f));
    params.emplace_back(std::make_unique<AudioParameterBool>(ParameterID("lfEnhanceAuto", 1), "LF Enhancer Auto Gain", false));
    params.emplace_back(std::make_unique<AudioParameterBool>(ParameterID("cabOn", 1), "Cab On/Off", false));
    params.emplace_back(std::make_unique<AudioParameterChoice>(ParameterID("cabType", 1), "Cab Type", StringArray("2x12", "4x12", "8x10"), 0));
    params.emplace_back(std::make_unique<AudioParameterChoice>(ParameterID("reverbType", 1), "Reverb Type", StringArray("Off", "Room", "Hall"), 0));
    params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("roomAmt", 1), "Reverb Amount", 0.f, 1.f, 0.f));
    // for (auto i = 0; i < 6; ++i)
    // {
    //     auto index = String(i);
    //     params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("delay" + index, 1), "Delay " + index, 0.f, 100.f, 40.f));
    // }
    // params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("hpFreq", 1), "HPFreq", 20.f, 600.f, 90.f));
    // params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("hpQ", 1), "HPQ", 0.1f, 5.f, 2.f));
    // params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("lp1Freq", 1), "LP1Freq", 200.f, 7500.f, 3500.f));
    // params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("lp1Q", 1), "LP1Q", 0.1f, 5.f, 0.7f));
    // params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("lp2Freq", 1), "LP2Freq", 200.f, 7500.f, 3500.f));
    // params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("lp2Q", 1), "LP2Q", 0.1f, 5.f, 1.5f));
    // params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("bpFreq", 1), "BPFreq", 20.f, 7500.f, 1700.f));
    // params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("bpQ", 1), "BPQ", 0.1f, 10.f, 2.f));

    return {params.begin(), params.end()};
}