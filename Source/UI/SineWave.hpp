// SineWave.hpp

#pragma once

namespace strix
{

struct AudioSource
{
    AudioSource() { newBuf = false; }
    ~AudioSource(){}

    void setSize(double sampleRate, int numSamples)
    {
        rmsSize = (0.05f * sampleRate) / (float)numSamples;
        fifo.setTotalSize(rmsSize);
        src.assign(rmsSize, 0.f);

        sum = 0.f;

        if (rmsSize > 1.f) {
            ptr %= src.size();
        }
        else
            ptr = 0;

        lp.setup(sampleRate, 300.0, 0.707);
    }

    void copyBuffer(const AudioBuffer<float>& buf)
    {
        auto in = buf.getArrayOfReadPointers();

        auto write = fifo.write(buf.getNumSamples());

        for (auto i = 0; i != write.blockSize1; ++i)
        {
            src[write.startIndex1 + i] = (in[0][write.startIndex1 + i] + in[1][write.startIndex1 + i]) / 2.f;
        }

        for (auto i = 0; i != write.blockSize2; ++i)
        {
            src[write.startIndex2 + i] = (in[0][write.startIndex2 + i] + in[1][write.startIndex2 + i]) / 2.f;
        }

        // lp.process(buf.getNumSamples(), 0, src.data());

        newBuf = true;
    }

    void getBufferRMS(const AudioBuffer<float>& buf)
    {
        rms = (buf.getRMSLevel(0, 0, buf.getNumSamples()) + buf.getRMSLevel(1, 0, buf.getNumSamples())) / 2.f;

        if (src.size() > 0) {
            src[ptr] = rms;
            ptr = (ptr + 1) % src.size();
        }
        else
            sum = rms;

        newBuf = true;
    }

    inline void readBuffer(std::vector<float>& dest)
    {
        // dest.resize(src.size());

        auto read = fifo.read(dest.size());

        for (auto i = 0; i != read.blockSize1; ++i)
        {
            dest[read.startIndex1 + i] = src[read.startIndex1 + i];
        }

        for (auto i = 0; i != read.blockSize2; ++i)
        {
            dest[read.startIndex2 + i] = src[read.startIndex2 + i];
        }
    }

    inline float getAvgRMS()
    {
        if (src.size() > 0)
            return std::sqrt(std::accumulate(src.begin(), src.end(), 0.f) / (float)src.size());

        return std::sqrt(sum);
    }

    void setFlag(bool newFlag) { newBuf = newFlag; }

    bool getFlag() const { return newBuf; }

private:
    AbstractFifo fifo{1024};
    float rmsSize = 0.f;
    float rms = 0.f;
    size_t ptr = 0;
    std::atomic<float> sum;
    std::vector<float> src;

    Dsp::SimpleFilter<Dsp::RBJ::LowPass, 1> lp;

    std::atomic<bool> newBuf;
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
        ColourGradient gradient{Colours::white, 0.f, 0.f, Colour(0xff363536), (float)getLocalBounds().getCentreX(), 0.f, false};
        // g.setColour(Colour(0xff363536));
        g.setGradientFill(gradient);
        g.fillRoundedRectangle(getLocalBounds().toFloat(), 3.f);

        drawSineWave(g);

        g.setColour(Colours::black);
        g.drawRoundedRectangle(getLocalBounds().toFloat(), 3.f, 2.f);
    }

    void drawSineWave(Graphics& g)
    {
        auto w = getWidth();
        std::vector<float> wave;
        wave.resize(w);
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
            q.startNewSubPath(0, getLocalBounds().getCentreY());

        for (int i = 0; i < w; ++i)
        {
            p[0].lineTo(i, map(std::sin(i * 0.025 - f) * rms));
            p[1].lineTo(i, map(std::sin(i * 0.0275 - f) * rms * 0.9));
            p[2].lineTo(i, map(std::sin(i * 0.03 - f) * rms * 0.8));
            p[3].lineTo(i, map(std::sin(i * 0.0325 - f) * rms * 0.7));
            p[4].lineTo(i, map(std::sin(i * 0.035 - f) * rms * 0.6));
            p[5].lineTo(i, map(std::sin(i * 0.0375 - f) * rms * 0.5));
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