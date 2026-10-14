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
#
# Headers included by a source file are still checked according to
# HeaderFilterRegex. Set LINT_HEADERS=1 to also invoke clang-tidy directly on
# every header; this is slower and usually repeats dependency parsing.

set -uo pipefail

# Repo root: git top-level if available, otherwise the current directory.
ROOT="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
CONFIG_FILE="$ROOT/.clang-tidy"

SRC_DIR="${1:-$ROOT/src}"
TEST_DIR="$ROOT/tests"
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

if [[ ! -d "$BUILD_DIR" ]]; then
    echo "Error: build directory '$BUILD_DIR' does not exist." >&2
    exit 1
fi

if [[ ! -d "$TEST_DIR" ]]; then
    echo "Error: test directory '$TEST_DIR' does not exist." >&2
    exit 1
fi

HEADER_FILTER_REGEX="^${ROOT}/(src|tests)/.*"
TIDY_ARGS=(
    --fix
    --fix-errors
    --config-file="$CONFIG_FILE"
    --header-filter="$HEADER_FILTER_REGEX"
    --quiet
)
if [[ -f "$BUILD_DIR/compile_commands.json" ]]; then
    TIDY_ARGS+=(-p "$BUILD_DIR")
else
    echo "Warning: $BUILD_DIR/compile_commands.json not found; running without a compilation database." >&2
fi

FILE_FIND_ARGS=(
    -name '*.c' -o -name '*.cc' -o -name '*.cpp' -o -name '*.cxx'
)
if [[ "${LINT_HEADERS:-0}" == "1" ]]; then
    FILE_FIND_ARGS+=(
        -o -name '*.h' -o -name '*.hpp' -o -name '*.hh' -o -name '*.hxx'
    )
fi

mapfile -d '' FILES < <(
    find "$SRC_DIR" "$TEST_DIR" -type f \
        \( "${FILE_FIND_ARGS[@]}" \) \
        -print0
)

if [[ ${#FILES[@]} -eq 0 ]]; then
    echo "No C/C++ source files found in '$SRC_DIR'."
    exit 0
fi

echo "Running $CLANG_TIDY on ${#FILES[@]} file(s) in '$SRC_DIR' and '$TEST_DIR'..."

FAILED=0
for file in "${FILES[@]}"; do
    echo "==> $file"
    # shellcheck disable=SC2086
    if ! "$CLANG_TIDY" "${TIDY_ARGS[@]}" ${CLANG_TIDY_ARGS:-} "$file"; then
        FAILED=$((FAILED + 1))
    fi
done

if [[ $FAILED -eq 0 ]]; then
    echo "clang-tidy finished with no failures."
    exit 0
fi

echo "clang-tidy reported problems in $FAILED file(s); checking for unresolved problems after fixes." >&2

# A fix run can return non-zero even when every reported diagnostic was fixed.
# Only run the verification pass when the fixing pass reported a problem.
VERIFY_FAILED=0
VERIFY_ARGS=(--config-file="$CONFIG_FILE")
if [[ -f "$BUILD_DIR/compile_commands.json" ]]; then
    VERIFY_ARGS+=(-p "$BUILD_DIR")
fi

echo "Verifying clang-tidy fixes..."
for file in "${FILES[@]}"; do
    echo "==> $file"
    # shellcheck disable=SC2086
    if ! "$CLANG_TIDY" "${VERIFY_ARGS[@]}" ${CLANG_TIDY_ARGS:-} "$file"; then
        VERIFY_FAILED=$((VERIFY_FAILED + 1))
    fi
done

if [[ $VERIFY_FAILED -gt 0 ]]; then
    echo "clang-tidy found unresolved problems in $VERIFY_FAILED file(s)." >&2
    exit 1
fi

echo "clang-tidy fixes resolved all reported problems."
