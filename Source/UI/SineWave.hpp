// SineWave.hpp

#pragma once

namespace strix
{

struct AudioSource
{
    AudioSource() { newBuf = false; }
    ~AudioSource(){}

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
            b.setSize(2, spec.maximumBlockSize);
    }

    template <typename T>
    void getBufferRMS(const AudioBuffer<T>& buf)
    {
        splitBuffers(buf);

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

        newBuf = true;
    }

    inline std::array<float, 6> getAvgRMS()
    {
        std::array<float, 6> b_rms;

        for (int i = 0; i < 6; ++i)
        {
            if (src[i].size() > 0)
                b_rms[i] = std::sqrt(std::accumulate(src[i].begin(), src[i].end(), 0.f) / (float)src[i].size());
            else
                b_rms[i] = std::sqrt(sum[i]);
        }

        return b_rms;
    }

    void setFlag(bool newFlag) { newBuf = newFlag; }

    bool getFlag() const { return newBuf; }

private:
    float rmsSize = 0.f;
    float rms[6] {0.f};
    size_t ptr = 0;
    std::atomic<float> sum[6];
    std::vector<float> src[6];

    dsp::LinkwitzRileyFilter<float> hp[5];
    AudioBuffer<float> band[6];

    std::atomic<bool> newBuf;

    template <typename T>
    void splitBuffers(const AudioBuffer<T>& buf)
    {
        for (auto& b : band)
        {
            b.clear();
            b.makeCopyOf(buf, true);
        }

        for (int i = 0; i < 5; ++i)
        {
            auto hpf = band[i + 1].getWritePointer(0);
            auto lpf = band[i].getWritePointer(0);
            for (auto j = 0; j < buf.getNumSamples(); ++j)
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
        startTimerHz(60);
    }
    ~SineWaveComponent(){ stopTimer(); }

    void paint(Graphics& g) override
    {
        g.setColour(Colour(0xff363536));
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 5.f);

        drawSineWave(g);

        // g.setColour(Colours::black);
        // g.drawRoundedRectangle(getLocalBounds().toFloat(), 5.f, 3.f);
    }

    void drawSineWave(Graphics& g)
    {
        if (!needsRepaint)
            return;
        
        auto w = getWidth();
        // std::vector<float> wave;
        // wave.resize(w);
        // src.readBuffer(wave);
        auto rms = src.getAvgRMS();

        float min = getLocalBounds().getBottom();
        float max = getLocalBounds().getY();

        auto map = [min, max](float input)
        {
            return jmap(input, -1.f, 1.f, min, max);
        };

        Path p[6];

        for (auto& q : p)
            q.startNewSubPath(-1, getLocalBounds().getCentreY());

        for (int i = 0; i < w; ++i)
        {
            p[0].lineTo(i, map(std::sin(i * 0.025f  - f ) * rms[0]));
            p[1].lineTo(i, map(std::sin(i * 0.0275f - f ) * rms[1] /*  * 0.9f */));
            p[2].lineTo(i, map(std::sin(i * 0.03f   - f ) * rms[2]   /*  * 0.8f */));
            p[3].lineTo(i, map(std::sin(i * 0.0325f - f ) * rms[3] /*  * 0.7f */));
            p[4].lineTo(i, map(std::sin(i * 0.035f  - f ) * rms[4]  /*  * 0.6f */));
            p[5].lineTo(i, map(std::sin(i * 0.0375f - f ) * rms[5] /*  * 0.5f */));
        }

        g.setColour(Colour(0xff975792));
        g.strokePath(p[5], PathStrokeType(0.75f));
        g.setColour(Colour(0xff516692));
        g.strokePath(p[4], PathStrokeType(1.f));
        g.setColour(Colour(0xff499481));
        g.strokePath(p[3], PathStrokeType(1.25f));
        g.setColour(Colour(0xff859358));
        g.strokePath(p[2], PathStrokeType(2.5f));
        g.setColour(Colour(0xff977757));
        g.strokePath(p[1], PathStrokeType(2.75f));
        g.setColour(Colour(0xff975755));
        g.strokePath(p[0], PathStrokeType(3.f));

        f += 0.05;
        needsRepaint = false;
    }

    void timerCallback() override
    {
        if (src.getFlag()) {
            src.setFlag(false);
            needsRepaint = true;
            repaint();
        }
    }

private:
    AudioSource &src;

    float f = 0.f;
    bool needsRepaint = false;
};

} // namespace strix