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
        fifo.setTotalSize(numSamples);
        src.resize(numSamples);

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

        lp.process(buf.getNumSamples(), 0, src.data());

        newBuf = true;
    }

    void readBuffer(std::vector<float>& dest)
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

    void setFlag(bool newFlag) { newBuf = newFlag; }

    bool getFlag() const { return newBuf; }

private:
    AbstractFifo fifo{1024};
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
        drawSineWave(g);

        g.setColour(Colours::black);
        g.drawRoundedRectangle(getLocalBounds().toFloat(), 3.f, 2.f);
    }

    void drawSineWave(Graphics& g)
    {
        auto w = getWidth();
        std::vector<float> wave;
        wave.resize(w);
        src.readBuffer(wave);

        g.setColour(Colours::white);

        Path p;

        float min = getLocalBounds().getBottom();
        float max = getLocalBounds().getY();

        auto map = [min, max](float input)
        {
            return jmap(input, -1.f, 1.f, min, max);
        };

        p.startNewSubPath(0, wave[0]);

        for (int i = 0; i < w; ++i)
        {
            p.lineTo(i, map(wave[i]));
        }

        g.strokePath(p, PathStrokeType(2.f));
    }

    void timerCallback() override
    {
        if (src.getFlag()) {
            src.setFlag(false);
            repaint();
        }
    }

private:
    AudioSource &src;
};

} // namespace strix