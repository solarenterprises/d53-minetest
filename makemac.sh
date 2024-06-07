#!/bin/bash

rm -r build/

mkdir build/

cd build/

cmake .. \
            -DCMAKE_BUILD_TYPE=Debug
            -DCMAKE_FIND_FRAMEWORK=LAST \
            -DCMAKE_INSTALL_PREFIX=../build/macos/ \
            -DRUN_IN_PLACE=FALSE -DENABLE_GETTEXT=TRUE \
            -DINSTALL_DEVTEST=FALSE \
            -DENABLE_REDIS=FALSE \
            -DENABLE_LEVELDB=FALSE \
   	        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON