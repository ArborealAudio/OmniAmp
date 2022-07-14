# TO-DO

- Working auto-gain processor, probably some distinct class whose gains are supplied by each individual processor
  
  - had an idea to make each processor have its own auto gain. Like you can alt-click it and a little A will show up above the knob, and that knob will compensate for added gain
  
  - How to handle the EQ section? Each EQ band could have an auto gain component, which gets a little complicated for the gtr tonestack bc it's all one filter and the exact db measurement isn't clear. So maybe not the EQ.

- Comp behavior is currently not-ideal. Just gets louder for first 50% and then real GR kicks in after that. Adding too much gain in the early parts of the knob.

- Advanced options including stereo/mono switch, stereo/MS linking for compressor

- Cabs, both the processing and UI features
  
  - New Plan: combine some kind of FDN w basic IIR filtering, create a couple static modes that satisfy some basic cab needs.
  
  - Main trick will be deriving an FDN or some other type of high-pole filter to get the phase-y sound of a cab
    
    - Currently looking at Yeh et al (2008)[dafx08_17] & Harma et al (2000)

- Getting different defaults for different modes? Would need to make them separate States if so.

- SIMD implementation
  
  - If u wanna SIMD the enhancers, you'll need to either write your own Bessel filter or use JUCE IIRs (or some other SIMD-compatible lib)

# NOTES

Using JUCE 7.0.1 (well, trying to, but created a local branch @ 7.0.0 with some fixes for IPP...TBD)