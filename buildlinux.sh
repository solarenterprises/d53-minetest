#!/bin/sh

#sudo apt-get install git g++ make libc6-dev cmake libpng-dev libjpeg-dev libxxf86vm-dev libgl1-mesa-dev libsqlite3-dev libogg-dev libvorbis-dev libopenal-dev libcurl4-gnutls-dev libfreetype6-dev zlib1g-dev libgmp-dev libjsoncpp-dev libzstd-dev libluajit-5.1-dev libssl-dev -y

#rm -rf lib/irrlichtmt
#git clone --depth 1 https://github.com/district53/irrlicht.git lib/irrlichtmt

#rm -rf games/minetest_game
#git clone --depth 1 https://github.com/minetest/minetest_game.git games/minetest_game

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

#rm -rf appimage-builder-x86_64.AppImage
#wget -O appimage-builder-x86_64.AppImage https://github.com/AppImageCrafters/appimage-builder/releases/download/v1.0.0-beta.1/appimage-builder-1.0.0-677acbd-x86_64.AppImage
#chmod +x appimage-builder-x86_64.AppImage

./appimage-builder-x86_64.AppImage --recipe AppImageBuilder.yml


