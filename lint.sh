#!/usr/bin/env bash

cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=debug -DBUILD_TESTING=ON
cmake --build build --target lint
