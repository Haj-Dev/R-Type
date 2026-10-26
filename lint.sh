#!/usr/bin/env bash

cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=debug -DBUILD_TESTING=ON
cmake --build build/debug --target lint
