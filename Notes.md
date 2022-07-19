# TO-DO

- Working auto-gain processor, probably some distinct class whose gains are supplied by each individual processor
  
  - had an idea to make each processor have its own auto gain. Like you can alt-click it and a little A will show up above the knob, and that knob will compensate for added gain
  
  - How to handle the EQ section? Each EQ band could have an auto gain component, which gets a little complicated for the gtr tonestack bc it's all one filter and the exact db measurement isn't clear. So maybe not the EQ.

- Comp behavior is currently not-ideal. Just gets louder for first 50% and then real GR kicks in after that. Adding too much gain in the early parts of the knob.

- Advanced options including stereo/mono switch, stereo/MS linking for compressor

- Cabs, both the processing and UI features
  
  - Main trick will be deriving an FDN or some other type of high-pole filter to get the phase-y sound of a cab
    
    - Currently looking at Yeh et al (2008)[dafx08_17] & Harma et al (2000)
    - Basic allpass topology is working ok but there could be more things to do to smooth out the sound. Is sounding a bit metallic and is sucking up a lot of midrange.
  
  - 2X12:
    
    - 1 FDN, dTime of 411, fdbk of 0.06
    
    - HP 90 Q 5
    
    - LP1 3500 Q .7
    
    - LP2 5000 Q 1.5
  
  - 4x12:
    
    - 4 FDNs
    
    - dTime0 = 410
    
    - dTime1 = 230 (try 340 too)
    
    - dTime2 = 120 (or 230)
    
    - dTime3 = 326
    
    - HP 75 Hz Q 0.8
    
    - LP1 2821 Q 0.7
    
    - LP2 4191 Q 1.2
    
    - BP 1700 Q 2
  
  - Might look into nested allpasses to smooth out the sound
  
  - Consider how to load the tube stages w/ the cab response. Create some kind of method in the tubes that can accept a buffer of samples from the cab, to be used in modifying the feedback sample for the tube

- Getting different defaults for different modes? Would need to make them separate States if so.

- SIMD implementation
  
  - If u wanna SIMD the enhancers, you'll need to either write your own Bessel filter or use JUCE IIRs (or some other SIMD-compatible lib)

- Add sag???

# NOTES

Using JUCE 7.0.1 (well, trying to, but created a local branch @ 7.0.0 with some fixes for IPP...TBD)