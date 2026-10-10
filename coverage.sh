#!/usr/bin/env bash
# Build tests with clang coverage instrumentation, run them,
# and generate an HTML report (build-coverage/report/index.html).
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD="$ROOT/build-coverage"
PROFDIR="$BUILD/profraw"

# Override if your tools are versioned, e.g. LLVM_COV=llvm-cov-18
CC="${CC:-clang}"
CXX="${CXX:-clang++}"
LLVM_COV="${LLVM_COV:-llvm-cov}"
LLVM_PROFDATA="${LLVM_PROFDATA:-llvm-profdata}"

COV_FLAGS="-fprofile-instr-generate -fcoverage-mapping -O0 -g"

# 1. Configure + build (only the test executable)
cmake -S "$ROOT" -B "$BUILD" -G Ninja \
  -DCMAKE_C_COMPILER="$CC" \
  -DCMAKE_CXX_COMPILER="$CXX" \
  -DCMAKE_BUILD_TYPE=Debug \
  -DBUILD_TESTING=ON \
  -DCMAKE_C_FLAGS="$COV_FLAGS" \
  -DCMAKE_CXX_FLAGS="$COV_FLAGS"
cmake --build "$BUILD" --target rtype_tests

# 2. Run tests, collecting raw profiles
rm -rf "$PROFDIR" && mkdir -p "$PROFDIR"
LLVM_PROFILE_FILE="$PROFDIR/%p-%m.profraw" "$BUILD/rtype_tests"

# 3. Merge profiles
"$LLVM_PROFDATA" merge -sparse "$PROFDIR"/*.profraw -o "$BUILD/coverage.profdata"

# 4. Report (only our own code: src/, excluding tests and dependencies)
IGNORE='(_deps|\.cache|/tests/|/usr/|/nix/)'

"$LLVM_COV" report "$BUILD/rtype_tests" \
  -instr-profile="$BUILD/coverage.profdata" \
  -ignore-filename-regex="$IGNORE" \
  "$ROOT/src"

"$LLVM_COV" show "$BUILD/rtype_tests" \
  -instr-profile="$BUILD/coverage.profdata" \
  -ignore-filename-regex="$IGNORE" \
  -format=html \
  -output-dir="$BUILD/report" \
  -show-line-counts-or-regions \
  "$ROOT/src"

echo
echo "HTML report: $BUILD/report/index.html"
