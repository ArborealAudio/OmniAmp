/*
  ==============================================================================

    PresetManager.h

  ==============================================================================
*/

#pragma once

struct PresetManager
{
    PresetManager(AudioProcessorValueTreeState& vts) : apvts(vts)
    {
        if (!userDir.isDirectory())
            userDir.createDirectory();
    }

    StringArray loadFactoryPresetList()
    {
        StringArray names;

        for (int i = 0; i < 12; ++i)
        {
            if (String(BinaryData::originalFilenames[i]).endsWith(".aap"))
                names.set(i, String(BinaryData::originalFilenames[i]).upToFirstOccurrenceOf(".", false, false));
        }

        return names;
    }

    StringArray loadUserPresetList()
    {
        userPresets = userDir.findChildFiles(2, false, "*.aap");

        StringArray names;

        for (int i = 0; i < userPresets.size(); ++i)
        {
            names.set(i, userPresets[i].getFileNameWithoutExtension());
        }

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
            xml->setTagName(filename.removeCharacters("\"#@,;:<>*^|?\\/ "));

        return xml->writeTo(file);
    }

    bool loadPreset(const String& filename, bool factoryPreset)
    {
        ValueTree newstate;
        if (factoryPreset) {
            int size;
            auto str = String(filename).replace(" ", "_");
            str.append("_aap", 4);
            auto data = BinaryData::getNamedResource(str.toRawUTF8(), size);
            auto xml = parseXML(String::createStringFromData(data, size));
            newstate = ValueTree::fromXml(*xml);
        }
        else {
            auto file = userDir.getChildFile(File::createLegalFileName(filename)).withFileExtension("aap");
            if (!file.exists())
                return false;

            auto xml = parseXML(file);
            DBG(xml->toString());
            if (!xml->hasTagName(filename.removeCharacters("\"#@,;:<>*^|?\\/ ")))
                return false;

            newstate = ValueTree::fromXml(*xml);
        }

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
        
        return true;
    }

    String getStateAsString()
    {
        auto xml = apvts.state.createXml();
        return xml->toString();
    }

    void setStateFromString(const String& str)
    {
        auto newstate = ValueTree::fromXml(str);
        apvts.replaceState(newstate);
    }

    File userDir{ File::getSpecialLocation(File::SpecialLocationType::userApplicationDataDirectory)
        .getFullPathName() + "/Arboreal Audio/Gamma/Presets/User Presets" };

private:
    AudioProcessorValueTreeState& apvts;

    Array<File> factoryPresets;
    Array<File> userPresets;
};
