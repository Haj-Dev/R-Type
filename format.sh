#!/usr/bin/env bash
# Run clang-format in place (-i) on every C/C++ source and header in src.
#
# Formatting follows the .clang-format file at the repo root.
#
# Usage: ./run-clang-format.sh [src_dir]
#   src_dir : directory to scan (default: <repo root>/src)
#
# Extra clang-format arguments can be passed via CLANG_FORMAT_ARGS.

set -uo pipefail

# Repo root: git top-level if available, otherwise the current directory.
ROOT="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
STYLE_FILE="$ROOT/.clang-format"

SRC_DIR="${1:-$ROOT/src}"
CLANG_FORMAT="${CLANG_FORMAT:-clang-format}"

if ! command -v "$CLANG_FORMAT" >/dev/null 2>&1; then
    echo "Error: '$CLANG_FORMAT' not found in PATH." >&2
    exit 127
fi

if [[ ! -f "$STYLE_FILE" ]]; then
    echo "Error: style file '$STYLE_FILE' not found." >&2
    exit 1
fi

if [[ ! -d "$SRC_DIR" ]]; then
    echo "Error: source directory '$SRC_DIR' does not exist." >&2
    exit 1
fi

# Collect files (NUL-delimited to be safe with spaces in names).
mapfile -d '' FILES < <(
    find "$SRC_DIR" -type f \
        \( -name '*.c' -o -name '*.cc' -o -name '*.cpp' -o -name '*.cxx' \
           -o -name '*.h' -o -name '*.hh' -o -name '*.hpp' -o -name '*.hxx' \) \
        -print0
)

if [[ ${#FILES[@]} -eq 0 ]]; then
    echo "No C/C++ files found in '$SRC_DIR'."
    exit 0
fi

echo "Running $CLANG_FORMAT on ${#FILES[@]} file(s) in '$SRC_DIR'..."

FAILED=0
for file in "${FILES[@]}"; do
    echo "==> $file"
    # shellcheck disable=SC2086
    if ! "$CLANG_FORMAT" -i --style="file:$STYLE_FILE" ${CLANG_FORMAT_ARGS:-} "$file"; then
        FAILED=$((FAILED + 1))
    fi
done

if [[ $FAILED -gt 0 ]]; then
    echo "clang-format failed on $FAILED file(s)." >&2
    exit 1
fi

echo "clang-format finished with no failures."
