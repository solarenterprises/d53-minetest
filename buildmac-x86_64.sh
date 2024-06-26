#!/bin/bash

./cleanup.sh

mkdir build/
cd build/

export PATH="/usr/local/bin:$PATH"

arch -x86_64 /usr/local/bin/cmake .. \
            -DCMAKE_OSX_ARCHITECTURES=x86_64 \
            -DCMAKE_FIND_FRAMEWORK=LAST \
            -DCMAKE_INSTALL_PREFIX=../build/macos_x86_64/ \
            -DRUN_IN_PLACE=FALSE -DENABLE_GETTEXT=TRUE \
            -DINSTALL_DEVTEST=FALSE \
            -DENABLE_LEVELDB=FALSE \
            -DENABLE_REDIS=FALSE

arch -x86_64 /usr/local/bin/cmake -j$(sysctl -n hw.logicalcpu)
make install

# M1 Macs w/ MacOS >= BigSur
# codesign --force --deep -s - macos/minetest.app