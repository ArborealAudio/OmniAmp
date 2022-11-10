#!/bin/bash

# set up files in the appropriate folder for packaging

set -e

name=$1
version=$2
parent_dir=~/${name}_install/$version

[ ! -d "$parent_dir" ] && mkdir -p $parent_dir

echo "Copying binaries..."

cp -r ../build/${name}_artefacts/Release/VST/$name.dll $parent_dir
cp -r ../build/${name}_artefacts/Release/VST3/$name.vst3 $parent_dir
cp ../manual/${name}.pdf $parent_dir/${name}_manual.pdf

echo "Exited with code $?"