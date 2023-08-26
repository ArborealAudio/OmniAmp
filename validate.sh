#!/bin/bash

set -e

plugin=OmniAmp

# Install pluginval
wget -O pluginval.zip https://github.com/Tracktion/pluginval/releases/download/v1.0.3/pluginval_${RUNNER_OS}.zip
unzip pluginval

if [[ $RUNNER_OS == 'Windows' ]]; then
	pluginval="./pluginval.exe"
	plugins=("build/${plugin}_artefacts/Release/VST3/${plugin}.vst3")
fi
if [[ $RUNNER_OS == 'macOS' ]]; then
	pluginval="pluginval.app/Contents/MacOS/pluginval"
	plugins=(
		"build/${plugin}_artefacts/Release/VST3/${plugin}.vst3"
		"build/${plugin}_artefacts/Release/AU/${plugin}.component"
	)
fi

for p in ${plugins[@]}; do
	echo "Validating $p"
	if $pluginval --strictness-level 10 --validate-in-process --validate $p --skip-gui-tests --timeout-ms 300000;
	then
		echo "Pluginval successful"
	else
		echo "Pluginval failed"
		exit 1
	fi
done

rm -rf pluginval*
