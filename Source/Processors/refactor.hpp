namespace Processors {
enum ProcessorType {
    Guitar,
    Bass,
    Channel
};

enum GuitarMode {
    GammaRay,
    Sunbeam,
    Moonbeam,
    XRay
};

enum BassMode {
    Cobalt,
    Emerald,
    Quartz
};

enum ChannelMode {
    Modern,
    Vintage,
    Biamp,
};

struct PreampProcess {
#if USE_SIMD
    typedef void (*Process)(strix::AudioBlock<vec>&);
#else
    typedef void (*Process)(dsp::AudioBlock<double>&);
#endif
    Process proc;
    bool shouldBypass = false;
};

struct Preamp {
    std::vector<PreampProcess*> procs;

#if USE_SIMD
    void process(strix::AudioBlock<vec>& block)
    {
        for (auto p : procs)
            if (!p->shouldBypass)
                p->proc(block);
    }
#else
    void process(dsp::AudioBlock<double>& block)
    {
        for (auto p : procs)
            if (!p->shouldBypass)
                p->proc(block);
    }
#endif
};

struct ProcessorData {
};

} // namespace Processors
