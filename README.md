# R-Type
R-Type is a C++ multiplayer game with a Raylib client, an Asio server, and a shared entity-component-system simulation.

## Environment

This project is built with CMake and uses CPM.cmake to fetch third-party dependencies (raylib, Asio, GoogleTest) at configure time. A POSIX-like environment (Linux, macOS) or Windows is supported.

Required tools (minimum):

- CMake 3.20+
- Ninja (recommended generator)
- A C++23-capable compiler (Clang/LLVM or GCC)
- optional: LLVM**21** for formatting and linting (clang-format, clang-tidy)

On Linux/macOS you typically need: a system C/C++ toolchain (gcc/clang), CMake and Ninja.
On Windows for native desktop builds you need:

- Visual Studio Build Tools (MSVC) for system libraries
- LLVM/Clang (tested) or MSVC (untested) compiler toolchain
- Ninja (for the CMake Ninja generator)

## Building the project

A minimal out-of-source build (cross-platform):

```bash
cmake -S . -B build -G Ninja
cmake --build build --config Release
```

Notes:
- CMake will download and configure dependencies via CPM at configure time; no manual dependency installation is required for the bundled libraries.
- You can set CMake options (e.g. `-B build -DCMAKE_BUILD_TYPE=Debug`) as you normally would.

### Windows (native) build notes

For native Windows desktop builds use the Ninja generator and a supported compiler. Example (PowerShell / cmd):

```powershell
cmake -S . -B build -G Ninja
cmake --build build --config Release
```

Requirements for Windows desktop builds:
- Visual Studio Build Tools (MSVC) or the full Visual Studio with C++ workload installed. This provides the MSVC toolchain required for building desktop executables.
- If you want to build with Clang/LLVM on Windows, install LLVM/Clang and make sure the clang/clang++ executables are on your PATH and a compatible toolchain file or generator is selected.
- Ninja installed and available on PATH (CMake uses it as the generator above).

If you are building on Windows from WSL2, the Linux instructions (installing system packages and using CMake/Ninja inside WSL) are applicable instead.

## Cleaning

Remove the CMake build dir to clean artifacts:

```bash
rm -rf build
```

(There are helper scripts like `build.sh`, `lint.sh`, `format.sh` and `coverage.sh` present in the repository for CI or convenience; you can use them if they match your workflow.)

## Running

Start the server, then run a client and pass the server IP:

```bash
./build/R-Type_server
./build/R-Type_client 127.0.0.1
```

On Windows the executables are produced under `build/` with a `Release` or `Debug` configuration subfolder depending on the generator/IDE; adjust the paths accordingly (for example `build/R-Type_client.exe` or `build/Release/R-Type_client.exe`).

## Tests and linting

Run the test suite (when `BUILD_TESTING=ON`) with CTest or the provided script:

```bash
cmake --build build --config Release --target all
ctest --test-dir build --output-on-failure
# or use the included wrapper
./coverage.sh
```

Run lint checks with the repository helper:

```bash
./lint.sh
```

## Technical stack

- C++ (C++23)
- raylib (game rendering)
- Asio (networking)
- GoogleTest (unit tests)
- CMake + CPM.cmake (build + dependency fetching)
- Ninja (recommended generator)
