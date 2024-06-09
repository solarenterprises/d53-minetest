#!/bin/bash

rm -r build/

mkdir build/

cd build/

cmake .. \
            -DCMAKE_FIND_FRAMEWORK=LAST \
            -DCMAKE_INSTALL_PREFIX=../build/macos/ \
            -DRUN_IN_PLACE=FALSE -DENABLE_GETTEXT=TRUE \
            -DINSTALL_DEVTEST=FALSE \
            -DENABLE_LEVELDB=FALSE \
            -DENABLE_REDIS=FALSE

cmake -j$(sysctl -n hw.logicalcpu)
make install

# M1 Macs w/ MacOS >= BigSur
# codesign --force --deep -s - macos/minetest.app