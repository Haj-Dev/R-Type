# R-Type

R-Type is a C++23 multiplayer game inspired by the classic shoot 'em up. It
contains a Raylib client, an Asio-based server, a master server for discovery,
and a shared entity-component-system simulation.

The server is authoritative: clients send input and render snapshots received
from the game server. A master server keeps track of available game servers;
it is not part of the gameplay data path.

## Features

- Master-server discovery and health checks
- Multiple named game servers on one host or across a network
- TCP handshake and lobby protocol
- UDP gameplay input and authoritative snapshots
- Rooms for up to four players
- In-game room chat
- Five-level campaign with a level indicator, server-authoritative transitions,
  and a campaign-complete screen after the final level
- Room voice chat with microphone mute, deafen, and volume controls
- Shared ECS used by the server simulation and client render world
- Unit tests for ECS, networking, controls, rooms, chat, and simulation

## Requirements

The project is built with CMake and uses CPM.cmake to download Raylib, Asio,
miniaudio, Opus, toml++, and GoogleTest during configuration. You do not need
to install those libraries separately.

Third-party dependency licenses and copyright notices are documented in
[`THIRD_PARTY_NOTICES.md`](THIRD_PARTY_NOTICES.md). Keep that file with any
source or binary distribution.

Required:

- CMake 3.20 or newer
- Ninja
- A C++23 compiler: GCC 15+, Clang 17+, or a recent MSVC toolchain
- Git, for dependency downloads performed by CPM.cmake

Optional development tools:

- `ccache`, enabled automatically when available
- LLVM tools such as `clang-format`, `clang-tidy`, `llvm-cov`, and
    `llvm-profdata`

### Linux

On Debian or Ubuntu, install the system toolchain with:

```bash
sudo apt update
sudo apt install build-essential cmake ninja-build git clang clang-format clang-tidy
```

On Arch Linux, the equivalent packages are:

```bash
sudo pacman -S --needed base-devel cmake ninja git clang
```

### Windows

Open PowerShell as Administrator and install the required tools with
`winget`:

```powershell
winget install --id Microsoft.VisualStudio.2022.BuildTools --exact `
    --override "--wait --passive --norestart --add Microsoft.VisualStudio.Workload.VCTools"
winget install --id Kitware.CMake --exact
winget install --id Ninja-build.Ninja --exact
winget install --id Git.Git --exact
```

The Visual Studio command installs the MSVC compiler, Windows SDK, and C++
build tools through the **Desktop development with C++** workload. If the
Visual Studio Installer opens, accept the defaults and let it finish before
continuing.

LLVM/Clang is optional but useful for formatting, linting, and coverage:

```powershell
winget install --id LLVM.LLVM --exact
```

Close and reopen PowerShell after installation, then verify the tools:

```powershell
cmake --version
ninja --version
git --version
clang-format --version
```

For MSVC builds, open **Developer PowerShell for VS 2022** from the Start menu
before configuring the project. This loads `cl.exe`, the Windows SDK, and the
MSVC linker into the environment. From that shell, use the normal CMake
commands:

```powershell
git clone <repository-url>
Set-Location R-Type
.\build.ps1
```

The resulting programs are `build\r-type_server.exe`,
`build\r-type_client.exe`, and `build\rtype_tests.exe`. WSL2 users can follow
the Linux instructions inside WSL instead of installing the native Windows
toolchain.

## Build

Clone the repository and enter its directory:

```bash
git clone <repository-url>
cd R-Type
```

### Recommended helper script

On Linux or WSL, `build.sh` creates a separate build directory for the selected
configuration. On macOS, use the manual CMake commands below or replace
`nproc` in the script with the local CPU-count command:

```bash
./build.sh debug
./build.sh release
```

The resulting executables are:

```text
build/debug/r-type_server
build/debug/r-type_client
build/debug/rtype_tests
```

Use the corresponding `build/release/` paths for a release build.

### Manual CMake build

For a single build directory on any supported platform:

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DBUILD_TESTING=ON
cmake --build build
```

For a release build, replace `Debug` with `Release`. Disable tests with
`-DBUILD_TESTING=OFF`. CMake stores downloaded sources in `.cache/CPM` unless
`CPM_SOURCE_CACHE` is already set.

On Windows PowerShell, the same CMake commands apply. Executables are written
to the configured build directory, commonly `build\r-type_client.exe` and
`build\r-type_server.exe` with Ninja.

## Run locally

Open separate terminals and start the services in this order.

### 1. Start the master server

The master server is the same `r-type_server` executable started without
arguments. It listens on TCP port `7100` by default.

```bash
./build/debug/r-type_server
```

### 2. Start a game server

This process registers itself with the master and listens for clients on TCP
port `7000` and UDP port `7001` by default:

```bash
./build/debug/r-type_server 127.0.0.1 \
    --name server1 \
    --advertise 127.0.0.1 \
    --tcp-port 7000 \
    --udp-port 7001
```

To run another game server on the same machine, give it a different name and
pair of gameplay ports:

```bash
./build/debug/r-type_server 127.0.0.1 \
    --name server2 \
    --advertise 127.0.0.1 \
    --tcp-port 7010 \
    --udp-port 7011
```

The `--advertise` address must be reachable by clients. For a LAN or internet
deployment, use the host's reachable address or DNS name and allow the master,
gameplay TCP, and gameplay UDP ports through the firewall.

### 3. Start a client

The client contacts the master, displays the available servers, and connects to
the selected game server:

```bash
./build/debug/r-type_client 127.0.0.1
```

During a game, clear each level's enemy waves to continue. A three-second
intermission separates the five levels; the final level ends the campaign.

To select a server by name without using the selection screen:

```bash
./build/debug/r-type_client 127.0.0.1 --server server1
```

Use `--master-port <port>` with either a game server or client when the master
is not using port `7100`.

Stop long-running processes with `Ctrl+C`.

## Command-line reference

### Game server

```text
r-type_server
r-type_server <master-ip> [--name <name>] [--advertise <address>]
                            [--tcp-port <port>] [--udp-port <port>]
                            [--master-port <port>]
```

With no arguments it starts the master. With a master address it starts a game
server and registers it with that master.

### Client

```text
r-type_client <master-ip> [--master-port <port>] [--server <name>]
```

## Tests, formatting, and coverage

Run the unit tests with CTest:

```bash
cmake --build build/debug
ctest --test-dir build/debug --output-on-failure
```

Convenience scripts configure a debug build and invoke the relevant target:

```bash
./lint.sh
./format.sh
```

`format.sh` modifies C++ sources in `src/` and `tests/`. The coverage script
requires Clang's coverage tools and writes an HTML report to
`build/coverage/report/index.html`:

```bash
./coverage.sh
```

To remove generated build and dependency artifacts:

```bash
rm -rf build .cache
```

## Project layout

```text
src/Client/   Client input, networking, lobby, chat, and rendering
src/Server/   Authoritative simulation, rooms, and gameplay networking
src/Master/   Server registration, health checks, and discovery
src/Shared/   ECS, components, systems, world state, and wire protocols
tests/        GoogleTest unit and integration tests
docs/         Architecture, protocol, and server-system documentation
assets/       Maps and game assets
```

## Documentation

- [Architecture](docs/architecture.md) - source structure, ECS, and simulation
- [Network protocol](docs/protocol.md) - TCP/UDP packet and lobby formats
- [Server system](docs/server_system.md) - discovery, registration, rooms, and responsibilities
- [Level layout](docs/level-layout.md) - level and map conventions
- [Contributor guide](CONTRIBUTOR.md) - contribution workflow and conventions
- [Comparative study](docs/library-comparative-study.md) - comparative study
- [Voice chat](docs/voice-chat.md) - audio capture, transport, controls, and limitations

## Technical stack

- C++23
- Raylib 6.0 for rendering
- standalone Asio 1.38 for networking
- miniaudio 0.11.26 for microphone capture and playback
- Opus 1.5.2 for voice encoding
- toml++ 3.4 for TOML data
- GoogleTest 1.17 for tests
- CMake, CPM.cmake, and Ninja
