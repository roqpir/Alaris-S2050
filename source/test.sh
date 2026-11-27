#!/bin/sh
set -eu
cd "$(dirname "$0")"
test_dir=$(mktemp -d)
trap 'rm -rf "$test_dir"' EXIT HUP INT TERM
for test in test-protocol test-session test-scan test-stack test-pdf test-options test-cleanup; do
  xcrun clang -std=c11 -Wall -Wextra -Werror -Wno-deprecated-declarations \
    -fsanitize=address,undefined \
    -I"$(xcrun --show-sdk-path)/usr/include/libxml2" -lxml2 \
    "$test.c" -framework IOKit -framework CoreFoundation -framework CoreGraphics -framework ImageIO -o "$test_dir/$test"
  "$test_dir/$test"
done
