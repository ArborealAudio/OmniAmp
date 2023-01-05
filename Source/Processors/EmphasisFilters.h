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
        const auto numSamples = block.getNumSamples();
        FloatVectorOperations::copy(wetBlock.getChannelPointer(0), block.getChannelPointer(0), numSamples);
        if (block.getNumChannels() > 1 && wetBlock.getNumChannels() > 1)
            FloatVectorOperations::copy(wetBlock.getChannelPointer(1), block.getChannelPointer(1), numSamples);

        auto gain = Decibels::decibelsToGain(amount->get()) - 1.f; /* -1 bc of wet/dry schema */
        if (lastFreq != *freq)
            fIn.setCutoffFreq(freq->get());

        fIn.processChannel(wetBlock.getChannelPointer(0), 0, numSamples);
        if (block.getNumChannels() > 1 && wetBlock.getNumChannels() > 1)
            fIn.processChannel(wetBlock.getChannelPointer(1), 1, numSamples);

        wetBlock *= gain;
        FloatVectorOperations::add(block.getChannelPointer(0), wetBlock.getChannelPointer(0), numSamples);
        if (block.getNumChannels() > 1 && wetBlock.getNumChannels() > 1)
            FloatVectorOperations::add(block.getChannelPointer(1), wetBlock.getChannelPointer(1), numSamples);
    }

    template <typename Block>
    void processOut(Block &block)
    {
        FloatVectorOperations::subtract(block.getChannelPointer(0), wetBlock.getChannelPointer(0), block.getNumSamples());
        if (block.getNumChannels() > 1 && wetBlock.getNumChannels() > 1)
            FloatVectorOperations::subtract(block.getChannelPointer(1), wetBlock.getChannelPointer(1), block.getNumSamples());
        // block.subtract(wetBlock.getSubBlock(0, block.getNumSamples()));
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
    float lastFreq = 1.f;
};