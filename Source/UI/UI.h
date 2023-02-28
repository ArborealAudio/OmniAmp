// UI.h
// header for including all UI-related files, global UI definitions & functions

#pragma once

#define BACKGROUND_COLOR 0xff293236
#define DEEP_BLUE 0xff194545
#define LIGHT_BLUE 0xff4b626c
#define LIGHT_GREEN 0xff4b6c59
#define DULL_RED 0xff6c4b4b

static const Typeface::Ptr getCustomFont()
{
    static auto typeface = Typeface::createSystemTypefaceFor(BinaryData::LiberationSansRegular_ttf, BinaryData::LiberationSansRegular_ttfSize);

    return typeface;
}

/**
 * will lay out components in vector-wise order, either horizontally or vertically across the bounds provided.
 * @param components should be a vector of component pointers
 * @param vertical by default stuff will be laid out horizontally
 * @param proportion a skew value, < 1 will skew every component after initial component in the vector by this fraction of init component's width; > 1 will skew components after initial component to be larger than initial component by said factor
 * @param padding multiplier of each component's width to be applied as padding
 */
static void layoutComponents(std::vector<Component *> comp, Rectangle<int> &bounds, bool vertical = false, float proportion = 1.f, float padding = 0.f)
{
    size_t numComp = comp.size();
    assert(numComp != 0);
    assert(bounds.getWidth() != 0);
    assert((bounds.getWidth() / numComp) > 1);

    int chunk = vertical ? bounds.getHeight() / numComp : bounds.getWidth() / numComp;

    if (!vertical)
    {
        {
            auto b = bounds.removeFromLeft(chunk * (1.f / proportion));
            comp[0]->setBounds(b.reduced(b.getWidth() * padding));
        }
        for (size_t i = 1; i < numComp; ++i)
        {
            auto b = bounds.removeFromLeft(chunk * proportion);
            comp[i]->setBounds(b.reduced(b.getWidth() * padding));
        }
    }
    else
    {
        {
            auto b = bounds.removeFromTop(chunk * (1.f / proportion));
            comp[0]->setBounds(b.reduced(b.getWidth() * padding));
        }
        for (size_t i = 1; i < numComp; ++i)
        {
            auto b = bounds.removeFromTop(chunk * proportion);
            comp[i]->setBounds(b.reduced(b.getWidth() * padding));
        }
    }
}

namespace Blur
{
    // SPDX-License-Identifier: Zlib
    // Copyright (c)2020 by George Yohng

    template <int blurShift, bool enhanceContrast = false>
    inline void blurImage(Image &img)
    {
        if (!img.isValid() || !img.getWidth() || !img.getHeight())
            return;
        img = img.convertedToFormat(Image::ARGB);
        Image::BitmapData bm(img, 0, 0, img.getWidth(), img.getHeight(), Image::BitmapData::readWrite);
        int h = img.getHeight();
        int w = img.getWidth();
        for (int y = 0; y < h; y++)
        {
            for (int c = 0; c < 4; c++)
            {
                uint8 *p = bm.getLinePointer(y) + c;
                int s = p[0] << 16;
                for (int x = 0; x < w; x++, p += 4)
                {
                    int px = int(p[0]) << 16;
                    s += (px - s) >> blurShift;
                    p[0] = s >> 16;
                }

                p -= 4;
                for (int x = 0; x < w; x++, p -= 4)
                {
                    int px = int(p[0]) << 16;
                    s += (px - s) >> blurShift;
                    p[0] = s >> 16;
                }
            }
        }

        for (int x = 0; x < w; x++)
        {
            for (int c = 0; c < 4; c++)
            {
                uint8 *p = bm.getPixelPointer(x, 0) + c;
                int incr = int(bm.getPixelPointer(x, 1) - bm.getPixelPointer(x, 0));
                int s = p[0] << 16;
                for (int y = 0; y < h; y++, p += incr)
                {
                    int px = int(p[0]) << 16;
                    s += (px - s) >> blurShift;
                    p[0] = s >> 16;
                }

                p -= incr;
                for (int y = 0; y < h; y++, p -= incr)
                {
                    int px = int(p[0]) << 16;
                    s += (px - s) >> blurShift;
                    if (enhanceContrast)
                    {
                        px = s >> 8;
                        px = ((((98304 - px) >> 7) * px >> 16) * px >> 16); // sine clamp
                        p[0] = jlimit(0, 255, px);
                    }
                    else
                    {
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
#include "../Presets/PresetComp.h"
#include "Splash.h"