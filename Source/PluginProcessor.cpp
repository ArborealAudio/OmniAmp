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
    : AudioProcessor(
          BusesProperties()
#if !JucePlugin_IsMidiEffect
#if !JucePlugin_IsSynth
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
#endif
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)
#endif
              ),
      apvts(*this, nullptr, "Parameters", createParams()),
      guitar(apvts, meterSource), bass(apvts, meterSource),
      channel(apvts, meterSource),
      cab(apvts,
          (Processors::CabType)apvts.getRawParameterValue("cabType")->load()),
      hfEnhancer(apvts), lfEnhancer(apvts), reverb(apvts),
      emphLow(apvts, (strix::FloatParameter *)apvts.getParameter("lfEmphasis"),
              (strix::FloatParameter *)apvts.getParameter("lfEmphasisFreq")),
      emphHigh(apvts, (strix::FloatParameter *)apvts.getParameter("hfEmphasis"),
               (strix::FloatParameter *)apvts.getParameter("hfEmphasisFreq")),
      cutFilters(apvts)
#endif
{
    inGain = apvts.getRawParameterValue("inputGain");
    outGain = apvts.getRawParameterValue("outputGain");
    // gate = apvts.getRawParameterValue("gate");
    hiGain = apvts.getRawParameterValue("hiGain");
    autoGain = apvts.getRawParameterValue("autoGain");
    hfEnhance = apvts.getRawParameterValue("hfEnhance");
    lfEnhance = apvts.getRawParameterValue("lfEnhance");

    apvts.addParameterListener("gainLink", this);
    // // apvts.addParameterListener("gate", this);
    apvts.addParameterListener("treble", this);
    apvts.addParameterListener("mid", this);
    apvts.addParameterListener("bass", this);
    apvts.addParameterListener("mode", this);
    apvts.addParameterListener("dist", this);
    apvts.addParameterListener("hq", this);

    LookAndFeel::getDefaultLookAndFeel().setDefaultSansSerifTypeface(
        getCustomFont());

    checkLicense();
}

GammaAudioProcessor::~GammaAudioProcessor()
{
    apvts.removeParameterListener("gainLink", this);
    // // apvts.removeParameterListener("gate", this);
    apvts.removeParameterListener("treble", this);
    apvts.removeParameterListener("mid", this);
    apvts.removeParameterListener("bass", this);
    apvts.removeParameterListener("mode", this);
    apvts.removeParameterListener("dist", this);
    apvts.removeParameterListener("hq", this);
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

double GammaAudioProcessor::getTailLengthSeconds() const { return 0.0; }

int GammaAudioProcessor::getNumPrograms()
{
    return 1; // NB: some hosts don't cope very well if you tell them there are
              // 0 programs, so this should be at least 1, even if you're not
              // really implementing programs.
}

int GammaAudioProcessor::getCurrentProgram() { return 0; }

void GammaAudioProcessor::setCurrentProgram(int index) {}

const juce::String GammaAudioProcessor::getProgramName(int index)
{
    return "Default";
}

void GammaAudioProcessor::changeProgramName(int index,
                                            const juce::String &newName)
{
}

//==============================================================================
void GammaAudioProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    for (auto &o : oversample)
        o.initProcessing(samplesPerBlock);

    setOversampleIndex();
    int ovs_fac = oversample[os_index].getOversamplingFactor();

    maxBlockSize = samplesPerBlock;
    SR = sampleRate;
    lastSampleRate = sampleRate * ovs_fac;

    dsp::ProcessSpec osSpec{lastSampleRate,
                            static_cast<uint32>(samplesPerBlock * ovs_fac),
                            (uint32)getTotalNumOutputChannels()};
    dsp::ProcessSpec spec{sampleRate, (uint32)samplesPerBlock,
                          (uint32)getTotalNumOutputChannels()};

    currentMode = (Mode)apvts.getRawParameterValue("mode")->load();

    emphLow.prepare(spec);
    emphHigh.prepare(spec);

    guitar.prepare(osSpec);
    bass.prepare(osSpec);
    channel.prepare(osSpec);

    guitar.setToneControl(0,
                          calcBassParam(*apvts.getRawParameterValue("bass")));
    guitar.setToneControl(1, *apvts.getRawParameterValue("mid"));
    guitar.setToneControl(2, *apvts.getRawParameterValue("treble"));

    bass.setToneControl(0, calcBassParam(*apvts.getRawParameterValue("bass")));
    bass.setToneControl(1, *apvts.getRawParameterValue("mid"));
    bass.setToneControl(2, *apvts.getRawParameterValue("treble"));

    channel.setFilters(0, *apvts.getRawParameterValue("bass"));
    channel.setFilters(1, *apvts.getRawParameterValue("mid"));
    channel.setFilters(2, *apvts.getRawParameterValue("treble"));

    lfEnhancer.setMode((Processors::ProcessorType)currentMode);
    hfEnhancer.setMode((Processors::ProcessorType)currentMode);
    lfEnhancer.prepare(spec);
    hfEnhancer.prepare(spec);
    cutFilters.prepare(spec);

    cab.prepare(spec);

    reverb.prepare(spec);

    mixDelay.prepare(spec);
    mixDelay.setMaximumDelayInSamples(spec.maximumBlockSize + 256);
    sm_mix.reset(spec.sampleRate, 0.01f);
    dryDelay.prepare(spec);
    dryDelay.setMaximumDelayInSamples(spec.maximumBlockSize + 256);

    doubler.prepare(spec);
    doubler.setDelayTime(18);

    doubleBuffer.setSize(2, samplesPerBlock);
    preAmpBuf.setSize(spec.numChannels, samplesPerBlock);
    preAmpCrossfade.setFadeTime(spec.sampleRate, 0.1f);

    simd.setInterleavedBlockSize(spec.numChannels, samplesPerBlock);
}

void GammaAudioProcessor::releaseResources()
{
    guitar.reset();
    bass.reset();
    channel.reset();
    hfEnhancer.reset();
    lfEnhancer.reset();
    cutFilters.reset();
    emphLow.reset();
    emphHigh.reset();
    cab.reset();
    reverb.reset();
    emphasisIn.reset();
    emphasisOut.reset();
    doubler.reset();
    mixDelay.reset();
}

#ifndef JucePlugin_PreferredChannelConfigurations
bool GammaAudioProcessor::isBusesLayoutSupported(
    const BusesLayout &layouts) const
{
    return (layouts.getMainOutputChannels() <= 2 &&
            layouts.getMainInputChannels() <= 2 &&
            layouts.getMainInputChannels() <= layouts.getMainOutputChannels());
}
#endif

void GammaAudioProcessor::parameterChanged(const String &parameterID,
                                           float newValue)
{
    if (parameterID == "mode") {
        currentMode = (Mode)newValue;
        lfEnhancer.setMode((Processors::ProcessorType)currentMode);
        hfEnhancer.setMode((Processors::ProcessorType)currentMode);
        lfEnhancer.flagUpdate(true);
    } else if (parameterID == "bass") {
        auto adjVal = calcBassParam(newValue);
        guitar.setToneControl(0, adjVal);
        bass.setToneControl(0, adjVal);
        channel.sm_low.setTargetValue(newValue);
    } else if (parameterID == "mid") {
        guitar.setToneControl(1, newValue);
        bass.setToneControl(1, newValue);
        channel.sm_mid.setTargetValue(newValue);
    } else if (parameterID == "treble") {
        guitar.setToneControl(2, newValue);
        bass.setToneControl(2, newValue);
        channel.sm_hi.setTargetValue(newValue);
    } else if (parameterID == "dist") {
        auto logval = std::tanh(3.f * newValue);
        // auto map = [](float x)
        // { return (std::pow(0.1f, x) - 1.f) / -0.9f; };
        // auto logval = std::sqrt(newValue);
        guitar.setDistParam(logval);
        bass.setDistParam(logval);
        channel.setDistParam(logval);
    }
    // else if (parameterID == "gate")
    // gateProc.setThreshold(*gate);
    else if (parameterID == "gainLink") {
        if ((bool)newValue) {
            /*if we switched Link on, output = input + output*/
            auto adjustedGain = *inGain + *outGain;
            auto outGain_p = apvts.getParameter("outputGain");
            outGain_p->setValueNotifyingHost(
                outGain_p->convertTo0to1(adjustedGain));
            // apvts.getParameterAsValue("outputGain").setValue(apvts.getRawParameterValue("outputGain")->load()
            // + apvts.getRawParameterValue("inputGain")->load());
        } else {
            /*if we switched Link off, output = output - input*/
            auto adjustedGain = *outGain - *inGain;
            auto outGain_p = apvts.getParameter("outputGain");
            outGain_p->setValueNotifyingHost(
                outGain_p->convertTo0to1(adjustedGain));
            // apvts.getParameterAsValue("outputGain").setValue(apvts.getRawParameterValue("outputGain")->load()
            // - apvts.getRawParameterValue("inputGain")->load());
        }
    } else if (parameterID == "hq") {
        setOversampleIndex();
        auto ovs_fac = oversample[os_index].getOversamplingFactor();
        lastSampleRate = SR * ovs_fac;
        dsp::ProcessSpec osSpec{lastSampleRate, uint32(maxBlockSize * ovs_fac),
                                (uint32)getTotalNumInputChannels()};
        suspendProcessing(true);
        guitar.prepare(osSpec);
        bass.prepare(osSpec);
        channel.update(osSpec, *apvts.getRawParameterValue("bass"),
                       *apvts.getRawParameterValue("mid"),
                       *apvts.getRawParameterValue("treble"));
        suspendProcessing(false);
    }
}

void GammaAudioProcessor::setOversampleIndex()
{
    if (*apvts.getRawParameterValue("hq") ||
        (isNonRealtime() && *apvts.getRawParameterValue("renderHQ")))
        os_index = 1;
    else
        os_index = 0;
}

void GammaAudioProcessor::processBlock(juce::AudioBuffer<float> &buffer,
                                       juce::MidiBuffer &)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    doubleBuffer.makeCopyOf(buffer, true);

    if (totalNumInputChannels < totalNumOutputChannels)
        doubleBuffer.copyFrom(1, 0, doubleBuffer.getReadPointer(0),
                              doubleBuffer.getNumSamples());

    processDoubleBuffer(doubleBuffer, totalNumOutputChannels < 2);

    auto L = doubleBuffer.getReadPointer(0);
    auto outL = buffer.getWritePointer(0);
    for (auto i = 0; i < buffer.getNumSamples(); ++i)
        outL[i] = static_cast<float>(L[i]);

    if (totalNumOutputChannels > 1) {
        auto R = doubleBuffer.getReadPointer(1);
        auto outR = buffer.getWritePointer(1);
        for (auto i = 0; i < buffer.getNumSamples(); ++i)
            outR[i] = static_cast<float>(R[i]);
    }
}

void GammaAudioProcessor::processBlock(juce::AudioBuffer<double> &buffer,
                                       juce::MidiBuffer &midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    auto totalNumInputChannels = getTotalNumInputChannels();
    auto totalNumOutputChannels = getTotalNumOutputChannels();

    for (auto i = totalNumInputChannels; i < totalNumOutputChannels; ++i)
        buffer.clear(i, 0, buffer.getNumSamples());

    if (totalNumInputChannels < totalNumOutputChannels)
        buffer.copyFrom(1, 0, buffer.getReadPointer(0), buffer.getNumSamples());

    processDoubleBuffer(buffer, totalNumOutputChannels < 2);
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
    xml->setAttribute("Preset", currentPreset);
    copyXmlToBinary(*xml, destData);
}

void GammaAudioProcessor::setStateInformation(const void *data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml != nullptr) {
        apvts.replaceState(ValueTree::fromXml(*xml));
        currentPreset = xml->getStringAttribute("Preset");
    }
}

//==============================================================================
// This creates new instances of the plugin..
juce::AudioProcessor *JUCE_CALLTYPE createPluginFilter()
{
    return new GammaAudioProcessor();
}

AudioProcessorValueTreeState::ParameterLayout
GammaAudioProcessor::createParams()
{
    std::vector<std::unique_ptr<RangedAudioParameter>> params;

    NormalisableRange<float> resoRange(0.5f, 2.f, 0.01f);
    resoRange.setSkewForCentre(1.f);

    using fParam = strix::FloatParameter;
    using bParam = strix::BoolParameter;
    using cParam = strix::ChoiceParameter;

    params.emplace_back(std::make_unique<fParam>(
        ParameterID("inputGain", 1), "Input Gain", -12.f, 12.f, 0.f));
    params.emplace_back(std::make_unique<fParam>(
        ParameterID("outputGain", 1), "Output Gain", -12.f, 12.f, 0.f));
    params.emplace_back(std::make_unique<bParam>(ParameterID("gainLink", 1),
                                                 "Gain Link", false));
    // params.emplace_back(std::make_unique<fParam>(ParameterID("gate", 1),
    // "Gate Thresh", -96.f, -20.f, -96.f));
    params.emplace_back(
        std::make_unique<bParam>(ParameterID("m/s", 1), "Stereo/MS", false));
    params.emplace_back(std::make_unique<fParam>(ParameterID("width", 1),
                                                 "Width", 0.f, 2.f, 1.f));
    params.emplace_back(std::make_unique<fParam>(
        ParameterID("stereoEmphasis", 1), "Stereo Emphasis", 0.f, 1.f, 0.5f));
    params.emplace_back(std::make_unique<fParam>(ParameterID("doubler", 1),
                                                 "Doubler", 0.f, 1.f, 0.f));
    params.emplace_back(
        std::make_unique<fParam>(ParameterID("mix", 1), "Mix", 0.f, 1.f, 1.f));
    params.emplace_back(std::make_unique<fParam>(
        ParameterID("lfEmphasis", 1), "LF Emphasis", -12.f, 12.f, 0.f));
    params.emplace_back(
        std::make_unique<fParam>(ParameterID("lfEmphasisFreq", 1),
                                 "LF Emphasis Freq", 30.f, 1800.f, 200.f));
    params.emplace_back(std::make_unique<fParam>(
        ParameterID("hfEmphasis", 1), "HF Emphasis", -12.f, 12.f, 0.f));
    params.emplace_back(
        std::make_unique<fParam>(ParameterID("hfEmphasisFreq", 1),
                                 "HF Emphasis Freq", 1800.f, 18000.f, 5000.f));
    params.emplace_back(std::make_unique<fParam>(ParameterID("comp", 1),
                                                 "Compression", 0.f, 1.f, 0.f));
    params.emplace_back(std::make_unique<bParam>(ParameterID("compLink", 1),
                                                 "Comp Stereo Link", true));
    params.emplace_back(std::make_unique<bParam>(ParameterID("compPos", 1),
                                                 "Comp Pre/Post", false));
    params.emplace_back(std::make_unique<fParam>(
        ParameterID("dist", 1), "Pedal Distortion", 0.f, 1.f, 0.f));
    params.emplace_back(
        std::make_unique<cParam>(ParameterID("mode", 1), "Mode",
                                 StringArray{"Guitar", "Bass", "Channel"}, 2));
    params.emplace_back(std::make_unique<cParam>(
        ParameterID("guitarMode", 1), "Guitar Amp Type",
        StringArray{"GammaRay", "Sunbeam", "Moonbeam", "XRay"}, 0));
    params.emplace_back(std::make_unique<cParam>(
        ParameterID("bassMode", 1), "Bass Amp Type",
        StringArray{"Cobalt", "Emerald", "Quartz"}, 0));
    params.emplace_back(
        std::make_unique<cParam>(ParameterID("channelMode", 1), "Channel Strip",
                                 StringArray{"Modern", "Vintage", "Biamp"}, 0));
    params.emplace_back(
        std::make_unique<bParam>(ParameterID("ampOn", 1), "Amp On/Off", true));
    params.emplace_back(std::make_unique<bParam>(ParameterID("ampAutoGain", 1),
                                                 "Amp Auto Gain", false));
    params.emplace_back(std::make_unique<fParam>(ParameterID("preampGain", 1),
                                                 "Preamp Gain", 0.f, 1.f, 0.f));
    params.emplace_back(std::make_unique<fParam>(
        ParameterID("powerampGain", 1), "Power Amp Gain", 0.f, 1.f, 0.f));
    params.emplace_back(
        std::make_unique<bParam>(ParameterID("hiGain", 1), "High Gain", false));
    params.emplace_back(std::make_unique<fParam>(ParameterID("bass", 1), "Bass",
                                                 0.f, 1.f, 0.5f));
    params.emplace_back(
        std::make_unique<fParam>(ParameterID("mid", 1), "Mid", 0.f, 1.f, 0.5f));
    params.emplace_back(std::make_unique<fParam>(ParameterID("treble", 1),
                                                 "Treble", 0.f, 1.f, 0.5f));
    params.emplace_back(std::make_unique<fParam>(ParameterID("hfEnhance", 1),
                                                 "HF Enhancer", 0.f, 1.f, 0.f));
    params.emplace_back(std::make_unique<bParam>(
        ParameterID("hfEnhanceAuto", 1), "HF Enhancer Auto Gain", false));
    params.emplace_back(std::make_unique<bParam>(
        ParameterID("hfEnhanceInvert", 1), "HF Enhancer Invert", false));
    params.emplace_back(std::make_unique<fParam>(ParameterID("lfEnhance", 1),
                                                 "LF Enhancer", 0.f, 1.f, 0.f));
    params.emplace_back(std::make_unique<bParam>(
        ParameterID("lfEnhanceAuto", 1), "LF Enhancer Auto Gain", false));
    params.emplace_back(std::make_unique<bParam>(
        ParameterID("lfEnhanceInvert", 1), "LF Enhancer Invert", false));
    params.emplace_back(std::make_unique<fParam>(ParameterID("lfCut", 1),
                                                 "LF Cut", 5.f, 1000.0, 5.f));
    params.emplace_back(std::make_unique<fParam>(
        ParameterID("hfCut", 1), "HF Cut", 1500.f, 22000.0, 22000.f));
    params.emplace_back(std::make_unique<cParam>(
        ParameterID("cabType", 1), "Cab Type",
        StringArray("Off", "2x12", "4x12", "6x10"), 0));
    params.emplace_back(std::make_unique<fParam>(
        ParameterID("cabMicPosX", 1), "Cab Mic Pos", 0.f, 1.f, 0.5f));
    params.emplace_back(std::make_unique<fParam>(
        ParameterID("cabMicPosZ", 1), "Cab Mic Depth", 0.f, 1.f, 0.5f));
    params.emplace_back(std::make_unique<fParam>(
        ParameterID("cabResoLo", 1), "Cab Low Reso", resoRange, 1.f));
    params.emplace_back(std::make_unique<fParam>(
        ParameterID("cabResoHi", 1), "Cab Hi Reso", resoRange, 1.f));
    params.emplace_back(
        std::make_unique<cParam>(ParameterID("reverbType", 1), "Reverb Type",
                                 StringArray("Off", "Room", "Hall"), 0));
    params.emplace_back(std::make_unique<bParam>(ParameterID("reverbBright", 1),
                                                 "Reverb Bright", false));
    params.emplace_back(std::make_unique<fParam>(
        ParameterID("reverbAmt", 1), "Reverb Amount", 0.f, 1.f, 0.f));
    params.emplace_back(std::make_unique<fParam>(
        ParameterID("reverbDecay", 1), "Reverb Decay", 0.5f, 2.f, 1.f));
    params.emplace_back(std::make_unique<fParam>(ParameterID("reverbSize", 1),
                                                 "Reverb Size", 0.f, 2.f, 1.f));
    params.emplace_back(std::make_unique<fParam>(
        ParameterID("reverbPredelay", 1), "Reverb Predelay", 0.f, 200.f, 0.f));
    params.emplace_back(
        std::make_unique<bParam>(ParameterID("hq", 1), "HQ On/Off", false));
    params.emplace_back(std::make_unique<bParam>(ParameterID("renderHQ", 1),
                                                 "Render HQ", true));
    params.emplace_back(
        std::make_unique<bParam>(ParameterID("bypass", 1), "Bypass", false));

    return {params.begin(), params.end()};
}
