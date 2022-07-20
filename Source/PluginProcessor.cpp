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
     : AudioProcessor (BusesProperties()
                     #if ! JucePlugin_IsMidiEffect
                      #if ! JucePlugin_IsSynth
                       .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                      #endif
                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)
                     #endif
                       ), apvts(*this, nullptr, "Parameters", createParams()),
                        guitar(apvts, meterSource), bass(apvts, meterSource), channel(apvts, meterSource),
                        cab(apvts, currentCab), room(25.0, 1.5)
#endif
{
    gain = apvts.getRawParameterValue("inputGain");
    outGain = apvts.getRawParameterValue("outputGain");
    hiGain = apvts.getRawParameterValue("hiGain");
    autoGain = apvts.getRawParameterValue("autoGain");
    hfEnhance = apvts.getRawParameterValue("hfEnhance");
    lfEnhance = apvts.getRawParameterValue("lfEnhance");

    apvts.addParameterListener("treble", this);
    apvts.addParameterListener("mid", this);
    apvts.addParameterListener("bass", this);
    apvts.addParameterListener("mode", this);
    apvts.addParameterListener("dist", this);
    apvts.addParameterListener("cabType", this);
}

GammaAudioProcessor::~GammaAudioProcessor()
{
    apvts.removeParameterListener("treble", this);
    apvts.removeParameterListener("mid", this);
    apvts.removeParameterListener("bass", this);
    apvts.removeParameterListener("mode", this);
    apvts.removeParameterListener("dist", this);
    apvts.removeParameterListener("cabType", this);
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
    return 0.0;
}

int GammaAudioProcessor::getNumPrograms()
{
    return 1;   // NB: some hosts don't cope very well if you tell them there are 0 programs,
                // so this should be at least 1, even if you're not really implementing programs.
}

int GammaAudioProcessor::getCurrentProgram()
{
    return 0;
}

void GammaAudioProcessor::setCurrentProgram (int index)
{
}

const juce::String GammaAudioProcessor::getProgramName (int index)
{
    return {};
}

void GammaAudioProcessor::changeProgramName (int index, const juce::String& newName)
{
}

//==============================================================================
void GammaAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    oversample.initProcessing(samplesPerBlock);

    lastSampleRate = sampleRate * oversample.getOversamplingFactor();

    dsp::ProcessSpec spec{ lastSampleRate, (uint32)samplesPerBlock * 4, (uint32)getTotalNumInputChannels() };

    guitar.prepare(spec);
    bass.prepare(spec);
    channel.prepare(spec);

    lfEnhancer.setType((Processors::LFEnhancer<double>::Mode)currentMode);
    lfEnhancer.prepare(spec);
    hfEnhancer.prepare(spec);

    cab.prepare(dsp::ProcessSpec{sampleRate, (uint32)samplesPerBlock, (uint32)getTotalNumInputChannels()});
    cab.setCabType((Processors::CabType)(apvts.getRawParameterValue("cabType")->load()));

    room.prepare(dsp::ProcessSpec{sampleRate, (uint32)samplesPerBlock, (uint32)getTotalNumInputChannels()});

    audioSource.prepare(dsp::ProcessSpec{sampleRate, (uint32)samplesPerBlock, (uint32)getTotalNumInputChannels()});

    doubleBuffer.setSize(getTotalNumInputChannels(), samplesPerBlock);

    simd.setInterleavedBlockSize(getTotalNumInputChannels(), samplesPerBlock);
}

void GammaAudioProcessor::releaseResources()
{
    guitar.reset();
    bass.reset();
    channel.reset();
    hfEnhancer.reset();
    lfEnhancer.reset();
    cab.reset();
    room.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool GammaAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

   #if ! JucePlugin_IsSynth
    if (layouts.getMainOutputChannels() > 2 || layouts.getMainInputChannels() > 2)
        return false;
   #endif

    return true;
  #endif
}
#endif

void GammaAudioProcessor::parameterChanged(const String& parameterID, float newValue)
{
    if (parameterID.contains("mode")) {
        currentMode = (Mode)newValue;
        lfEnhancer.setType((Processors::LFEnhancer<double>::Mode)newValue);
        lfEnhancer.updateFilters();
    }
    else if (parameterID.contains("bass"))
    {
        guitar.setToneControl(0, newValue);
        bass.setToneControl(0, newValue);
    }
    else if (parameterID.contains("mid"))
    {
        guitar.setToneControl(1, newValue);
        bass.setToneControl(1, newValue);
    }
    else if (parameterID.contains("treble"))
    {
        guitar.setToneControl(2, newValue);
        bass.setToneControl(2, newValue);
    }
    else if (parameterID.contains("dist"))
    {
        guitar.setDistParam(newValue);
        channel.setDistParam(newValue);
    }
    else if (parameterID.contains("cabType"))
        cab.setCabType((Processors::CabType)newValue);
}

void GammaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    doubleBuffer.makeCopyOf(buffer);

    dsp::AudioBlock<double> block(doubleBuffer);

    auto osBlock = oversample.processSamplesUp(block);

    switch (currentMode)
    {
    case Guitar:
        guitar.processBlock(osBlock);
        break;
    case Bass:
        bass.processBlock(osBlock);
        break;
    case Channel:
        channel.processBlock(osBlock);
        break;
    }

    if (*lfEnhance)
        lfEnhancer.processBlock(osBlock, (double)*lfEnhance);

    if (*hfEnhance)
        hfEnhancer.processBlock(osBlock, (double)*hfEnhance);

    oversample.processSamplesDown(block);

    setLatencySamples(oversample.getLatencyInSamples());

#if USE_SIMD
    auto simdBlock = simd.interleaveBlock(block);
    auto &&processBlock = simdBlock;
#else
    auto &&processBlock = block;
#endif

    if (currentMode != Mode::Channel && *apvts.getRawParameterValue("cabOn"))
        cab.processBlock(processBlock);

#if USE_SIMD
    block = simd.deinterleaveBlock(processBlock);
#endif

    if (*apvts.getRawParameterValue("roomOn"))
        room.process(doubleBuffer, *apvts.getRawParameterValue("roomAmt"));

    buffer.makeCopyOf(doubleBuffer);

    MessageManager::callAsync([=]()
                              { audioSource.getBufferRMS(buffer); });
}

//==============================================================================
bool GammaAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* GammaAudioProcessor::createEditor()
{
    return new GammaAudioProcessorEditor (*this);
    // return new GenericAudioProcessorEditor(*this);
}

//==============================================================================
void GammaAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    auto xml = state.createXml();
    copyXmlToBinary(*xml, destData);
}

void GammaAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml != nullptr)
        apvts.replaceState(ValueTree::fromXml(*xml));
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new GammaAudioProcessor();
}

AudioProcessorValueTreeState::ParameterLayout GammaAudioProcessor::createParams()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("inputGain", 1), "Input Gain", 0.f, 1.f, 0.f));
    params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("outputGain", 1), "Output Gain", 0.f, 1.f, 0.f));
    params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("comp", 1), "Compression", 0.f, 1.f, 0.f));
    params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("dist", 1), "Pedal Distortion", NormalisableRange<float>(0.f, 1.f, 0.01f, 2.f), 0.f));
    params.emplace_back(std::make_unique<AudioParameterBool>(ParameterID("hiGain", 1), "High Gain", false));
    params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("bass", 1), "Bass", NormalisableRange<float>(0.f, 1.f, 0.01f, 0.25f), 0.06f));
    params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("mid", 1), "Mid", 0.f, 1.f, 0.5f));
    params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("treble", 1), "Treble", 0.f, 1.f, 0.5f));
    params.emplace_back(std::make_unique<AudioParameterBool>(ParameterID("autoGain", 1), "Auto Gain", false));
    params.emplace_back(std::make_unique<AudioParameterChoice>(ParameterID("mode", 1), "Mode", StringArray{ "Guitar", "Bass", "Channel" }, 2));
    params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("hfEnhance", 1), "HF Enhancer", 0.f, 1.f, 0.f));
    params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("lfEnhance", 1), "LF Enhancer", 0.f, 1.f, 0.f));
    params.emplace_back(std::make_unique<AudioParameterBool>(ParameterID("cabOn", 1), "Cab On/Off", true));
    params.emplace_back(std::make_unique<AudioParameterChoice>(ParameterID("cabType", 1), "Cab Type", StringArray("2x12", "4x12", "8x10"), 0));
    params.emplace_back(std::make_unique<AudioParameterBool>(ParameterID("roomOn", 1), "Room On/Off", false));
    params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("roomAmt", 1), "Room Amount", 0.f, 1.f, 0.f));
    params.emplace_back(std::make_unique<AudioParameterBool>(ParameterID("seriesAP", 1), "Series AP", false));
    // for (auto i = 0; i < 6; ++i)
    // {
    //     auto index = String(i);
    //     params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("delay" + index, 1), "Delay " + index, 0.f, 1000.f, (float)(i * 100) + (i + 23)));
    // }
    //     params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("feedback" + index, 1), "Feedback " + index, 0.f, 1.f, 0.06f));
    // }
    // params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("hpFreq", 1), "HPFreq", 20.f, 600.f, 90.f));
    // params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("hpQ", 1), "HPQ", 0.1f, 5.f, 5.f));
    // params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("lp1Freq", 1), "LP1Freq", 200.f, 7500.f, 3500.f));
    // params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("lp1Q", 1), "LP1Q", 0.1f, 5.f, 0.7f));
    // params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("lp2Freq", 1), "LP2Freq", 200.f, 7500.f, 3500.f));
    // params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("lp2Q", 1), "LP2Q", 0.1f, 5.f, 1.5f));
    // params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("bpFreq", 1), "BPFreq", 20.f, 7500.f, 1700.f));
    // params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("bpQ", 1), "BPQ", 0.1f, 10.f, 2.f));

    return { params.begin(), params.end() };
}