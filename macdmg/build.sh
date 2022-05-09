#!/bin/sh

test -f district53-Installer.dmg && rm district53-Installer.dmg

create-dmg \
  --volname "district53 Installer" \
  --volicon "minetest-icon.icns" \
  --background "web.png" \
  --window-pos 200 120 \
  --window-size 800 400 \
  --icon-size 100 \
  --icon "district53.app" 200 190 \
  --hide-extension "district53.app" \
  --app-drop-link 600 185 \
  "district53-Installer.dmg" \
  "../build/macos/"
  
  
  # --codesign <signature>