// SineWave.hpp

#pragma once

namespace strix
{

struct AudioSource : Timer
{
    AudioSource()
    {
        startTimerHz(45);
    }
    ~AudioSource() override
    {
        stopTimer();
    }

    void prepare(const dsp::ProcessSpec& spec)
    {
        rmsSize = (0.5f * spec.sampleRate) / (float)spec.maximumBlockSize;

        for (int i = 0; i < 6; ++i)
        {
            src[i].assign(rmsSize, 0.f);

            sum[i] = 0.f;

            if (rmsSize > 1.f) {
                ptr %= src[i].size();
            }
            else
                ptr = 0;
        }

        float freq = 50.f;
        float nyq = spec.sampleRate * 0.5f;
        for (int i = 0; i < 5; ++i)
        {
            freq = jlimit(50.f, nyq * 0.73f, freq * (float)(i+1));
            hp[i].prepare(spec);
            hp[i].setType(dsp::LinkwitzRileyFilterType::highpass);
            hp[i].setCutoffFrequency(freq);
        }

        for (auto& b : band)
            b.setSize(1, spec.maximumBlockSize, false, true, false);

        numSamplesToRead = spec.maximumBlockSize;
        mainBuf.setSize(1, 44100, false, true, false);
        fifo.setTotalSize(numSamplesToRead);
    }

    /* no locking, must be real-time safe! */
    template <typename T>
    void copyBuffer(const AudioBuffer<T> &buffer)
    {
        const auto numSamples = buffer.getNumSamples();
        const auto scope = fifo.write(numSamples);
        if (scope.blockSize1 > 0)
        {
            auto *dest = mainBuf.getWritePointer(0) + scope.startIndex1;
            const auto *source = buffer.getReadPointer(0);
            for (size_t i = 0; i < scope.blockSize1; ++i)
                dest[i] = static_cast<float>(source[i]);
        }
        if (scope.blockSize2 > 0)
        {
            auto *dest = mainBuf.getWritePointer(0) + scope.startIndex2;
            const auto *source = buffer.getReadPointer(0);
            for (size_t i = 0; i < scope.blockSize2; ++i)
                dest[i] = static_cast<float>(source[i]);
        }
        bufCopied = true;
    }

    void timerCallback() override
    {
        if (bufCopied)
        {
            calcBufferRMS();
            bufCopied = false;
        }
    }

    /* locking, message-thread operation */
    void calcBufferRMS()
    {
        {
            std::unique_lock lock(mutex);
            splitBuffers();

            for (int i = 0; i < 6; ++i)
            {
                rms[i] = band[i].getRMSLevel(0, 0, band[i].getNumSamples());

                if (src[i].size() > 0) {
                    src[i][ptr] = rms[i];
                    ptr = (ptr + 1) % src[i].size();
                }
                else
                    sum[i] = rms[i];
            }
        }

        newBuf = true;
    }

    inline std::array<float, 6> getAvgRMS()
    {
        std::array<float, 6> b_rms;

        for (size_t i = 0; i < 6; ++i)
        {
            if (src[i].size() > 0)
                b_rms[i] = std::sqrt(std::accumulate(src[i].begin(), src[i].end(), 0.f) / (float)src[i].size());
            else
                b_rms[i] = std::sqrt(sum[i]);
        }

        return b_rms;
    }

    std::atomic<bool> newBuf = false;

private:
    float rmsSize = 0.f;
    float rms[6] {0.f};
    size_t ptr = 0;
    std::atomic<float> sum[6];
    std::vector<float> src[6];

    dsp::LinkwitzRileyFilter<float> hp[5];
    juce::AudioBuffer<float> band[6];

    AbstractFifo fifo{1024};
    AudioBuffer<float> mainBuf;
    int numSamplesToRead = 0;

    std::atomic<bool> bufCopied = false;

    std::mutex mutex;

    void splitBuffers()
    {
        {
            const auto scope = fifo.read(numSamplesToRead);
            for (auto &b : band)
            {
                if (scope.blockSize1 > 0)
                {
                    b.copyFrom(0, 0, mainBuf.getReadPointer(0) + scope.startIndex1, scope.blockSize1);
                }
                if (scope.blockSize2 > 0)
                {
                    b.copyFrom(0, scope.blockSize1, mainBuf.getReadPointer(0) + scope.startIndex2, scope.blockSize2);
                }
            }
        }

        for (int i = 0; i < 5; ++i)
        {
            auto hpf = band[i + 1].getWritePointer(0);
            auto lpf = band[i].getWritePointer(0);
            for (auto j = 0; j < band[i].getNumSamples(); ++j)
            {
                auto mono = hpf[j];
                hpf[j] = hp[i].processSample(0, mono);
                lpf[j] = lpf[j] - hpf[j];
            }
        }
    }
};

struct SineWaveComponent : Component, Timer
{
    SineWaveComponent(AudioSource& s) : src(s)
    {
        startTimerHz(45);
    }
    ~SineWaveComponent() override
    { stopTimer(); }

    void paint(Graphics& g) override
    {
        g.setColour(Colour(TOP_TRIM).withAlpha(0.9f));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), getHeight() * 0.1f);
        drawSineWave(g);
        g.setColour(Colour(DEEP_BLUE));
        g.drawRoundedRectangle(getLocalBounds().reduced(1).toFloat(), getHeight() * 0.1f, 2.f);
    }

    inline void drawSineWave(Graphics& g)
    {
        if (!needsRepaint)
            return;

        auto w = getWidth();

        auto f = phase.advance(0.05f); // inc btw 0->2Pi

        auto rms = src.getAvgRMS();

        float min = getLocalBounds().getBottom();
        float max = getLocalBounds().getY();

        auto map = [min, max](float input)
        {
            return jmap(input, -1.f, 1.f, min, max);
        };

        Path p[6];
        Colour colors[6]{Colour(0xff975755), Colour(0xff977757), Colour(0xff859358),
                         Colour(0xff499481), Colour(0xff516692), Colour(0xff975792) };
        float scale[6]{0.025f, 0.0275f, 0.03f, 0.0325f, 0.035f, 0.0375f};
        float lineThickness[6]{3.f, 2.75f, 2.5f, 1.25f, 1.f, 0.75f};

        auto drawWave = [&](size_t i)
        {
            p[i].startNewSubPath(-1, getLocalBounds().getCentreY());
            std::vector<float> x;
            for (int j = 0; j < w; ++j)
                x.emplace_back((float)j);
            FloatVectorOperations::multiply(x.data(), scale[i], x.size());
            FloatVectorOperations::add(x.data(), -f, x.size()); /*much faster w/ float vec ops!*/
            for (size_t j = 0; j < w; ++j)
                p[i].lineTo(j, map(std::sin(x[j]) * rms[i]));
        };

        for (size_t i = 0; i < 6; ++i)
            drawWave(i);

        for (size_t i = 5; i > 0; --i)
        {
            g.setColour(colors[i]);
            g.strokePath(p[i], PathStrokeType(lineThickness[i]));
        }

        // g.setColour(Colour(0xff975792));
        // g.strokePath(p[5], PathStrokeType(0.75f));
        // g.setColour(Colour(0xff516692));
        // g.strokePath(p[4], PathStrokeType(1.f));
        // g.setColour(Colour(0xff499481));
        // g.strokePath(p[3], PathStrokeType(1.25f));
        // g.setColour(Colour(0xff859358));
        // g.strokePath(p[2], PathStrokeType(2.5f));
        // g.setColour(Colour(0xff977757));
        // g.strokePath(p[1], PathStrokeType(2.75f));
        // g.setColour(Colour(0xff975755));
        // g.strokePath(p[0], PathStrokeType(3.f));

        needsRepaint = false;
    }

    void timerCallback() override
    {
        if (src.newBuf) {
            src.newBuf = false;
            needsRepaint = true;
            repaint(getLocalBounds());
        }
    }

private:
    AudioSource &src;

    dsp::Phase<float> phase;
    std::atomic<bool> needsRepaint = false;
};

} // namespace strix
