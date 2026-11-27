#!/bin/sh
set -eu
cd "$(dirname "$0")"
mkdir -p bin
xcrun clang -std=c11 -Wall -Wextra -Werror -Wno-deprecated-declarations \
  -arch arm64 -arch x86_64 -mmacosx-version-min=14.0 \
  probe.c -I"$(xcrun --show-sdk-path)/usr/include/libxml2" -lxml2 -framework IOKit -framework CoreFoundation -framework CoreGraphics -framework ImageIO -o bin/s2050-probe
