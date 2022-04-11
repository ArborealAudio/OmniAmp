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
    kP(8.7f, 1.35f, 1460.f, 4500.f, 48.f, 12.f, 375.f)
#endif
{
    gain = apvts.getRawParameterValue("inputGain");
    outGain = apvts.getRawParameterValue("outputGain");
    p_comp = apvts.getRawParameterValue("comp");
    hiGain = apvts.getRawParameterValue("hiGain");
    cutoff = apvts.getRawParameterValue("cutoff");
    autoGain = apvts.getRawParameterValue("autoGain");
    mix = apvts.getRawParameterValue("mix");
    bass = apvts.getRawParameterValue("bass");
    mid = apvts.getRawParameterValue("mid");
    treb = apvts.getRawParameterValue("treble");

    apvts.addParameterListener("hiGain", this);
    apvts.addParameterListener("cutoff", this);
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

    comp.prepare(spec);

    for (auto& t : avTriode)
        t.prepare(spec);

    for (auto& t : toneStack)
        t.prepare(spec);

    for (auto& t : triodes)
        t.prepare(spec);

    pentodes.prepare(spec);

    gtrPre.prepare(spec);
}

void GammaAudioProcessor::releaseResources()
{
    comp.reset();

    for (auto& t : avTriode)
        t.reset();

    for (auto& t : toneStack)
        t.reset();

    gtrPre.reset();

    for (auto& t : triodes)
        t.reset();

    pentodes.reset();
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
    if (parameterID.contains("bass"))
    {
        for (auto& t : toneStack)
            t.setBass(newValue);
    }
    else if (parameterID.contains("treble"))
    {
        for (auto& t : toneStack)
            t.setTreble(newValue);
    }
    else if (parameterID.contains("mid"))
    {
        for (auto& t : toneStack)
            t.setMid(newValue);
    }
    else if (parameterID.contains("mode"))
        currentMode = newValue;
}

void GammaAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels  = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear (i, 0, buffer.getNumSamples());

    float gain_raw = pow(10.f, (*gain / 20.f));
    float out_raw = pow(10.f, (*outGain / 20.f));

    if (*p_comp > 0.f)
        comp.process(buffer, *p_comp);

    buffer.applyGain(gain_raw);

    avTriode[0].process(buffer, 0.5f, 1.f);

    if (currentMode == 0)
        gtrPre.process(buffer, *hiGain);

    avTriode[1].process(buffer, 0.5f, 1.f);
    avTriode[2].process(buffer, 0.5f, 1.f);

    toneStack[currentMode].process(buffer);

    if (*hiGain)
        avTriode[3].process(buffer, 2.f, 2.f);

    buffer.applyGain(out_raw);
    pentodes.processBufferClassB(buffer, 1.f, 1.f);

    if (*autoGain)
        buffer.applyGain(1.f / (gain_raw * out_raw));
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
    // You should use this method to store your parameters in the memory block.
    // You could do that either as raw data, or use the XML or ValueTree classes
    // as intermediaries to make it easy to save and load complex data.
}

void GammaAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    // You should use this method to restore your parameters from this memory block,
    // whose contents will have been created by the getStateInformation() call.
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


    params.emplace_back(std::make_unique<AudioParameterFloat>("inputGain", "Input Gain", -24.f, 18.f, 0.f));
    params.emplace_back(std::make_unique<AudioParameterFloat>("outputGain", "Output Gain", -24.f, 24.f, 0.f));
    params.emplace_back(std::make_unique<AudioParameterFloat>("comp", "Compression", 0.f, 1.f, 0.f));
    params.emplace_back(std::make_unique<AudioParameterBool>("hiGain", "High Gain", false));
    params.emplace_back(std::make_unique<AudioParameterFloat>("bass", "Bass", 0.f, 1.f, 0.5f));
    params.emplace_back(std::make_unique<AudioParameterFloat>("mid", "Mid", 0.f, 1.f, 0.5f));
    params.emplace_back(std::make_unique<AudioParameterFloat>("treble", "Treble", 0.f, 1.f, 0.5f));
    params.emplace_back(std::make_unique<AudioParameterBool>("autoGain", "Auto Gain", false));
    params.emplace_back(std::make_unique<AudioParameterChoice>("mode", "Mode", StringArray{ "Guitar", "Bass", "Flat" }, 0));

    return { params.begin(), params.end() };
}