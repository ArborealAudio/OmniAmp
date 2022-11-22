// UI.h
// header for including all UI-related files, global UI definitions & functions

#pragma once

#define BACKGROUND_COLOR 0xffaa8875 // unused atm
#define AMP_COLOR 0xffcab4a0 // guitar amp, specifically
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

namespace Blur
{
    // SPDX-License-Identifier: Zlib
    // Copyright (c)2020 by George Yohng

    template<int blurShift, bool enhanceContrast = false>
    inline void blurImage(Image& img) {
        if (!img.isValid() || !img.getWidth() || !img.getHeight()) return;
        img = img.convertedToFormat(Image::ARGB);
        Image::BitmapData bm(img, 0, 0, img.getWidth(), img.getHeight(), Image::BitmapData::readWrite);
        int h = img.getHeight();
        int w = img.getWidth();
        for (int y = 0; y < h; y++) {
            for (int c = 0; c < 4; c++) {
                uint8* p = bm.getLinePointer(y) + c;
                int s = p[0] << 16;
                for (int x = 0; x < w; x++, p += 4) {
                    int px = int(p[0]) << 16;
                    s += (px - s) >> blurShift;
                    p[0] = s >> 16;
                }

                p -= 4;
                for (int x = 0; x < w; x++, p -= 4) {
                    int px = int(p[0]) << 16;
                    s += (px - s) >> blurShift;
                    p[0] = s >> 16;
                }
            }
        }

        for (int x = 0; x < w; x++) {
            for (int c = 0; c < 4; c++) {
                uint8* p = bm.getPixelPointer(x, 0) + c;
                int incr = int(bm.getPixelPointer(x, 1) - bm.getPixelPointer(x, 0));
                int s = p[0] << 16;
                for (int y = 0; y < h; y++, p += incr) {
                    int px = int(p[0]) << 16;
                    s += (px - s) >> blurShift;
                    p[0] = s >> 16;
                }

                p -= incr;
                for (int y = 0; y < h; y++, p -= incr) {
                    int px = int(p[0]) << 16;
                    s += (px - s) >> blurShift;
                    if (enhanceContrast) {
                        px = s >> 8;
                        px = ((((98304 - px) >> 7) * px >> 16) * px >> 16); // sine clamp
                        p[0] = jlimit(0, 255, px);
                    }
                    else {
                        p[0] = s >> 16;
                    }
                }
            }
        }
    }
} // namespace Blur

#include "Button.h"
#include "ChoiceMenu.h"
#include "Knob.h"
#include "PreComponent.h"
#include "AmpControls.h"
#include "SineWave.hpp"
#include "CabComponent.h"
#include "ReverbComponent.h"
#include "Menu.h"
#include "EnhancerComponent.h"
#include "DownloadManager.h"
#include "../Presets/PresetComp.h"
#include "Splash.h"
#include "Init.h"