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
                        guitar(apvts), bass(apvts), channel(apvts)
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
}

GammaAudioProcessor::~GammaAudioProcessor()
{
    apvts.removeParameterListener("treble", this);
    apvts.removeParameterListener("mid", this);
    apvts.removeParameterListener("bass", this);
    apvts.removeParameterListener("mode", this);
    apvts.removeParameterListener("dist", this);
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

    dsp::ProcessSpec spec{ sampleRate * oversample.getOversamplingFactor(), (uint32)samplesPerBlock * 4, (uint32)getTotalNumInputChannels() };

    guitar.prepare(spec);
    bass.prepare(spec);
    channel.prepare(spec);

    lfEnhancer.setType((LFEnhancer::Mode)currentMode);
    lfEnhancer.prepare(spec);
    hfEnhancer.prepare(spec);

    audioSource.prepare(dsp::ProcessSpec{sampleRate, (uint32)samplesPerBlock, (uint32)getTotalNumInputChannels()});
}

void GammaAudioProcessor::releaseResources()
{
    guitar.reset();
    bass.reset();
    channel.reset();
    hfEnhancer.reset();
    lfEnhancer.reset();
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
    if (layouts.getMainOutputChannelSet() != layouts.getMainInputChannelSet())
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
        lfEnhancer.setType((LFEnhancer::Mode)currentMode);
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
}

void GammaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    dsp::AudioBlock<float> block(buffer);

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
        lfEnhancer.processBlock(osBlock, *lfEnhance);

    if (*hfEnhance)
        hfEnhancer.processBlock(osBlock, *hfEnhance);

    oversample.processSamplesDown(block);

    setLatencySamples(oversample.getLatencyInSamples());

    // audioSource.copyBuffer(buffer);
    audioSource.getBufferRMS(buffer);
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
    params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("dist", 1), "Pedal Distortion", 0.f, 1.f, 0.f));
    params.emplace_back(std::make_unique<AudioParameterBool>(ParameterID("hiGain", 1), "High Gain", false));
    params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("bass", 1), "Bass", 0.f, 1.f, 0.5f));
    params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("mid", 1), "Mid", 0.f, 1.f, 0.5f));
    params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("treble", 1), "Treble", 0.f, 1.f, 0.5f));
    params.emplace_back(std::make_unique<AudioParameterBool>(ParameterID("autoGain", 1), "Auto Gain", false));
    params.emplace_back(std::make_unique<AudioParameterChoice>(ParameterID("mode", 1), "Mode", StringArray{ "Guitar", "Bass", "Channel" }, 0));
    params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("hfEnhance", 1), "HF Enhancer", 0.f, 1.f, 0.f));
    params.emplace_back(std::make_unique<AudioParameterFloat>(ParameterID("lfEnhance", 1), "LF Enhancer", 0.f, 1.f, 0.f));

    return { params.begin(), params.end() };
}