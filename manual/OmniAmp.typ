#set page(
	fill: rgb("283238")
)
#set text(
	fill: white,
	font: "Liberation Sans",
	size: 14pt
)

#align(center + top)[
	#image(
		"../Resources/logo.svg",
		width: 13%,
		fit: "cover"
	)
]

#set align(center)
Arboreal Audio presents:

#set text(
	font: "Sora",
	size: 16pt
)

= OmniAmp
== The all-in-one amplifier
 
#linebreak()

#align(center)[
	#image(
		"UI_full.png",
		width: 100%
	)
]
#linebreak()
#set align(left)
#set text(
	font: "Liberation Sans",
	size: 14pt
)
OmniAmp is a robust audio tool meant to be used on any type of sound source. With its full feature set, this can be the only plugin in your signal chain.

It's designed to take the layout of a guitar amplifier and port it to a universal context, with some added features to keep you contained in one interface for the bulk of your mixing.

#linebreak()

== Modes

Central to OmniAmp are its three modes:

=== I. Guitar

#image("Guitar.png")

Four different, custom amp models.

- *GammaRay* is a classically-voiced guitar amp with a modest, clean-to-crunch default channel, and a more aggressive and bright Boost channel. Great for indie or moderate rock tones.

- *Sunbeam* is a cleaner amp with thick low-end and very clear upper mids. Works great for clean tones or mellower overdriven tones.

- *Moonbeam* is a darker amp with less headroom and heavier distortion, good for metal or heavier rock rhythm tones.

- *XRay* an aggressive, midrange-focused amp that bridges the gap between **GammaRay** and **Moonbeam**. Has a fair amount of distortion and cuts through quite well.

=== II. Bass

#image("Bass.png")

Three custom bass amp models:

- *Cobalt:* a modern-sounding, full-voiced bass amp that doesn't spare an ounce of low-end.

- *Emerald* a more modest, modern amp with more headroom than **Cobalt**.

- *Quartz* a vintage-sounding amp with a dampened low-end and punchier hi-mids.

=== III. Channel

#image("Channel.png")

Here, you can use OmniAmp like a channel strip. The tone controls are neutrally-voiced at 12 o'clock, and its saturation stages are bypassed at 0, so you're only doing the processing that you need.

Additionally, the filters change their response depending on the amount of gain applied, so deep cuts are more surgical, and low boosts are broader.

There are two modes:

- *Modern:* here the Preamp stage produces symmetric distortion, and the filters are a bit more precise in their rolloff. The Power Amp stage has a sharper knee but is cleaner below the saturation point.

- *Vintage:* the Preamp stage uses more tube-like asymmetric saturation, the filters are broader, and the Power Amp digs into the signal earlier but has a gentler overall knee.

== Amp Controls

=== Overdrive

This *is* modelled after a classic distortion pedal (it's not the Tube Screamer, don't you have enough of those?) It's the MXR Distortion+! 

Conveniently, the real-life pedal just has one knob to control the distortion, and an output volume knob, so we rolled those together into one knob for a quick and easy distortion pedal before the amp. At 0 this is fully bypassed.

=== Preamp and Boost

The preamp stage of the amp, or a series of triodes if you're using Channel mode. By default it will add a nice warmth and crunch in all modes, but by enabling *Boost* you can get some gnarly, tube-flavored distortion.

In Channel mode, this is bypassed at 0.

=== Auto Gain

Automatic gain compensation for Preamp and Poweramp gain stages, as well as the EQ section, where you'll get frequency-weighted gain compensation.

=== Tone Controls

A mostly self-explanitory set of controls if you've ever used or even thought about using a guitar amp. However, in Channel mode they behave more like a channel strip's EQ section with fixed frequency bands. The filters also have a variable Q depending on the gain.

=== Power Amp

An additional gain stage at the end of the amp/channel strip. In the Guitar and Bass modes, this functions like the pentode tubes in the amp's output stage, or like an optional pair of pentodes at the end of your channel strip.

In Channel mode, this is bypassed at 0.

#pagebreak()
== Pre Section

#image("Pre.png")

=== Stereo/MS

By default, this passes in a stereo input as you would expect, processing left and right independently. If you enable M/S processing, your input signal is converted into Mid/Side and processed as such throughout the plugin. This is useful if you want compression or distortion to process Mid and Side independently, which can make for more transparent or natural-sounding compression or distortion (and sounds great on a full mix!).

*Note*: This, along with Stereo Emphasis, won't have any effect if your channel configuration is 1 input / 1 output. You need stereo input and output for this feature to work!

=== Stereo Emphasis

The Stereo Emphasis tool applies either a boost or a reduction of the stereo width, and then an equal and opposite boost or cut *after* the processing. With this, you can emphasize or de-emphasize the sides of a stereo image. With the M/S option, you can create interesting relationships between Mid and Side information, and affect the sense of space in your sound.

=== LF/HF Emphasis

Like the Stereo Emphasis tool, except with frequency. These are high- or low-shelf filters that either boost or cut before the processing, and then apply an equal and opposite filter to negate the frequency response, so you end up *emphasizing* those frequencies. This is useful if you want to have your low-end distort less, clip off more highs for a more vintage sound, or to just drive everything into obscenity and mayhem.

=== Doubler

This uses short delay lines to add width to a sound that may be narrow or even mono. Great for making a mono guitar sound double-tracked! It's also mono-compatible, meaning that you won't get phase issues if your track is played back in mono.

=== Opto Comp

An opto-style compressor, inspired by but in no way modelled after one of the earlier Boss compressor pedals.

All you *really* need to know is that the higher the knob goes = more compression. At 0 it's fully bypassed.

In the vein of opto compressors, the attack and release times are highly dependent on the signal and the amount of gain reduction being applied. Essentially, with small amounts of gain reduction, the attack and release are slower, with the attack being as high as 50ms, and the release approaching 1 second. With greater amounts of gain reduction, the attack time increases and so does the release, up to 5ms and 50ms respectively.

This results in a very colorful compression, and it's also frequency-dependent. The sidechain is calibrated based on the mode, but with the general pattern of gently filtering the low- and high-end. For the sake of simplicity, just know that the sidechains have been tailored to sound good for whichever mode you're using.

Lastly, in Guitar or Bass mode, the compressor imparts a high-midrange boost to aid in the plucky, punchy effect the compression gives.

==== Pre/Post

Sets whether the compressor is before or after the amp. Yes, while this is technically the pre-amp section, it can be interesting to put the compressor after the amp!

==== Link

Enables or disables stereo linking for the compression. Linking is on by default and suitable for most stereo content--but for wildly asymmetric stereo images, or if you want unlinked M/S compression, here you go!

#pagebreak()
== Cabs and Reverb

#image("./Bottom.png")

What amp plugin would be complete without cab simulation? And you're going to need reverb, too.

=== Cabs

There are three cabs to choose from:

- *2x12:* A small two-speaker cabinet with a tight, bright, and punchy sound, good for clean guitar or more moderate tones.

- *4x12:* A four-speaker cabinet designed for roaring rock tones, with a beefy low-end and a scooped midrange.

- *6x12:* Intended to be used as a bass cabinet, this has a huge low-end and a top-end voicing that sounds great on bass guitar.

==== Reso Filters

This adjust the low and high frequency resonance of the cabinets. You can add or attenuate the response of the cab for further customization over the sound.

==== Mic Position

An emulated mic position in front of one of the cab's speakers. You can adjust the horizontal position and the distance away from the speaker, which affects tone in a variety of ways.

=== Reverbs

There are two algorithmic reverbs:

- *Room:* A medium-sized, dampened room with short reflections, great for adding a touch of ambience or space to a sound without getting overwhelming.

- *Hall:* A longer, richer reverb, perfect for adding a grander scope to your sound.

You also have *Predelay, Decay,* and *Size* controls for customizing the response more finely. Increasing the size changes the frequency response and early reflection level of the reverb, making things a bit darker and tending towards late reflections, so you can use it in conjunction with the decay control to fine-tune how the space feels.

Last, the *Bright* control gives you more shimmering top-end in the reverb. With this off, frequencies above 8.5kHz will be rolled off with a 2nd-order filter.

#linebreak()
== Post Section

#image("./Post.png")

=== Low Frequency Enhancer

On the left is a Low Frequency Enhancer. It's a saturating low-end boost that works wonders in thickening your sound. The frequency of the boost depends on the mode being used:

- *Guitar:* 300Hz

- *Bass:* 175Hz

- *Channel:* 200Hz

The enhancer signal is processed in parallel and added onto the dry signal.

=== Invert

Inverts the enhancer's signal for a change in response. This will generally function like a more resonant highpass filter, but due to the nonlinear nature of the enhancer, at higher levels its response can end up looking more like a peak boost around the filter's center.

=== High Frequency Enhancer

On the top right is a High Frequency Enhancer. This is a saturating high-end boost, perfect for adding air, presence, or a nice top-end sheen to your sound. The cutoff for the filter is at 7500Hz

=== Invert

Inverts the enhancer's signal for a change in response. This will generally function like a more resonant lowpass filter, which can end up functioning like a differently-voiced high-shelf at higher levels.

=== Auto

Enables auto-gain compensation for the enhancer filter, which will reduce the amount of frequency boost. The filtered signal is gain compensated after the saturation, so this is useful if you just want the saturation and less of a frequency boost.

=== Cut Filters

A pair of 6db/octave high-pass and low-pass filters, processed after the enhancer filters

#linebreak()
== Top Controls

1. *Input:* An input gain before all processing, between -12dB and +12dB, useful for adding or decresing distortion or compression.

2. *Link:* Link the input gain to the output gain, with the ability to still adjust the output gain. If you turn link off it will automatically adjust the output gain so you don't get a huge jump in volume.

3. *Output:* An output gain after all processing, between -12dB and +12dB, for fine-tuning the final level.

4. *Width:* Add or subtract stereo width. Since this is applied _after_ all the nonlinear processing, you can use this in conjunction with Stereo Emphasis for interesting manipulations of the stereo field.

5. *Mix:* A simple dry/wet mixer for global parallel processing. Keep in mind that many of OmniAmp's processes are non-linear phase, so you may encounter phase cancellation if using this (not necessarily a bad thing!)

6. *Byp:* A global latency-compensated bypass control

#pagebreak()
== Menu

In the upper right is a popup menu for maintaining some of OmniAmp's more advanced features:

- *OpenGL On/Off:* On Windows and Linux, this controls whether hardware-accelerated graphics rendering is done via OpenGL or software rendering. If you've got a decent graphics card in your computer, this may make the UI snappier. Otherwise, if you don't have a graphics card or OpenGL installed on your system, enabling this will either do nothing or cause issues.

- *HQ:* Enables 4x oversampling with linear-phase filters. Off by default to save CPU.

- *Render HQ:* This will enable 4x oversampling when rendering. If HQ is too CPU-intensive for your computer, you could use this to keep aliasing out of your final mix, while getting better performance when mixing in real-time.

- *Show tooltips:* You can disable tooltip hints with this option

- *Default UI Size:* This will reset the UI size to its default of 800x800

- *Check update:* Checks for an update to OmniAmp. If there is one, you'll be presented with a list of new changes and asked if you want to download the update.

- *Activate:* If you haven't activated OmniAmp yet, you can click this to bring up the activation prompt and enter your license.

#linebreak()
== Acknowledgements

Programming, DSP, and design: Alex Riccio

This plugin would not have been possible without the research and open-source contributions of the following:

#show link: underline

- Jatin Chowdhury | https://github.com/Chowdhury-DSP/chowdsp_wdf (c) 2022, Chowdhury-DSP #super[1] | #link("https://chowdsp.com/")[chowdsp.com]

- Sam Schachter | https://github.com/schachtersam32/WaveDigitalFilters_Sharc

- xsimd | https://github.com/xtensor-stack/xsimd (c) 2016, Johan Mabille, Sylvain Corlay, Wolf Vollprecht and Martin Renou
  (c) 2016, QuantStack
  (c) 2018, Serge Guelton #super[1]

- Roland Rabien | https://github.com/FigBug/Gin (c) 2018, Roland Rabien #super[1]

- JUCE | https://github.com/juce-framework/JUCE | #link("https://juce.com")[juce.com]

- Alexandre Bique and Paul Walker | #link("https://github.com/free-audio")[Free Audio/CLAP] | (c) 2021 Alexandre BIQUE
  (c) 2019-2020, Paul Walker #super[2]

*And a huge thank-you to the beta testers:*

Jatin Chowdhury

Florian Mrugalla

***REMOVED*** 

=== #super[1] BSD-3-Clause License

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its
   contributors may be used to endorse or promote products derived from
   this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

=== #super[2] MIT License

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the "Software"), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, 
publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.