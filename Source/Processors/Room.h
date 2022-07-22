// Room.h

#pragma once

class Room
{   
    template <int size>
    struct MixMatrix
    {
        MixMatrix() = default;

        /**
         * Expects an array of N channels worth of samples
        */
        static inline void processHadamardMatrix(double* ch)
        {
            recursive(ch);

            auto scale = std::sqrt(1.0 / (double)size);

            for (int i = 0; i < size; ++i)
                ch[i] *= scale;
        }

        /**
         * Expects an array of N channels worth of samples
        */
        static void processHouseholder(double* ch)
        {
            const double h_mult = -2.0 / (double)size;

            double sum = 0.0;
            for (int j = 0; j < size; ++j)
                sum += ch[j];

            sum *= h_mult;

            for (int j = 0; j < size; ++j)
                ch[j] += sum;
        }

        static inline void recursive(double* data)
        {
            if (size <= 1)
                return;
            const int hSize = size / 2;

            MixMatrix<hSize>::recursive(data);
            MixMatrix<hSize>::recursive(data + hSize);

            for (int i = 0; i < hSize; ++i)
            {
                double a = data[i];
                double b = data[i + hSize];
                data[i] = a + b;
                data[i + hSize] = a - b;
            }
        }
    };

    struct Diffuser
    {

        float delayRange;

        /**
        @param delayRangeinS you guessed it, delay range in seconds
        */
        Diffuser(float delayRangeinS) : delayRange(delayRangeinS)/*  : maxDelay(max), minDelay(min) */
        {
            delay.resize(channels);
            rand.setSeed((int64)0);
        }

        void prepare(dsp::ProcessSpec spec)
        {
            spec.numChannels = 1;

            invert.clear();

            int i = 0;
            for (auto &d : delay)
            {
                auto delayRangeSamples = delayRange * spec.sampleRate;
                double minDelay = delayRangeSamples * i / channels;
                double maxDelay = delayRangeSamples * (i + 1) / channels;
                d.prepare(spec);
                d.setMaximumDelayInSamples(maxDelay + 1);
                d.setDelay(rand.nextInt(Range<int>(minDelay, maxDelay)));
                invert.push_back(rand.nextInt() % 2 == 0);
                ++i;
            }
        }

        void reset()
        {
            for (auto& d : delay)
                d.reset();
        }

        // expects a block of N channels
        void process(dsp::AudioBlock<double>& block)
        {
            for (auto i = 0; i < block.getNumSamples(); ++i)
            {
                std::vector<double> vec;

                for (auto ch = 0; ch < channels; ++ch)
                {
                    delay[ch].pushSample(0, block.getSample(ch, i));
                    vec.push_back(delay[ch].popSample(0));
                }

                MixMatrix<channels>::processHadamardMatrix(vec.data());

                for (auto ch = 0; ch < channels; ++ch)
                    block.getChannelPointer(ch)[i] = vec[ch];
            }

            // shuffleChannels(block);
        }

    private:

        void shuffleChannels(dsp::AudioBlock<double>& block)
        {
            for (int ch = 0; ch < channels; ++ch)
            {
                auto in = block.getChannelPointer(ch);
                auto out = in;
                // if ((ch + 2) < channels)
                //     out = block.getChannelPointer(ch + 2);
                // else
                //     out = block.getChannelPointer(channels - ch);

                // FloatVectorOperations::copy(out, in, block.getNumSamples());

                if (invert[ch])
                    FloatVectorOperations::multiply(out, -1.0, block.getNumSamples());
            }
        }

        std::vector<dsp::DelayLine<double>> delay;
        std::vector<bool> invert;

        float inc;

        static constexpr size_t channels = 8;

        Random rand;
    };

    struct MixedFeedback
    {
        MixedFeedback() = default;

        void prepare(const dsp::ProcessSpec& spec)
        {
            double delaySamplesBase = delayMs * 0.001 * spec.sampleRate;

            for (int ch = 0; ch < channels; ++ch)
            {
                double r = ch * 1.0 / channels;
                delaySamples[ch] = std::pow(2.0, r) * delaySamplesBase;
                delays[ch].prepare(spec);
                delays[ch].setMaximumDelayInSamples(delaySamples[ch] + 1);
            }

            auto multiSpec = spec;
            multiSpec.numChannels = channels;

            lp.prepare(multiSpec);
            lp.setType(strix::FilterType::firstOrderLowpass);
            lp.setCutoffFreq(dampening * multiSpec.sampleRate * 0.5);
            lp.setResonance(1.0);
        }

        void reset()
        {
            for (auto& d : delays)
                d.reset();

            lp.reset();
        }

        void process(dsp::AudioBlock<double>& block)
        {
            for (int i = 0; i < block.getNumSamples(); ++i)
            {
                std::vector<double> delayed;

                for (int ch = 0; ch < channels; ++ch)
                {
                    delayed.push_back(delays[ch].popSample(0, delaySamples[ch]));
                }

                MixMatrix<channels>::processHouseholder(delayed.data());

                for (int ch = 0; ch < channels; ++ch)
                {
                    auto in = block.getChannelPointer(ch)[i];
                    auto sum = in + delayed[ch] * decayGain;
                    delays[ch].pushSample(0, sum);

                    block.setSample(ch, i, delayed[ch]);
                }
            }

            lp.processBlock(block);
        }

        double delayMs = 150.0;
        double decayGain = 0.85;

        /*A fraction of Nyquist, where the lowpass will be placed in the feedback path*/
        double dampening = 1.0;

    private:
        static constexpr size_t channels = 8;

        std::array<int, channels> delaySamples;
        std::array<dsp::DelayLine<double>, channels> delays;
        strix::SVTFilter<double> lp;
    };

    std::vector<Diffuser> diff;

    MixedFeedback feedback;

    AudioBuffer<double> splitBuf;
    AudioBuffer<double> erBuf;
    AudioBuffer<double> stereoBuf;

    template<typename Sample, int channels>
	class StereoMultiMixer {
		static_assert((channels/2)*2 == channels, "StereoMultiMixer must have an even number of channels");
		static_assert(channels >= 2, "StereoMultiMixer must have an even number of channels");
		static constexpr int hChannels = channels/2;
		std::array<Sample, channels> coeffs;
	public:
		StereoMultiMixer() {
			coeffs[0] = 1;
			coeffs[1] = 0;
			for (int i = 1; i < hChannels; ++i) {
				double phase = M_PI*i/channels;
				coeffs[2*i] = std::cos(phase);
				coeffs[2*i + 1] = std::sin(phase);
			}
		}
		
		void stereoToMulti(const Sample** input, Sample** output, int numSamples) const
        {
            for (int j = 0; j < numSamples; ++j)
            {
                output[0][j] = input[0][j];
                output[1][j] = input[1][j];
                for (int i = 2; i < channels; i += 2) {
                    output[i][j] = input[0][j] * coeffs[i] + input[1][j] * coeffs[i + 1];
                    output[i + 1][j] = input[1][j] * coeffs[i] - input[0][j] * coeffs[i + 1];
                }
            }
		}

		void multiToStereo(const Sample** input, Sample** output, int numSamples) const
        {
            for (int j = 0; j < numSamples; ++j)
            {
                output[0][j] = input[0][j];
                output[1][j] = input[1][j];
                for (int i = 2; i < channels; i += 2) {
                    output[0][j] += input[i][j] * coeffs[i] - input[i + 1][j] * coeffs[i + 1];
                    output[1][j] += input[i + 1][j] * coeffs[i] + input[i][j] * coeffs[i + 1];
                }
            }
		}
		/// Scaling factor for the downmix, if channels are phase-aligned
		static constexpr Sample scalingFactor1() {
			return 2/Sample(channels);
		}
		/// Scaling factor for the downmix, if channels are independent
		static Sample scalingFactor2() {
			return std::sqrt(scalingFactor1());
		}
	};

    StereoMultiMixer<double, 8> upMix;

    double erLevel = 1.0;

public:
    /**
     * @param roomSizeMs room size in Ms, use this in conjunction w/ rt60 to create bigger or larger rooms
     * @param rt60 controls density of reverberation (basically the feedback) of the algo
     * @param erLevel a value btw 0->1 that sets the level of early reflections. Should taper logarithmically from this value to
     * something lower.
     * @param dampening Set the level of dampening, as a fraction of Nyquist, in the feedback path using the Room constructor
    */
    Room(double roomSizeMs, double rt60, double erLevel, double dampening) : erLevel(erLevel)
    {
        auto diffusion = (roomSizeMs * 0.001);
        for (int i = 0; i < 4; ++i)
        {
            diffusion *= 0.5;
            diff.emplace_back(diffusion);
        }

        feedback.delayMs = roomSizeMs;

        double typicalLoopMs = roomSizeMs * 1.5;

        double loopsPerRt60 = rt60 / (typicalLoopMs * 0.001);

        double dbPerCycle = -60.0 / loopsPerRt60;

        feedback.decayGain = std::pow(10.0, dbPerCycle * 0.05);

        feedback.dampening = dampening;
    }

    void prepare(const dsp::ProcessSpec& spec)
    {
        stereoBuf.setSize(2, spec.maximumBlockSize);
        splitBuf.setSize(8, spec.maximumBlockSize);
        erBuf.setSize(8, spec.maximumBlockSize);

        for (auto& d : diff)
            d.prepare(spec);

        feedback.prepare(spec);
    }

    void reset()
    {
        for (auto& d : diff)
            d.reset();
        feedback.reset();
    }

    void process(AudioBuffer<double>& buf, float amt)
    {
        upMix.stereoToMulti(buf.getArrayOfReadPointers(), splitBuf.getArrayOfWritePointers(), buf.getNumSamples());

        dsp::AudioBlock<double> block(splitBuf);

        erBuf.clear();

        for (auto i = 0; i < diff.size(); ++i)
        {
            diff[i].process(block);
            auto r = i * 1.0 / diff.size();
            for (auto ch = 0; ch < 8; ++ch)
                erBuf.addFrom(ch, 0, block.getChannelPointer(ch), erBuf.getNumSamples(), erLevel / std::pow(2.0, r));
        }

        feedback.process(block);

        block.add(dsp::AudioBlock<double>(erBuf));

        upMix.multiToStereo(splitBuf.getArrayOfReadPointers(), stereoBuf.getArrayOfWritePointers(), stereoBuf.getNumSamples());

        buf.applyGain(1.0 - amt);

        buf.addFrom(0, 0, stereoBuf.getReadPointer(0), buf.getNumSamples(), amt * upMix.scalingFactor1());
        buf.addFrom(1, 0, stereoBuf.getReadPointer(1), buf.getNumSamples(), amt * upMix.scalingFactor1());
    }
};

enum class ReverbType
{
    Off,
    Room,
    Hall
};

class ReverbManager
{
    strix::ReleasePoolShared relPool;

    std::shared_ptr<Room> rev;

    dsp::ProcessSpec memSpec;

public:

    ReverbManager()
    {
        rev = std::make_shared<Room>(75.0, 2.0, 0.5, 0.15);
    }

    void prepare(const dsp::ProcessSpec& spec)
    {
        rev->prepare(spec);

        memSpec = spec;
    }

    void reset()
    {
        rev->reset();
    }

    void changeRoomType(ReverbType newType)
    {
        std::shared_ptr<Room> newRev;
        
        switch (newType)
        {
        case ReverbType::Off:
            return;
        case ReverbType::Room:
            newRev = std::make_shared<Room>(25.0, 1.0, 0.75, 0.1);
            break;
        case ReverbType::Hall:
            newRev = std::make_shared<Room>(75.0, 2.0, 0.5, 0.15);
            break;
        }

        newRev->prepare(memSpec);

        relPool.add(newRev);
        std::atomic_store(&rev, newRev);
    }

    void process(AudioBuffer<double>& buffer, float amt)
    {
        std::shared_ptr<Room> procRev = std::atomic_load(&rev);

        procRev->process(buffer, amt);
    }
};