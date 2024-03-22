#!/bin/bash

rm -r build/

mkdir build/

cd build/

cmake .. \
            -DCMAKE_OSX_DEPLOYMENT_TARGET=10.14 \
            -DCMAKE_FIND_FRAMEWORK=LAST \
            -DCMAKE_INSTALL_PREFIX=../build/macos/ \
            -DRUN_IN_PLACE=FALSE -DENABLE_GETTEXT=TRUE \
            -DINSTALL_DEVTEST=TRUE \
            -DENABLE_REDIS=FALSE
          cmake --build . -j$(sysctl -n hw.logicalcpu)
make install
