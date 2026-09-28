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
    libxkbcommon

    # --- Windows cross-compilation toolchain (mingw-w64) ---
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
  scripts.build-linux.exec = ''
    set -e
    cmake -S . -B build-linux -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build-linux
  '';

  scripts.build-windows.exec = ''
    set -e
    cmake -S . -B build-windows -G Ninja \
      -DCMAKE_TOOLCHAIN_FILE="$DEVENV_ROOT/cmake/toolchain-mingw.cmake" \
      -DCMAKE_BUILD_TYPE=Release
    cmake --build build-windows
  '';

  scripts.clean-all.exec = ''
    rm -rf build-linux build-windows
  '';

  enterShell = ''
    # Patch this shell's env with nixGL's driver-related exports (LD_LIBRARY_PATH
    # etc.) so a plain `./build-linux/R-Type` finds a working OpenGL driver
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
    echo "  build-linux    - configure + build native Linux binary"
    echo "  build-windows  - configure + build Windows binary via mingw-w64"
    echo "  clean-all      - remove build directories"
    echo ""
    echo "Run the Linux build directly: ./build-linux/R-Type"
  '';
}
