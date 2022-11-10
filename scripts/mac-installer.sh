# codesign & set up files in the appropriate folder for packaging

set -e

name=$1
version=$2
build=$3
[ $build = "" ] && $build = "Release"

parent_dir=~/${name}_pkg/$version

[ ! -d "$parent_dir" ] && mkdir -p $parent_dir

echo "Copying binaries..."

cp -r ../build/${name}_artefacts/$build/AU $parent_dir/Components
cp -r ../build/${name}_artefacts/$build/VST $parent_dir/VST
cp -r ../build/${name}_artefacts/$build/VST3 $parent_dir/VST3
cp ../manual/${name}.pdf $parent_dir/${name}_manual.pdf

cd $parent_dir

echo "Signing binaries..."

codesign -f -o runtime --timestamp -s "Developer ID Application: Alex Riccio (27286D3329)" Components/$name.component VST/$name.vst VST3/$name.vst3

echo "Exited with code $?"