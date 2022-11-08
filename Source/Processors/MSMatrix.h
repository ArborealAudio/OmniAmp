// MSMatrix.h

#pragma once

struct MSMatrix
{
    template <typename T>
    inline static void msEncode (dsp::AudioBlock<T>& block, float sideGain = 1.f)
    {
        jassert(block.getNumChannels() > 1);

        T* L = block.getChannelPointer(0);
        T* R = block.getChannelPointer(1);

        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            auto mid = (L[i] + R[i]) * 0.5f;
            auto side = (L[i] - R[i]) * (0.5f * sideGain);

            L[i] = mid;
            R[i] = side;
        }
    }

    template <typename T>
    inline static void msDecode (dsp::AudioBlock<T>& block)
    {
        jassert(block.getNumChannels() > 1);

        T* mid = block.getChannelPointer(0);
        T* side = block.getChannelPointer(1);

        for (size_t i = 0; i < block.getNumSamples(); ++i)
        {
            auto left = side[i] + mid[i];
            auto right = mid[i] - side[i];

            mid[i] = left;
            side[i] = right;
        }
    }
};

struct Balance
{
    template <typename T>
    inline static void processBalance(dsp::AudioBlock<T>& block, float balance, bool ms)
    {
        if (ms)
            block.getSingleChannelBlock(1).multiplyBy(balance);
        else {
            MSMatrix::msEncode(block);
            block.getSingleChannelBlock(1).multiplyBy(balance);
            MSMatrix::msDecode(block);
        }
    }
};