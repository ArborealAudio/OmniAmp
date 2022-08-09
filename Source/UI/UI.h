// UI.h
// header for including all UI-related files

#pragma once

#define BACKGROUND_COLOR 0xffaa8875
#define AMP_COLOR 0xffcab4a0
#define TOP_TRIM 0xff293236

static const Font getCustomFont()
{
    static auto typeface = Typeface::createSystemTypefaceFor (BinaryData::LiberationSansRegular_ttf, BinaryData::LiberationSansRegular_ttfSize);
    
    return Font (typeface);
}

#include "Button.h"
#include "ChoiceMenu.h"
#include "Knob.h"
#include "AmpControls.h"
#include "SineWave.hpp"
#include "Cabs.h"
#include "Reverb.h"