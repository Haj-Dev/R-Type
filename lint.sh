#!/usr/bin/env bash
# Run clang-tidy on every C/C++ file in the src directory.
#
# Uses the .clang-tidy config and compile_commands.json at the repo root.
#
# Usage: ./run-clang-tidy.sh [src_dir] [build_dir]
#   src_dir   : directory to scan               (default: <repo root>/src)
#   build_dir : dir with compile_commands.json  (default: <repo root>)
#
# Fixes suggested by clang-tidy are applied in place (--fix).
# Extra clang-tidy arguments can be passed via CLANG_TIDY_ARGS, e.g.:
#   CLANG_TIDY_ARGS="--fix-errors" ./run-clang-tidy.sh

set -uo pipefail

# Repo root: git top-level if available, otherwise the current directory.
ROOT="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
CONFIG_FILE="$ROOT/.clang-tidy"

SRC_DIR="${1:-$ROOT/src}"
BUILD_DIR="${2:-$ROOT}"
CLANG_TIDY="${CLANG_TIDY:-clang-tidy}"

if ! command -v "$CLANG_TIDY" >/dev/null 2>&1; then
    echo "Error: '$CLANG_TIDY' not found in PATH." >&2
    exit 127
fi

if [[ ! -f "$CONFIG_FILE" ]]; then
    echo "Error: config file '$CONFIG_FILE' not found." >&2
    exit 1
fi

if [[ ! -d "$SRC_DIR" ]]; then
    echo "Error: source directory '$SRC_DIR' does not exist." >&2
    exit 1
fi

# Use the compilation database if available; otherwise fall back to
# clang-tidy's default behaviour (looks for .clang-tidy / flags after --).
TIDY_ARGS=(--fix --config-file="$CONFIG_FILE")
if [[ -f "$BUILD_DIR/compile_commands.json" ]]; then
    TIDY_ARGS+=(-p "$BUILD_DIR")
else
    echo "Warning: $BUILD_DIR/compile_commands.json not found; running without a compilation database." >&2
fi

# Collect files (NUL-delimited to be safe with spaces in names).
# Add header extensions (h, hpp, hh, hxx) to the list if you want them checked directly.
mapfile -d '' FILES < <(
    find "$SRC_DIR" -type f \
        \( -name '*.c' -o -name '*.cc' -o -name '*.cpp' -o -name '*.cxx' \) \
        -print0
)

if [[ ${#FILES[@]} -eq 0 ]]; then
    echo "No C/C++ source files found in '$SRC_DIR'."
    exit 0
fi

echo "Running $CLANG_TIDY on ${#FILES[@]} file(s) in '$SRC_DIR'..."

FAILED=0
for file in "${FILES[@]}"; do
    echo "==> $file"
    # shellcheck disable=SC2086
    if ! "$CLANG_TIDY" "${TIDY_ARGS[@]}" ${CLANG_TIDY_ARGS:-} "$file"; then
        FAILED=$((FAILED + 1))
    fi
done

if [[ $FAILED -gt 0 ]]; then
    echo "clang-tidy reported problems in $FAILED file(s)." >&2
    exit 1
fi

echo "clang-tidy finished with no failures."
