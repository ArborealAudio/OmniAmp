/**
 * EmphasisFilters.h
 * for managing emphasis filters
 */

enum EmphasisFilterType
{
    Low,
    High
};

template <typename T, EmphasisFilterType type = Low>
struct EmphasisFilter
{
    EmphasisFilter(strix::FloatParameter *_amount, strix::FloatParameter *_freq) : amount(_amount), freq(_freq)
    {
        lastFreq = *freq;
    }

    void prepare(const dsp::ProcessSpec &spec)
    {
        SR = spec.sampleRate;
        fIn.prepare(spec);
        fOut.prepare(spec);
        fIn.setType(type == Low ? strix::FilterType::firstOrderLowpass : strix::FilterType::firstOrderHighpass);
        fOut.setType(type == Low ? strix::FilterType::firstOrderLowpass : strix::FilterType::firstOrderHighpass);
        fIn.setCutoffFreq(freq->get());
        fOut.setCutoffFreq(freq->get());

        heap.allocate(spec.maximumBlockSize * spec.numChannels, false);
        wetBlock = dsp::AudioBlock<double>(heap, (size_t)spec.numChannels, (size_t)spec.maximumBlockSize);
    }

    void reset()
    {
        fIn.reset();
        fOut.reset();
    }

    template <typename Block>
    void processIn(Block &block)
    {
        wetBlock.copyFrom(block);

        auto gain = Decibels::decibelsToGain(amount->get()) - 1.f; /* -1 bc of wet/dry schema */
        if (lastFreq != *freq)
            fIn.setCutoffFreq(freq->get());

        auto subBlock = wetBlock.getSubBlock(0, block.getNumSamples());
        fIn.processBlock(subBlock);

        subBlock *= gain;
        block.add(subBlock);
    }

    template <typename Block>
    void processOut(Block &block)
    {
        block.subtract(wetBlock.getSubBlock(0, block.getNumSamples()));
        // wetBlock.copyFrom(block);

        // auto gain = (Decibels::decibelsToGain(-1.f * amount->get())) - 1.f;
        // if (lastFreq != *freq)
        //     fOut.setCutoffFreq(freq->get());

        // auto subBlock = wetBlock.getSubBlock(0, block.getNumSamples());
        // fOut.processBlock(subBlock);

        // subBlock *= gain;
        // block.add(subBlock);

        lastFreq = *freq;
    }

    strix::FloatParameter *amount, *freq;

private:
    strix::SVTFilter<T> fIn, fOut;
    HeapBlock<char> heap;
    dsp::AudioBlock<T> wetBlock;
    double SR = 44100.0;
    float lastFreq;
};