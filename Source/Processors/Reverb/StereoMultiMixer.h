/**
 * StereoMultiMixer.h
 * Helper class for up- and down-mixing stereo material to and from multi-channel buffers
*/

template <typename Sample, int channels>
class StereoMultiMixer
{
    static_assert((channels / 2) * 2 == channels, "StereoMultiMixer must have an even number of channels");
    static_assert(channels >= 2, "StereoMultiMixer must have an even number of channels");
    static constexpr int hChannels = channels / 2;
    std::array<Sample, channels> coeffs;

public:
    StereoMultiMixer()
    {
        coeffs[0] = 1;
        coeffs[1] = 0;
        for (int i = 1; i < hChannels; ++i)
        {
            double phase = MathConstants<float>::pi * i / channels;
            coeffs[2 * i] = std::cos(phase);
            coeffs[2 * i + 1] = std::sin(phase);
        }
    }

    void stereoToMulti(const Sample *const *input, Sample *const *output, int numSamples) const
    {
        for (int j = 0; j < numSamples; ++j)
        {
            output[0][j] = input[0][j];
            output[1][j] = input[1][j];
            for (int i = 2; i < channels; i += 2)
            {
                output[i][j] = input[0][j] * coeffs[i] + input[1][j] * coeffs[i + 1];
                output[i + 1][j] = input[1][j] * coeffs[i] - input[0][j] * coeffs[i + 1];
            }
        }
    }

    void multiToStereo(const Sample *const *input, Sample *const *output, int numSamples) const
    {
        for (int j = 0; j < numSamples; ++j)
        {
            output[0][j] = input[0][j];
            output[1][j] = input[1][j];
            for (int i = 2; i < channels; i += 2)
            {
                output[0][j] += input[i][j] * coeffs[i] - input[i + 1][j] * coeffs[i + 1];
                output[1][j] += input[i + 1][j] * coeffs[i] + input[i][j] * coeffs[i + 1];
            }
        }
    }

    /// Scaling factor for the downmix, if channels are phase-aligned
    static constexpr Sample scalingFactor1() { return 2 / Sample(channels); }

    /// Scaling factor for the downmix, if channels are independent
    static Sample scalingFactor2() { return std::sqrt(scalingFactor1()); }
};