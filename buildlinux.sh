#!/bin/sh

cmake . -DRUN_IN_PLACE=FALSE -DBUILD_SERVER=FALSE -DBUILD_CLIENT=TRUE -DBUILD_UNITTESTS=FALSE -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)
rm AppDir -rf
mkdir AppDir
make install DESTDIR=AppDir

mkdir AppDir/usr/local/share/district53/misc

cp misc/district53-xorg-icon-128.png AppDir/usr/local/share/district53/misc/
cp misc/AppRun AppDir/
cp misc/district53.desktop AppDir/
cp misc/district53.png AppDir/

#wget -O appimage-builder-x86_64.AppImage https://github.com/AppImageCrafters/appimage-builder/releases/download/v1.0.0-beta.1/appimage-builder-1.0.0-677acbd-x86_64.AppImage
#chmod +x appimage-builder-x86_64.AppImage

./appimage-builder-x86_64.AppImage --recipe AppImageBuilder.yml


