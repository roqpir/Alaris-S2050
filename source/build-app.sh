#!/bin/sh
set -eu
cd "$(dirname "$0")"
sh build.sh
app="dist/S2050 Scan.app"
mkdir -p "$app/Contents/MacOS" "$app/Contents/Resources"
cp app/Info.plist "$app/Contents/Info.plist"
cp app/AppIcon.icns "$app/Contents/Resources/AppIcon.icns"
cp bin/s2050-probe "$app/Contents/MacOS/s2050-probe"
xcrun clang -fobjc-arc -Wall -Wextra -Werror -arch arm64 -arch x86_64 -mmacosx-version-min=14.0 \
  app/main.m -framework Cocoa -framework PDFKit -framework UniformTypeIdentifiers -o "$app/Contents/MacOS/S2050Scan"
codesign --force --sign - --timestamp=none "$app/Contents/MacOS/s2050-probe"
codesign --force --sign - --timestamp=none "$app"
codesign --verify --deep --strict "$app"
echo "Built: $app"
