# TO-DO

- Working auto-gain processor, probably some distinct class whose gains are supplied by each individual processor

- Advanced options including stereo/mono switch, stereo/MS linking for compressor

- Cabs, both the processing and UI features
  
  - much to be done here. Multiple mic options, movable mics? Or shall we stick to just a few different cabs, keep it basic?
  
  - so far have added a guitar cab and a bass cab, not really sure how good they sound yet

- Getting different defaults for different modes? Would need to make them separate States if so.

- SIMD implementation
  
  - mostly just the enhancers left to optimize. Leaving compressor since it seems like a PITA but definitely could see that being possible too.
  - Glitches w/ NEON? Intel version not scanning in Plugin Doctor? Tune in next time...

# NOTES

Using JUCE 7.0.1 (well, trying to, but created a local branch @ 7.0.0 with some fixes for IPP...TBD)