# TO-DO

- Get freq-weighted auto gain for Channel EQ working properly

- Working on comp behavior:
  
  - adjusted Channel so that only threshold lowers for the first 60%, then add up to 1.5 gain into the sidechain. Past 50%, add up to 2x makeup gain

- Advanced options including stereo/mono switch, stereo/MS linking for compressor

- Cabs, both the processing and UI features
  
  - Main trick will be deriving an FDN or some other type of high-pole filter to get the phase-y sound of a cab
    
    - Currently looking at Yeh et al (2008)[dafx08_17] & Harma et al (2000)
    - Basic allpass topology is working ok but there could be more things to do to smooth out the sound. Is sounding a bit metallic and is sucking up a lot of midrange.
  
  - 2X12:
    
    - 1 delay, dTime of 411, fdbk of 0.06
    
    - HP 90 Q 5
    
    - LP1 3500 Q .7
    
    - LP2 5000 Q 1.5
    
    - These aren't really doing anything...need a better way of getting diffusion and complex comb filtering.
  
  - 2x12 V2
    
    - 4 delays, evens are AP
    
    - dTime 0 = 40
    
    - dTime 1 = 57
    
    - dTime 2 = 73
    
    - dTime 3 = 79
    
    - Reduce HP reso to 0.7
    
    - LP 1 6500 Q 0.7
    
    - LP2 5067 Q 1.5
  
  - 4x12:
    
    - 6 delays (2 were nested, then took em out and just used 3?)
    
    - dTime0 = 377
    
    - dTime1 = 365
    
    - dTime2 = 375
    
    - dTime3 = 63
    
    - dTime4 = 126
    
    - dTime5 = 185
    
    - HP 75 Hz Q 1.4
    
    - LP1 2821 Q 1.4
    
    - LP2 4581 Q 1.8
    
    - BP 1700 Q 2
  
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
  
  - Consider how to load the tube stages w/ the cab response. Create some kind of method in the tubes that can accept a buffer of samples from the cab, to be used in modifying the feedback sample for the tube

- Getting different defaults for different modes? Would need to make them separate States if so.
  
  - What would be the advantages to this? Mainly just getting the Cab to come on if you switch to one of the amps. 

- Add triple option for enhancer placement:
  
  - Pre
  
  - FX Loop
  
  - Post cab & verb

- Add sag???

- Figure out mono processing for reverb

# NOTES

Using JUCE 7.0.1