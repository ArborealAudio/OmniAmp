#!/bin/bash

set -e

plugin=OmniAmp

# Install pluginval
# NOTE: We only validate VST3 because AU seems to not work on CI
# Don't really care about validating VST2 and we'd have to pull in the SDK to
# build it anyway
# NOTE: See below re: AUs
# pluginval.exe --validate [pathToPlugin]Validates the file (or IDs for AUs).

plugin_path="build/${plugin}_artefacts/Release/VST3/${plugin}.vst3"

if [[ $RUNNER_OS == 'Windows' ]]; then
	powershell -Command "Invoke-WebRequest -Uri https://github.com/Tracktion/pluginval/releases/download/v1.0.3/pluginval_Windows.zip -OutFile pluginval.zip"
	powershell -Command "Expand-Archive -Path ./pluginval.zip -DestinationPath ."
	pluginval="./pluginval.exe"
else
	wget -O pluginval.zip https://github.com/Tracktion/pluginval/releases/download/v1.0.3/pluginval_${RUNNER_OS}.zip
	unzip pluginval
	pluginval="pluginval.app/Contents/MacOS/pluginval"
fi

echo "Validating $plugin_path"
if $pluginval --strictness-level 10 --validate-in-process --skip-gui-tests --timeout-ms 300000 $plugin_path;
then
	echo "Pluginval successful"
else
	echo "Pluginval failed"
	exit 1
fi

rm -rf pluginval*
