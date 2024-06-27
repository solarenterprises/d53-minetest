#!/bin/bash

./cleanup.sh

mkdir build/
cd build/

export PATH="/usr/local/bin:$PATH"

arch -x86_64 /usr/local/bin/cmake .. \
            -DDEVELOPMENT_BUILD=FALSE \
            -DCMAKE_OSX_DEPLOYMENT_TARGET=10.14 \
            -DCMAKE_OSX_ARCHITECTURES=x86_64 \
            -DCMAKE_FIND_FRAMEWORK=LAST \
            -DCMAKE_INSTALL_PREFIX=../build/macos_x86_64/ \
            -DRUN_IN_PLACE=FALSE \
            -DENABLE_GETTEXT=TRUE \
            -DINSTALL_DEVTEST=FALSE \
            -DENABLE_LEVELDB=FALSE \
            -DENABLE_REDIS=FALSE \
            -DBUNDLE_LIBS=/usr/local/lib

arch -x86_64 /usr/local/bin/cmake -j$(sysctl -n hw.logicalcpu)
make install

cp ../codesign.sh ./macos_x86_64/codesign.sh
cd macos_x86_64
./codesign.sh

# xcrun notarytool store-credentials "notary-credentials" --apple-id "sami@solarenterprises.com" --team-id "J3T8W3347Y" --password "bobx-upah-pnfj-ldlw"
# xcrun notarytool submit District53.zip --keychain-profile "notary-credentials" --wait

# xcrun notarytool log <submission-id> --keychain-profile "notary-credentials"
# xcrun notarytool log e43e1f2e-2dbd-420f-bf7c-d10fbd44b78f --keychain-profile "notary-credentials"