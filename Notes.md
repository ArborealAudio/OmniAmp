# TO-DO
- Change the default of the Bass knob to whatever it is at 50%

- Working auto-gain processor, probably some distinct class whose gains are supplied by each individual processor
  
  - had an idea to make each processor have its own auto gain. Like you can alt-click it and a little A will show up above the knob, and that knob will compensate for added gain
  
  - How to handle the EQ section? Each EQ band could have an auto gain component, which gets a little complicated for the gtr tonestack bc it's all one filter and the exact db measurement isn't clear. So maybe not the EQ.

- Comp behavior is currently not-ideal. Just gets louder for first 50% and then real GR kicks in after that. Adding too much gain in the early parts of the knob.

- Advanced options including stereo/mono switch, stereo/MS linking for compressor

- Cabs, both the processing and UI features
  
  - much to be done here. Multiple mic options, movable mics? Or shall we stick to just a few different cabs, keep it basic?
  
  - so far have added a guitar cab and a bass cab, not really sure how good they sound yet
  
  - change convo proc to a file loader so you can test em out?

- Getting different defaults for different modes? Would need to make them separate States if so.

- SIMD implementation
  
  - mostly just the enhancers left to optimize. Leaving compressor since it seems like a PITA but definitely could see that being possible too.
  - Glitches w/ NEON? Intel version not scanning in Plugin Doctor? Tune in next time...

# NOTES

Using JUCE 7.0.1 (well, trying to, but created a local branch @ 7.0.0 with some fixes for IPP...TBD)