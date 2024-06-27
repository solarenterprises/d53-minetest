#!/bin/sh

# WARNING: You may have to run Clean in Xcode after changing CODE_SIGN_IDENTITY! 

TARGET_BUILD_DIR=District53.app
FRAMEWORKS_FOLDER_PATH="Contents/Frameworks"

FRAMEWORKS_DIR="${TARGET_BUILD_DIR}/${FRAMEWORKS_FOLDER_PATH}"
if [ -d "$FRAMEWORKS_DIR" ] ; then
    find "${FRAMEWORKS_DIR}" -depth -type d -name "*.framework" -or -name "*.dylib" -or -name "*.bundle" | sed -e "s/\(.*framework\)/\1\/Versions\/A\//" > sign_list.txt
fi

# Change the Internal Field Separator (IFS) so that spaces in paths will not cause problems below.
SAVED_IFS=$IFS
IFS=$(echo -en "\n\b")

# Loop through all items.
while read -r ITEM;
do
    echo "Signing '${ITEM}'"
    codesign --force --verbose --timestamp --sign "Developer ID Application: Solar Network Finance LTD (J3T8W3347Y)" "${ITEM}"
    RESULT=$?
    if [[ $RESULT != 0 ]] ; then
        echo "Failed to sign '${ITEM}'."
        IFS=$SAVED_IFS
        exit 1
    fi
done < sign_list.txt

rm sign_list.txt

# Restore $IFS.
IFS=$SAVED_IFS

codesign --force --verbose --timestamp -i "io.solarnetwork.district53" --options runtime --sign "Developer ID Application: Solar Network Finance LTD (J3T8W3347Y)" "${TARGET_BUILD_DIR}/Contents/MacOS/District53"

ditto -c -k --sequesterRsrc --keepParent District53.app District53.zip
xcrun notarytool submit District53.zip --keychain-profile "notary-credentials" --wait
xcrun stapler staple District53.app
rm District53.zip
ditto -c -k --sequesterRsrc --keepParent District53.app District53.zip