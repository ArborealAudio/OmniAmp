// SIMD.h

#pragma once
using vec = xsimd::batch<double>;

template <typename T, class RegBlock, class SIMDBlock>
class SIMD
{
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

    SIMDBlock interleaved;
    RegBlock zero;

    HeapBlock<char> interleavedData, zeroData;
    std::vector<const T*> channelPointers;

public:

    SIMD() = default;

    void setInterleavedBlockSize(int numChannels, int numSamples)
    {
        interleaved = SIMDBlock(interleavedData, numChannels, numSamples);
        zero = RegBlock(zeroData, vec::size, numSamples);
        zero.clear();

        auto numVecChannels = (numChannels + vec::size - 1) / vec::size;

        channelPointers.resize(numVecChannels * vec::size);
    }

    SIMDBlock interleaveBlock(const RegBlock& block)
    {
        auto n = block.getNumSamples();
        auto numChannels = channelPointers.size();
        auto* inout = channelPointers.data();

        for (auto ch = 0; ch < numChannels; ch++)
            inout[ch] = (ch < block.getNumChannels() ? const_cast<T*> (block.getChannelPointer (ch)) : zero.getChannelPointer (ch % vec::size));
        
        for (size_t ch = 0; ch < numChannels; ch += vec::size)
        {
            auto* simdBlockData = reinterpret_cast<T*> (interleaved.getChannelPointer (ch / vec::size));
            interleaveSamples (&inout[ch], simdBlockData, static_cast<int> (n), static_cast<int> (vec::size));
        }

        return interleaved.getSubBlock(0, block.getNumSamples());
    }

    void deinterleaveBlock(SIMDBlock& block)
    {
        auto n = block.getNumSamples();
        auto numChannels = channelPointers.size();
        auto* inout = channelPointers.data();

        for (size_t ch = 0; ch < numChannels; ch += vec::size)
        {
            auto* simdBlockData = reinterpret_cast<T*> (block.getChannelPointer (ch / vec::size));
            deinterleaveSamples (simdBlockData,
                                const_cast<T**> (&inout[ch]),
                                static_cast<int> (n),
                                static_cast<int> (vec::size));
        }

        // return inBlock;
    }
};