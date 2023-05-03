/*
  ==============================================================================

    PresetManager.h

  ==============================================================================
*/

#pragma once

#if JUCE_MAC
    #define PRESET_PATH "/Library/Application Support/Arboreal Audio/OmniAmp/Presets"
#elif JUCE_WINDOWS
    #define PRESET_PATH File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName() + "/Arboreal Audio/OmniAmp/Presets"
#endif

#include <JuceHeader.h>

struct PresetManager : private AudioProcessorValueTreeState::Listener
{
    PresetManager(AudioProcessorValueTreeState& vts) : apvts(vts)
    {
        if (!userDir.isDirectory())
            userDir.createDirectory();

        for (auto* param : apvts.processor.getParameters())
        {
            if (const auto p = dynamic_cast<RangedAudioParameter*>(param))
                if (p->paramID != "hq" && p->paramID != "renderHQ")
                    apvts.addParameterListener(p->paramID, this);
        }
    }

    ~PresetManager()
    {
        for (auto* param : apvts.processor.getParameters())
        {
            if (const auto p = dynamic_cast<RangedAudioParameter*>(param))
                if (p->paramID != "hq" && p->paramID != "renderHQ")
                    apvts.removeParameterListener(p->paramID, this);
        }
    }

    void parameterChanged(const String&, float)
    {
        if (!stateChanged)
            stateChanged = true;
    }

    Array<File> loadFactorySubdirs()
    {
        return factoryDir.findChildFiles(File::TypesOfFileToFind::findDirectories, false);
    }
    
    Array<File> loadUserSubdirs()
    {
        return userDir.findChildFiles(File::TypesOfFileToFind::findDirectories, false);
    }

    // get array of file names of a factory subdirectory
    StringArray loadFactoryPresets(const File &subdir)
    {
        if (!subdir.exists())
            return StringArray{};
        
        assert(subdir.isDirectory());
        auto presets = subdir.findChildFiles(File::TypesOfFileToFind::findFiles, false, "*.aap");
        StringArray result;
        for (auto &p : presets)
            result.add(p.getFileNameWithoutExtension());
        return result;
    }

    StringArray loadUserPresets(const File &subdir)
    {
        if (!subdir.exists())
            return StringArray{};
        
        assert(subdir.isDirectory());
        auto presets = subdir.findChildFiles(File::TypesOfFileToFind::findFiles, false, "*.aap");
        StringArray names;
        for (auto &p : presets)
            names.add(p.getFileNameWithoutExtension());
        return names;
    }

    bool savePreset(const String& filename, const File& parentDir)
    {
        auto file = parentDir.getChildFile(File::createLegalFileName(filename)).withFileExtension("aap");
        if (!file.existsAsFile())
            file.create();
        
        auto state = apvts.copyState();
        auto xml = state.createXml();

        if (xml->isValidXmlName(filename))
            xml->setTagName(filename);
        else
            xml->setTagName(filename.removeCharacters("\"#@,;:<>*^|!?\\/ "));

        return xml->writeTo(file);
    }

    bool loadPreset(const String& filename, bool factoryPreset, const String &subdir = "")
    {
        ValueTree newstate;
        File &dirToUse = factoryPreset ? factoryDir : userDir;
        auto file = dirToUse.getChildFile(subdir + filename + ".aap");
        if (!file.exists())
            return false;

        auto xml = parseXML(file);
        if (!xml->hasTagName(filename.removeCharacters("\"#@,;:<>*^|!?\\/ ")))
            return false;

        newstate = ValueTree::fromXml(*xml);

        auto hq = apvts.state.getChildWithProperty("id", "hq");
        auto renderHQ = apvts.state.getChildWithProperty("id", "renderHQ");
        auto newHQ = newstate.getChildWithProperty("id", "hq");
        auto newRenderHQ = newstate.getChildWithProperty("id", "renderHQ");
        // auto bypass = apvts.state.getChildWithProperty("id", "bypass");
        // auto newBypass = newstate.getChildWithProperty("id", "bypass");

        if (newHQ.isValid())
            newHQ.copyPropertiesFrom(hq, nullptr);
        else
            jassertfalse;

        if (newRenderHQ.isValid())
            newRenderHQ.copyPropertiesFrom(renderHQ, nullptr);
        else
            jassertfalse;

        // if (newBypass.isValid())
        //     newBypass.copyPropertiesFrom(bypass, nullptr);
        // else
        //     jassertfalse;

        apvts.replaceState(newstate);

        stateChanged = false;
        
        return true;
    }

    bool stateChanged = false;

    String getStateAsString() const
    {
        auto xml = apvts.state.createXml();
        return xml->toString();
    }

    void setStateFromString(const String& str)
    {
        auto newstate = ValueTree::fromXml(str);
        apvts.replaceState(newstate);
    }

    File factoryDir {
        PRESET_PATH
        "/Factory" };
    File userDir{ 
        PRESET_PATH
        "/User" };

private:
    AudioProcessorValueTreeState& apvts;
};
