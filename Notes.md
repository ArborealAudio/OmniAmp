# TO-DO

- Look into optimizing UI drawing & UI polish
	- benchmark on lower-specced stuff to see how it performs
	- on resize, render each sinewave to an image. keep that on hand until next resize. move and scale each image per the rms levels
~~Auto-scale the waveform display~~ Settled for increasing RMS amplitude

- Working on comp behavior:
	- adjusted Channel so that only threshold lowers for the first 60%, then add up to 2x gain into the sidechain. Past 30%, add up to 3x makeup gain

- Lower initial threshold for comp in Bass mode (& guitar?)

- Menu:
	- OpenGL on/off (windows/linux)
	- HQ on/off
	- Render HQ
	- Comp stereo link
	- Default window size
	- Check for update
  
- Getting different defaults for different modes? Would need to make them separate States if so.
	- What would be the advantages to this? Mainly just getting the Cab to come on if you switch to one of the amps. 
- ~~Pedal is no longer working~~ Had to rewind chowdsp_wdf to a prior commit. Should figure out what caused the issue and submit a PR
	- (but it's working when pulling from upstream on Linux??)
	- could be an issue w/ Neon specifically
- Profile performance & identify potential bottlenecks
- Preset menu (start out with just the ability to save user presets)
- Parameter smoothing
- Save window size to config file
- Activation
	- Compile a beta version that doesn't include this so testers don't have to activate or be given keys
- [BACK-BURNER] Advanced options including
	- stereo/mono switch
		- would this involve fukking with SIMD?
	- stereo/MS linking for compressor
		- currently have stereo linking option for compressor
- [BACK-BURNER] Add triple option for enhancer placement:
	- Pre
	- FX Loop
	- Post cab & verb

# NOTES

Using JUCE 7.0.2

- Cabs: ended up using parallel alternating comb & allpass filters
  
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