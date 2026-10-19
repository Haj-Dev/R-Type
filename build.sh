#!/usr/bin/env bash
set -euo pipefail

BUILD_DIR="build"
CONFIG="${1:-debug}"

if [[ "$CONFIG" != "debug" && "$CONFIG" != "release" ]]; then
    echo "Usage: $0 {debug|release}"
    exit 1
fi

cmake -B "$BUILD_DIR/$CONFIG" -DCMAKE_BUILD_TYPE="$CONFIG" -G Ninja
cmake --build "$BUILD_DIR/$CONFIG" -j"$(nproc)"

cp "$BUILD_DIR/$CONFIG/compile_commands.json" .
