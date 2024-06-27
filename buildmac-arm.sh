#!/bin/bash

./cleanup.sh
mkdir build/
cd build/

cmake .. \
            -DDEVELOPMENT_BUILD=FALSE \
            -DCMAKE_OSX_DEPLOYMENT_TARGET=10.14 \
            -DCMAKE_FIND_FRAMEWORK=LAST \
            -DCMAKE_INSTALL_PREFIX=../build/macos/ \
            -DRUN_IN_PLACE=FALSE \
            -DENABLE_GETTEXT=TRUE \
            -DINSTALL_DEVTEST=FALSE \
            -DENABLE_LEVELDB=FALSE \
            -DENABLE_REDIS=FALSE \
            -DBUNDLE_LIBS=/opt/homebrew/lib

cmake -j$(sysctl -n hw.logicalcpu)
make install

cp ../codesign.sh ./macos/codesign.sh
cd macos
./codesign.sh