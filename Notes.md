# TO-DO

- Get freq-weighted auto gain for Channel EQ working properly
- Look into optimizing UI drawing & UI polish
	- on resize, render each sinewave to an image. keep that on hand until next resize. move and scale each image per the rms levels
- Working on comp behavior:
	- adjusted Channel so that only threshold lowers for the first 60%, then add up to 2x gain into the sidechain. Past 30%, add up to 3x makeup gain
- Channel EQ isn't working exactly right. Gain isn't symmetric in either direction and they're not adding/subtracting enough gain
- Menu:
	- OpenGL on/off (windows/linux)
	- HQ on/off
	- Default window size
	- About

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
  
- Getting different defaults for different modes? Would need to make them separate States if so.
	- What would be the advantages to this? Mainly just getting the Cab to come on if you switch to one of the amps. 
- Add triple option for enhancer placement:
	- Pre
	- FX Loop
	- Post cab & verb
- SIMD for reverb. Maybe cut channels in half and let SIMD do the rest?
	- Need to basically write a SIMD-compatible buffer, or devise a way to get copyable SIMD buffers that'll work as a drop-in for the current impl
- ~~Pedal is no longer working~~ Had to rewind chowdsp_wdf to a prior commit. Should figure out what caused the issue and submit a PR
	- (but it's working when pulling from upstream on Linux??)
	- could be an issue w/ Neon specifically
- Preset menu (start out with just the ability to save user presets)
- Parameter smoothing
- [BACK-BURNER] Advanced options including stereo/mono switch, stereo/MS linking for compressor

# NOTES

Using JUCE 7.0.2