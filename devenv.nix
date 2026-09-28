{ pkgs, lib, inputs, config,... }:

let
  # Cross-compilation package set targeting 64-bit Windows via mingw-w64
  mingw = pkgs.pkgsCross.mingwW64;

  # nixGL: wraps a binary so it uses the HOST machine's real GPU driver
  # instead of Nix's generic Mesa build. Needed because Nix's own libGL
  # usually can't find a valid GLXFBConfig on non-NixOS hosts, which shows
  # up as GLX errors and a segfault when raylib tries to create a window.
  nixgl = inputs.nixgl.packages.${pkgs.stdenv.system};
in
{
  # ---------------------------------------------------------------------
  # System packages
  # ---------------------------------------------------------------------
  packages = with pkgs; [
    # --- Core build tooling ---
    cmake
    ninja
    pkg-config
    gdb
    ccache

    # --- Clang toolchain (used for native Linux builds + tooling) ---
    clang
    clang-tools # provides clang-format, clang-tidy, clangd

    # --- Coverage tooling (llvm-profdata, llvm-cov) ---
    # Matches the clang used above, so `-fprofile-instr-generate` /
    # `-fcoverage-mapping` output can be read by these tools.
    llvm

    # --- Native Linux windowing/graphics deps needed by raylib (GLFW) ---
    libGL
    libGLU
    libx11
    libxrandr
    libxinerama
    libxcursor
    libxi
    libxext
    libffi
    mesa

    # Wayland (optional backend - GLFW/raylib can build with -DGLFW_BUILD_WAYLAND=ON)
    wayland
    wayland-protocols
    wayland-scanner
    libxkbcommon

    # --- Windows cross-compilation toolchain (mingw-w64) ---
    # Kept available for manual Windows builds (no helper script).
    mingw.stdenv.cc

    # --- nixGL: source of the host's real GPU driver env vars ---
    # `nixGLIntel` (Mesa - covers Intel/AMD/nouveau) is used below to
    # patch this shell's environment automatically, so the plain built
    # binary just runs. It's a "pure" nixGL package (no host probing at
    # eval time), which keeps `devenv shell` itself reliable for everyone.
    nixgl.nixGLIntel
  ];

  # Make it easy to find the mingw toolchain prefix from scripts if needed
  env.MINGW_TARGET = "x86_64-w64-mingw32";
  env.CPM_SOURCE_CACHE = "${config.devenv.state}/cpm";

  # GLFW dlopen()s most of its platform backend libraries at runtime rather
  # than linking them (libwayland-client, libxkbcommon, libEGL, the X11
  # libs, etc.), so they all need to be on the runtime linker's search
  # path regardless of which backend(s) CMakeLists.txt was built with.
  # Missing any single one shows up as a GLFW init warning immediately
  # followed by a segfault (raylib doesn't check glfwInit()'s return value).
  env.LD_LIBRARY_PATH = lib.makeLibraryPath [
    pkgs.wayland
    pkgs.libxkbcommon
    pkgs.mesa
    pkgs.libGL
    pkgs.libx11
    pkgs.libxrandr
    pkgs.libxinerama
    pkgs.libxcursor
    pkgs.libxi
    pkgs.libxext
    pkgs.libffi
  ];

  # ---------------------------------------------------------------------
  # Convenience scripts
  # ---------------------------------------------------------------------

  # build          -> Debug + tests   (build-debug/)
  # build release  -> Release, no tests (build-release/)
  scripts.build.exec = ''
    set -e
    MODE="''${1:-debug}"

    case "$MODE" in
      debug)
        cmake -S . -B build-debug -G Ninja \
          -DCMAKE_BUILD_TYPE=Debug \
          -DBUILD_TESTING=ON
        cmake --build build-debug
        echo ""
        echo "Binary: ./build-debug/R-Type"
        echo "Tests:  ctest --test-dir build-debug --output-on-failure"
        ;;
      release)
        cmake -S . -B build-release -G Ninja \
          -DCMAKE_BUILD_TYPE=Release \
          -DBUILD_TESTING=OFF
        cmake --build build-release
        echo ""
        echo "Binary: ./build-release/R-Type"
        ;;
      *)
        echo "Usage: build [debug|release]   (default: debug)" >&2
        exit 1
        ;;
    esac
  '';

  # Builds the tests with clang source-based coverage, runs them, prints a
  # per-file summary in the terminal and writes an HTML report.
  scripts.coverage.exec = ''
    set -e
    DIR=build-coverage
    PROFRAW_DIR="$PWD/$DIR/profraw"

    cmake -S . -B "$DIR" -G Ninja \
      -DCMAKE_BUILD_TYPE=Debug \
      -DBUILD_TESTING=ON \
      -DCMAKE_CXX_FLAGS="-fprofile-instr-generate -fcoverage-mapping"
    cmake --build "$DIR" --target rtype_tests

    # Start from a clean slate so stale data doesn't skew the numbers.
    # Absolute path + %p/%m: ctest runs each test in its own process from
    # the build dir, and %m merges the runs into a pool of files.
    rm -rf "$PROFRAW_DIR"
    mkdir -p "$PROFRAW_DIR"
    export LLVM_PROFILE_FILE="$PROFRAW_DIR/rtype-%p-%m.profraw"
    ctest --test-dir "$DIR" --output-on-failure

    llvm-profdata merge -sparse "$PROFRAW_DIR"/*.profraw -o "$DIR/coverage.profdata"

    # Only report on our own code: skip dependencies (CPM cache, Nix store),
    # the tests themselves and the build directories.
    IGNORE='(\.devenv|\.cache|/nix/store|/tests/|/build-[a-z]+/)'

    echo ""
    llvm-cov report "$DIR/rtype_tests" \
      -instr-profile="$DIR/coverage.profdata" \
      -ignore-filename-regex="$IGNORE"

    llvm-cov show "$DIR/rtype_tests" \
      -instr-profile="$DIR/coverage.profdata" \
      -ignore-filename-regex="$IGNORE" \
      -format=html -output-dir="$DIR/html" >/dev/null

    echo ""
    echo "HTML report: $DIR/html/index.html"
  '';

  scripts.clean-all.exec = ''
    rm -rf build*
  '';

  scripts.lint.exec = ''
    run-clang-tidy -p . -fix '^((?!.devenv).)*$'
  '';

  enterShell = ''
    # Patch this shell's env with nixGL's driver-related exports (LD_LIBRARY_PATH
    # etc.) so a plain `./build-debug/R-Type` finds a working OpenGL driver
    # instead of hitting "GLX: No GLXFBConfigs returned" / a segfault, which is
    # what happens if the binary only ever sees Nix's own bare Mesa build.
    # This sources nixGLIntel's exports into the current shell rather than
    # wrapping every invocation, so running the binary stays a plain command.
    if command -v nixGLIntel >/dev/null 2>&1; then
      eval "$(grep -E '^export ' "$(command -v nixGLIntel)")" 2>/dev/null || true
    fi

    export CC="ccache clang"
    export CXX="ccache clang++"

    echo "devenv ready:"
    echo "  build           - Debug build + tests (build-debug/)"
    echo "  build release   - Release build, no tests (build-release/)"
    echo "  coverage        - run tests and show coverage (HTML in build-coverage/html)"
    echo "  clean-all       - remove build directories"
    echo ""
    echo "Run the game directly: ./build-debug/R-Type"
  '';
}
