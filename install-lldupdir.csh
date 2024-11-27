#!/bin/csh -f

xcodebuild -list -project lldupdir.xcodeproj

# rm -rf DerivedData/
xcodebuild -derivedDataPath ./DerivedData/lldupdir -scheme lldupdir  -configuration Release clean build
# xcodebuild -configuration Release -alltargets clean


find ./DerivedData -type f -name lldupdir -perm +444 -ls

set src=./DerivedData/Build/Products/Release/lldupdir
echo File=$src
ls -al $src
cp $src ~/opt/bin/
