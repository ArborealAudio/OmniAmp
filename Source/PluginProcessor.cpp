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
    comp = apvts.getRawParameterValue("comp");
    hiGain = apvts.getRawParameterValue("hiGain");
    autoGain = apvts.getRawParameterValue("autoGain");

    apvts.addParameterListener("treble", this);
    apvts.addParameterListener("mid", this);
    apvts.addParameterListener("bass", this);
    apvts.addParameterListener("mode", this);
}

GammaAudioProcessor::~GammaAudioProcessor()
{
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
    lastSampleRate = sampleRate;

    dsp::ProcessSpec spec{ sampleRate, samplesPerBlock, getTotalNumInputChannels() };

    guitar.prepare(spec);
    bass.prepare(spec);
    channel.prepare(spec);
}

void GammaAudioProcessor::releaseResources()
{
    guitar.reset();
    bass.reset();
    channel.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool GammaAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
  #if JucePlugin_IsMidiEffect
    juce::ignoreUnused (layouts);
    return true;
  #else
    // This is the place where you check if the layout is supported.
    // In this template code we only support mono or stereo.
    // Some plugin hosts, such as certain GarageBand versions, will only
    // load plugins that support stereo bus layouts.
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::mono()
     && layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    // This checks if the input layout matches the output layout
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
    if (parameterID.contains("mode"))
        currentMode = (Mode)newValue;
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
}

void GammaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    switch (currentMode)
    {
    case Guitar:
        guitar.processBuffer(buffer);
        break;
    case Bass:
        bass.processBuffer(buffer);
        break;
    case Channel:
        channel.processBuffer(buffer);
        break;
    }
}

//==============================================================================
bool GammaAudioProcessor::hasEditor() const
{
    return true; // (change this to false if you choose to not supply an editor)
}

juce::AudioProcessorEditor* GammaAudioProcessor::createEditor()
{
    //return new GammaAudioProcessorEditor (*this);
    return new GenericAudioProcessorEditor(*this);
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

    params.emplace_back(std::make_unique<AudioParameterFloat>("inputGain", "Input Gain", 0.f, 1.f, 0.f));
    params.emplace_back(std::make_unique<AudioParameterFloat>("outputGain", "Output Gain", 0.f, 1.f, 0.f));
    params.emplace_back(std::make_unique<AudioParameterFloat>("comp", "Compression", 0.f, 1.f, 0.f));
    params.emplace_back(std::make_unique<AudioParameterBool>("hiGain", "High Gain", false));
    params.emplace_back(std::make_unique<AudioParameterFloat>("bass", "Bass", 0.f, 1.f, 0.5f));
    params.emplace_back(std::make_unique<AudioParameterFloat>("mid", "Mid", 0.f, 1.f, 0.5f));
    params.emplace_back(std::make_unique<AudioParameterFloat>("treble", "Treble", 0.f, 1.f, 0.5f));
    params.emplace_back(std::make_unique<AudioParameterBool>("autoGain", "Auto Gain", false));
    params.emplace_back(std::make_unique<AudioParameterChoice>("mode", "Mode", StringArray{ "Guitar", "Bass", "Channel" }, 0));

    return { params.begin(), params.end() };
}