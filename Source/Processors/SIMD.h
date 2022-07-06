// SIMD.h

#pragma once

using vec = xsimd::batch<float>;

class SIMD
{

    template <typename T>
    void interleaveSamples (const T** source, T* dest, int numSamples, int numChannels)
    {
        for (int chan = 0; chan < numChannels; ++chan)
        {
            auto i = chan;
            const auto* src = source[chan];

            for (int j = 0; j < numSamples; ++j)
            {
                dest[i] = src[j];
                i += numChannels;
            }
        }
    }

    template <typename T>
    void deinterleaveSamples (const T* source, T** dest, int numSamples, int numChannels)
    {
        for (int chan = 0; chan < numChannels; ++chan)
        {
            auto i = chan;
            auto* dst = dest[chan];

            for (int j = 0; j < numSamples; ++j)
            {
                dst[j] = source[i];
                i += numChannels;
            }
        }
    }

    chowdsp::AudioBlock<vec> interleaved;
    chowdsp::AudioBlock<float> zero;

    HeapBlock<char> interleavedData, zeroData;
    std::vector<const float*> channelPointers;

public:

    SIMD() = default;

    void setInterleavedBlockSize(int numChannels, int numSamples)
    {
        interleaved = chowdsp::AudioBlock<vec>(interleavedData, numChannels, numSamples);
        zero = dsp::AudioBlock<float>(zeroData, vec::size, numSamples);
        zero.clear();

        channelPointers.resize(numChannels * vec::size);
    }

    chowdsp::AudioBlock<vec> interleaveBlock(dsp::AudioBlock<float>& block)
    {
        auto n = block.getNumSamples();
        auto numChannels = block.getNumChannels();

        auto* inout = channelPointers.data();

        for (auto ch = 0; ch < numChannels; ch++)
            inout[ch] = (ch < numChannels ? const_cast<float*> (block.getChannelPointer (ch)) : zero.getChannelPointer (ch % vec::size));

        for (size_t ch = 0; ch < numChannels; ch += vec::size)
        {
            auto* simdBlockData = reinterpret_cast<float*> (interleaved.getChannelPointer (ch / vec::size));
            interleaveSamples (&inout[ch], simdBlockData, static_cast<int> (n), static_cast<int> (vec::size));
        }

        return interleaved;
    }

    dsp::AudioBlock<float> deinterleaveBlock(chowdsp::AudioBlock<vec>& block)
    {
        auto n = block.getNumSamples();
        auto inout = channelPointers.data();

        for (size_t ch = 0; ch < block.getNumChannels(); ch += vec::size)
        {
            auto* simdBlockData = reinterpret_cast<float*> (block.getChannelPointer (ch / vec::size));
            deinterleaveSamples (simdBlockData,
                                const_cast<float**> (&inout[ch]),
                                static_cast<int> (n),
                                static_cast<int> (vec::size));
        }
    }
};