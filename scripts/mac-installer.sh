# codesign & set up files in the appropriate folder for packaging

set -e

name=$1
version=$2

[ ! -d "~/${name}_pkg/$version" ] && mkdir -p ~/${name}_pkg/$version

echo "Copying binaries..."

cp -r ../build/${name}_artefacts/Release/AU ~/${name}_pkg/$version/Components
cp -r ../build/${name}_artefacts/Release/VST ~/${name}_pkg/$version/VST
cp -r ../build/${name}_artefacts/Release/VST3 ~/${name}_pkg/$version/VST3
cp ../manual/${name}.pdf ~/${name}_pkg/$version/${name}_manual.pdf

cd ~/${name}_pkg/$version

echo "Signing binaries..."

codesign -f -o runtime --timestamp -s "Developer ID Application: Alex Riccio (27286D3329)" Components/$name.component VST/$name.vst VST3/$name.vst3

echo "Exited with code $?"