// SIMD.h

#pragma once
using vec = xsimd::batch<double>;

template <typename T>
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

    chowdsp::AudioBlock<vec> interleaved;
    chowdsp::AudioBlock<T> zero;

    dsp::AudioBlock<T> inBlock;

    HeapBlock<char> interleavedData, zeroData;
    std::vector<const T*> channelPointers;

public:

    SIMD() = default;

    // use the actual number of channels and not just 1!!
    void setInterleavedBlockSize(int numChannels, int numSamples)
    {
        interleaved = chowdsp::AudioBlock<vec>(interleavedData, numChannels, numSamples);
        zero = dsp::AudioBlock<T>(zeroData, vec::size, numSamples);
        zero.clear();

        auto numVecChannels = chowdsp::Math::ceiling_divide((size_t)numChannels, vec::size);

        channelPointers.resize(numVecChannels * vec::size);
    }

    chowdsp::AudioBlock<vec> interleaveBlock(dsp::AudioBlock<T>& block)
    {
        inBlock = std::move(block);

        auto n = inBlock.getNumSamples();
        auto numChannels = channelPointers.size();
        auto* inout = channelPointers.data();

        for (auto ch = 0; ch < numChannels; ch++)
            inout[ch] = (ch < inBlock.getNumChannels() ? const_cast<T*> (inBlock.getChannelPointer (ch)) : zero.getChannelPointer (ch % vec::size));
        
        using Format = AudioData::Format<AudioData::Float32, AudioData::NativeEndian>;
 
        // AudioData::interleaveSamples (AudioData::NonInterleavedSource<Format> { inout, vec::size, },
        //                               AudioData::InterleavedDest<Format>      { reinterpret_cast<T*>(interleaved.getChannelPointer (0)), vec::size },
        //                               n);

        for (size_t ch = 0; ch < numChannels; ch += vec::size)
        {
            auto* simdBlockData = reinterpret_cast<T*> (interleaved.getChannelPointer (ch / vec::size));
            interleaveSamples (&inout[ch], simdBlockData, static_cast<int> (n), static_cast<int> (vec::size));
        }

        return interleaved;
    }

    dsp::AudioBlock<T> deinterleaveBlock(chowdsp::AudioBlock<vec>& block)
    {
        auto n = block.getNumSamples();
        auto numChannels = channelPointers.size();
        auto* inout = channelPointers.data();

        using Format = AudioData::Format<AudioData::Float32, AudioData::NativeEndian>;

        // AudioData::deinterleaveSamples (AudioData::InterleavedSource<Format>  { reinterpret_cast<T*>(interleaved.getChannelPointer (0)), vec::size },
        //                                 AudioData::NonInterleavedDest<Format> { const_cast<T**>(inout), vec::size}, n);

        for (size_t ch = 0; ch < numChannels; ch += vec::size)
        {
            auto* simdBlockData = reinterpret_cast<T*> (block.getChannelPointer (ch / vec::size));
            deinterleaveSamples (simdBlockData,
                                const_cast<T**> (&inout[ch]),
                                static_cast<int> (n),
                                static_cast<int> (vec::size));
        }

        return inBlock;
    }
};