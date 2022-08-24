# TO-DO

- Get freq-weighted auto gain for Channel EQ working properly

- Look into optimizing UI drawing
  
  - on resize, render each sinewave to an image. keep that on hand until next resize. move and scale each image per the rms levels

- Working on comp behavior:
  
  - adjusted Channel so that only threshold lowers for the first 60%, then add up to 1.5 gain into the sidechain. Past 50%, add up to 2x makeup gain
  - Get meter to display full 24dB (just need to get the label to read that high)

- [BACK-BURNER] Advanced options including stereo/mono switch, stereo/MS linking for compressor

- Cabs, both the processing and UI features
  
  - ~~Main trick will be deriving an FDN or some other type of high-pole filter to get the phase-y sound of a cab~~
    
    - ~~Currently looking at Yeh et al (2008)[dafx08_17] & Harma et al (2000)~~
  
  - Ended up using parallel alternating comb & allpass filters
  
  - 2x12 V2
    
    - 4 delays, evens are AP
    
    - dTime 0 = 40
    
    - dTime 1 = 57
    
    - dTime 2 = 73
    
    - dTime 3 = 79
    
    - Reduce HP reso to 0.7
    
    - LP 1 6500 Q 0.7
    
    - LP2 5067 Q 1.5
  
  - 4x12 V2
    
    - 4 delays
    
    - dTime0 = 57
    
    - dTime1 = 24
    
    - dTime2 = 40
    
    - dTime3 = 79
    
    - HP 70 Q 1.0
    
    - LP1 5011 Q 0.7
    
    - LP2 4500 Q 1.29
    
    - Lowshelf 232
  
  - 6x10
    
    - 4 Delays after all
    
    - dTime0 = 40
    
    - dTime1 71
    
    - dTime2 53
    
    - dTime3 15
    
    - HP 80 Q 2
    
    - LP1 6033 Q 1.21
    
    - LP2 3193 Q 1.5
    
    - LS 307
  
  - ~~Consider how to load the tube stages w/ the cab response. Create some kind of method in the tubes that can accept a buffer of samples from the cab, to be used in modifying the feedback sample for the tube~~ Who cares?

- Getting different defaults for different modes? Would need to make them separate States if so.
  
  - What would be the advantages to this? Mainly just getting the Cab to come on if you switch to one of the amps. 

- Add triple option for enhancer placement:
  
  - Pre
  
  - FX Loop
  
  - Post cab & verb

- SIMD for reverb. Maybe cut channels in half and let SIMD do the rest?

- Pedal is no longer working

- Parameter smoothing

# NOTES

Using JUCE 7.0.2