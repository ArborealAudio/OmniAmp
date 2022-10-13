// UI.h
// header for including all UI-related files

#pragma once

#define BACKGROUND_COLOR 0xffaa8875
#define AMP_COLOR 0xffcab4a0
#define TOP_TRIM 0xff293236
#define DEEP_BLUE 0xff194545

static const Typeface::Ptr getCustomFont()
{
    static auto typeface = Typeface::createSystemTypefaceFor (BinaryData::LiberationSansRegular_ttf, BinaryData::LiberationSansRegular_ttfSize);
    
    return typeface;
}

static void writeConfigFile(const String& property, int value)
{
    File config {File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName() + "/Arboreal Audio/Gamma/config.xml"};
    if (!config.existsAsFile())
        config.create();
    
    auto xml = parseXML(config);
    if (xml == nullptr)
    {
        xml.reset(new XmlElement("Config"));
    }

    xml->setAttribute(property, value);
    xml->writeTo(config);
}

/*returns integer value of read property, or -1 if it doesn't exist*/
static int readConfigFile(const String& property)
{
    File config {File::getSpecialLocation(File::userApplicationDataDirectory).getFullPathName() + "/Arboreal Audio/Gamma/config.xml"};
    if (!config.existsAsFile())
        return -1;
    
    auto xml = parseXML(config);
    if (xml != nullptr && xml->hasTagName("Config"))
        return xml->getIntAttribute(property, -1);
    
    return -1;
}

#include "Button.h"
#include "ChoiceMenu.h"
#include "Knob.h"
#include "AmpControls.h"
#include "SineWave.hpp"
#include "CabComponent.h"
#include "ReverbComponent.h"
#include "Menu.h"
#include "UI_Top.h"