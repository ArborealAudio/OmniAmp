# OmniAmp
## Multi-purpose Audio Tool

### Features:

Too many to list! Check out the webpage or the manual for more detailed info

## Building

Requirements:

|Windows|MacOS|Linux|
|-------|-----|-----|
|Visual Studio|XCode Command Line Tools|See JUCE [Linux deps](https://github.com/juce-framework/JUCE/blob/master/docs/Linux%20Dependencies.md)

To compile:

```sh
    cmake -Bbuild -DPRODUCTION_BUILD=1 -DINSTALL=1 -DNO_LICENSE_CHECK=1
    cmake --build build --config Release
```

## Credits

- [JUCE](https://github.com/juce-framework/juce)
- [chowdsp-wdf](https://github.com/Chowdhury-DSP/chowdsp_wdf)
- [Gin](https://github.com/FigBug/gin)
- [xsimd](https://github.com/xtensor-stack/xsimd)
- [clap-juce-extensions](https://github.com/free-audio/clap-juce-extensions)

See the manual under `manual/OmniAmp.pdf` for licensing info of these libraries

## License

OmniAmp is licensed under the GPLv3 license
