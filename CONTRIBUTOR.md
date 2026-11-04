# Contributing to R-Type

This guide gets you from a fresh clone to a merged pull request. It summarises
the documents in [`docs/`](docs/) and links to them for details:

| Document | Read it for |
| --- | --- |
| [`docs/architecture.md`](docs/architecture.md) | Source layout, simulation loop, the ECS |
| [`docs/protocol.md`](docs/protocol.md) | UDP gameplay packets and the TCP handshake |
| [`docs/server_system.md`](docs/server_system.md) | Master server, rooms, lobby and control-plane protocol |
| [`docs/level-layout.md`](docs/level-layout.md) | Level `.toml` file format |
| [`README.md`](README.md) | Tools, build and run commands |

## 1. Set up

Required: CMake 3.20+, Ninja, a C++23 compiler (Clang or GCC). Optional but
needed for formatting and linting: LLVM **21** (`clang-format`, `clang-tidy`).
On Windows you also need the Visual Studio Build Tools (see the README).

Dependencies (raylib, Asio, tomlplusplus, GoogleTest) are fetched by CPM at
configure time, so the first configure needs network access. Downloaded
sources are cached in `.cache/CPM`.

## 2. Build, run, test

```bash
./build.sh debug            # configure + build into build/debug (or: release)
ctest --test-dir build/debug --output-on-failure
./build/debug/rtype_tests   # or run the test binary directly
```

Run the whole stack locally (ports and options are described in
[`docs/server_system.md`](docs/server_system.md#running)):

```bash
./build/debug/r-type_server                                   # master, TCP 7100
./build/debug/r-type_server 127.0.0.1 --name server1 --advertise 127.0.0.1 \
    --tcp-port 7000 --udp-port 7001                           # game server
./build/debug/r-type_client 127.0.0.1                         # client
```

Several clients can connect to the same game server (up to four players per
room). Other helper scripts:

| Script | Purpose |
| --- | --- |
| `./format.sh` | Run `clang-format` on `src/` and `tests/` |
| `./lint.sh` | Run `clang-tidy` (applies fixes with `-fix`) |
| `./coverage.sh` | Build with clang coverage, run the tests, write an HTML report to `build/coverage/report/` |

All of our targets compile with `-Wall -Wextra -Wpedantic -pedantic-errors`,
so warnings are build errors. Fix them rather than silencing them.

## 3. Project layout

```text
src/
  Master/   registry and discovery (control plane)
  Server/   authoritative simulation, rooms, networking
  Client/   input, rendering, networking, UI
  Shared/   code used by both sides
    Components/  passive ECS components (SC* structs)
    ECS/         entities, component pools, registry
    Systems/     shared systems (gameplay, rendering interface, chat)
    Lobby*.hpp   lobby state and wire format
    Types.hpp    UDP gameplay wire format
tests/      GoogleTest suites (one file per area)
docs/       design and protocol documents
assets/     textures, sounds, level data
```

Every `.cpp` under `src/` (except `main.cpp`) is compiled into the
`rtype_core` library, which the server, client and tests link against. New
files are picked up automatically (the CMake glob is re-checked on build);
files under a directory named `old/` are ignored. Tests are every `.cpp` in
`tests/`.

## 4. How the game works

- **Server is authoritative.** It runs the simulation at a fixed 60 Hz and
  sends scene snapshots over UDP. The client sends input and renders the
  snapshots; it never decides gameplay outcomes.
- **Two transports.** A TCP connection per player carries the handshake, the
  lobby (rooms, launch) and chat. UDP carries player input and snapshots.
- **One game at a time.** The server has a single four-player simulation, so
  only one room can be in game at a time.
- **Threads.** The client has a draw thread, a logic thread and a network
  thread that share state under one mutex (`mtex`). Take the lock before
  touching `SSceneData`, `SLobbyState` or the player actions.
- **Master server.** Game servers register with the master; clients ask it
  for a server list and then connect to the chosen server directly. The master
  is never in the gameplay path.

See [`docs/architecture.md`](docs/architecture.md) and
[`docs/server_system.md`](docs/server_system.md) for diagrams and details.

## 5. Working with the ECS

The ECS lives in `src/Shared/ECS/` and is header-only:

- `Ecs::SEntity`: generational handle. A stale handle never aliases a reused
  slot.
- `Ecs::CComponentPool<T>`: sparse set per component type.
- `Ecs::CRegistry`: owns entities and pools. Use `create`, `destroy`,
  `emplace`, `get`, `has`, `remove`, and iterate with `each<Ts...>()`,
  `view<Ts...>()` or `eachEntity()`.

Rules:

1. **Components hold state only.** Behaviour belongs in systems, which are
   plain classes taking a `CRegistry&` (for example `CMovementSystem`).
2. **Do not create or destroy entities, or add/remove components, while
   iterating.** Collect them first and apply the change afterwards.
3. **Networked components derive from `AComponent`**, implement `type()` and
   `serialize()`, and provide a constructor from `std::vector<std::uint8_t>`
   that calls `setInvalid()` on short, trailing or out-of-range data.
4. **Local-only components** (for example `SCChatInput`) do not derive from
   `AComponent` and are never serialized.

### Adding a networked component

1. Create `src/Shared/Components/<Name>.hpp` with an `SC<Name>` struct.
   Use `Components::append`, `appendString` and `CReader` from
   `Serialization.hpp`; validate everything you read.
2. Add a value at the **end** of `AComponent::eType` (values are the wire
   IDs, never reorder or reuse them).
3. Include the header and call `registerType<SC<Name>>()` in
   `CComponentRegistry`.
4. Add tests for the round trip, truncated data, trailing bytes and limits
   (see `tests/chat.cpp`).
5. Document the record layout in the relevant doc.

## 6. Networking changes

Keep the wire format and its documentation in step: **a protocol change is
not done until the docs and tests are updated.**

Conventions per layer:

| Layer | Framing | Integers |
| --- | --- | --- |
| UDP gameplay (`Shared/Types.hpp`) | `HAJ` + packet type, commands, `0D 0A` terminator | little-endian, explicit field serialization |
| TCP handshake | 7-byte hello, 5-byte welcome | little-endian port |
| TCP lobby / control (`Shared/LobbyProtocol.hpp`) | `HAJ` + command + `u16` length + payload | big-endian lengths and integers |

General rules:

- Serialize every field explicitly. Never send a raw C++ struct.
- Validate before reading: magic, command, exact or bounded length, string
  contents. Reject malformed or unknown data without applying it.
- TCP is a byte stream: buffer, parse a header only when enough bytes are
  available, and wait for the full payload. UDP datagrams are processed
  independently and a bad datagram is dropped, not fatal.
- Receivers must consume exactly the declared payload; undefined trailing
  bytes are malformed.
- A malformed or unknown TCP frame is a protocol error: close the connection.
- The server enforces capacity, room membership and master rights itself,
  whatever the client or master claims.

### Adding a lobby command

1. Add the value to `Net::eLobbyCommand` (lobby values are `30`-`43` today;
   take the next free one).
2. Add `make<Name>` / `read<Name>` helpers in `Shared/LobbyProtocol.hpp`.
   `make*` returns an empty vector for invalid input; `read*` returns `false`.
3. Handle it on the receiving side: `CServerNetworkThread::handleLobbyCommand`
   for client-to-server, `CNetworkThread::handleLobbyMessage` for
   server-to-client. Reply with `ROOM_REJECTED` when a request is not valid.
4. For a new client request, add an `eClientCommand` and handle it in
   `CNetworkThread::processLobbyCommands`.
5. Add round-trip and rejection tests in `tests/room_protocol.cpp`.
6. Update the command table and payload layout in
   [`docs/server_system.md`](docs/server_system.md#lobby-protocol) and the
   command list in [`docs/protocol.md`](docs/protocol.md).

The room chat is a complete example: `SCChatMessage`, `Chat::CChatSystem`,
the `CHAT_SEND` / `CHAT_MESSAGE` commands and `CChatUI`.

## 7. Levels

The five-level campaign uses `.toml` files in `assets/maps/`. The first level
is `init.toml`; levels two through five are `level_2.toml` to `level_5.toml`.
Each `[[enemies]]` table defines a spawn wave. Supported keys and the level
transition rules are documented in [`docs/level-layout.md`](docs/level-layout.md).

## 8. Code style

Formatting is enforced by `.clang-format` (LLVM-based, 4 spaces, 105 columns)
and CI reformats pull requests. Run `./format.sh` before you push to avoid
noise. `.clang-tidy` enforces these names:

| Kind | Convention | Example |
| --- | --- | --- |
| Class | `C` prefix, CamelCase (`I*` interfaces and `A*` abstract bases keep their own prefix) | `CRegistry`, `IRenderSystem`, `AComponent` |
| Struct | `S` prefix | `SSceneData`; components are `SC*` (`SCPosition`) |
| Enum | `e` prefix, `UPPER_CASE` values | `eEntityKind::PLAYER` |
| Function | camelBack | `makeSnapshot` |
| Namespace | CamelCase | `Net`, `Game`, `Room`, `Lobby` |

The surrounding code also uses an `m` prefix for thread class members
(`mSceneData`) and a trailing underscore in some helpers (`renderer_`). Follow
the style of the file you are editing.

Other expectations:

- Prefer the existing dependencies (raylib, Asio, tomlplusplus). Add a new one
  only when justified.
- Rendering goes through `IRenderSystem` so that raylib can be replaced.
  Convert between raylib `Color` and `SColor` explicitly; do not
  `reinterpret_cast`.
- Do not add comments that restate the code. Comment intent, constraints and
  trade-offs.

## 9. Tests

Add or update a test with every behaviour change:

- Protocol and serialization: round trip, bad magic, truncated and oversized
  data.
- Simulation and ECS: `tests/game_simulation.cpp`, `tests/ecs.cpp`.
- Rooms and lobby: `tests/room_protocol.cpp`.

Rendering and raylib input are not unit-tested, so keep logic in systems that
work on a registry and can be tested without a window.

## 10. Pull requests

1. Branch from `dev` and open the pull request against `dev` (or `main` for
   releases). The workflow runs for both.
2. Make sure `./build.sh` succeeds and the tests pass locally. Run
   `./format.sh` and `./lint.sh` if you have LLVM 21.
3. CI (`.github/workflows/pullrequests.yaml`) builds the project, runs
   `clang-tidy`, then runs `clang-format` and pushes a
   `ci: apply clang-format` commit to the branch when files change. Pull
   changes before pushing again.
4. Pull requests that only change `docs/**`, `.github/**` or `README.md`
   skip the build checks, so double-check those changes yourself.
5. In the description, explain what changed and why, call out wire-format or
   behaviour changes, and say how you tested them.

## 11. Known limitations

Be aware of these when designing changes (all from the docs):

- One room can be in game at a time; per-room simulations are future work.
- There is no sequence number, acknowledgement or encryption on UDP, and no
  authentication of player or server names.
- Client-side prediction and correction packets are planned, not implemented.
- A server list is a snapshot, not a reservation: handle rejection and
  go back to discovery.
- `docs/server_system.md` states it supersedes `docs/protocol.md`, but
  `protocol.md` remains authoritative for the implemented gameplay packets.
