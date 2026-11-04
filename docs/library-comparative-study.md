# Comparative Study and Initial Technology Selection

## Purpose and context

This study describes the library choices made when starting R-Type, a C++23
multiplayer shoot 'em up with a Raylib client, an authoritative server, a
master server, a shared ECS simulation, TCP lobby traffic, and UDP gameplay
snapshots.

The selection criteria were:

- **Architectural fit:** support for a split client/server design and a shared
   simulation.
- **Control:** ability to keep the game loop, wire protocol, and ownership
   rules explicit.
- **Performance:** low overhead for a small 2D game and a 60 Hz simulation.
- **Portability:** Linux, Windows, and possible macOS builds.
- **Learning and implementation cost:** useful features without unnecessary
   framework complexity.
- **Maintenance:** documentation, community, CMake integration, and long-term
   availability.

This is an architectural comparison, not a benchmark. The chosen libraries
were selected for the project's expected scope and constraints, not because
they are universally superior.

## Programming-language selection

### Requirements at project start

The language needed to support both the client and the authoritative server,
with a shared ECS, shared component definitions, and shared binary protocol
serialization. It also needed predictable performance for a 60 Hz simulation,
low-level TCP and UDP access, portable desktop builds, and access to mature 2D
game and testing libraries.

The project also benefited from explicit ownership and data-layout control. The
server must remain authoritative, packet data must be validated explicitly, and
the client and server should share domain types without duplicating the game
model in separate languages.

| Language | Strengths for R-Type | Limitations for the initial project |
| --- | --- | --- |
| **C++** | Mature game-development ecosystem; direct memory and data-layout control; strong TCP/UDP and graphics-library availability; supports a shared native ECS and protocol code; excellent desktop portability | More complex language rules; manual lifetime and concurrency errors are possible; build times and dependency management require discipline |
| **Rust** | Memory and thread safety by default; strong performance; expressive enums and pattern matching are useful for protocol handling; good tooling | Smaller and less uniform game-library ecosystem; ownership and borrowing would increase the learning cost for a shared ECS and threaded client/server design; Raylib and Asio-equivalent integration is less direct |
| **C#** | Productive language; strong tooling; mature networking and testing libraries; good options through Unity, MonoGame, or Stride | Runtime and framework choices add deployment considerations; less direct control over data layout and allocation; sharing the exact same low-level simulation and wire structures with a native client/server is less natural |
| **Go** | Simple concurrency model; fast builds; practical standard networking package; easy server deployment | Weaker fit for a real-time graphical client; garbage collection and limited low-level control are less attractive for the shared simulation and rendering side |
| **Java** | Portable runtime; mature networking, concurrency, and testing ecosystems; strong tooling | Runtime and allocation behavior are less predictable for a small real-time game; graphics and native-library integration add extra choices; less natural fit for the selected native game libraries |
| **Python** | Very fast prototyping and broad tooling | Runtime performance, allocation behavior, deployment, and native graphics integration make it unsuitable as the primary language for this client/server simulation |

### Selected option: C++23

C++23 was selected because it offered the best combination of performance,
native library compatibility, portability, and control for the initial design.
It allowed the client and server to share the ECS, components, protocol types,
serialization code, and simulation without introducing a second language or a
cross-language data contract.

The language choice also aligned directly with the selected libraries: Raylib
provides a straightforward C/C++ client API, standalone Asio provides portable
TCP and UDP primitives, toml++ exposes a modern C++ interface, and GoogleTest
integrates naturally with the CMake build. C++23 features such as ranges,
strongly typed enums, `std::span`, and standard concurrency utilities improve
expressiveness while retaining control over allocations and data layout.

Rust was the strongest alternative for safety-critical ownership and
concurrency guarantees. It was not selected because the initial project placed
more weight on the established C++ game ecosystem, direct library integration,
and the team's ability to share native code quickly. C# would have been a
strong alternative for a framework-led game client, while Go would have been
more attractive if the project were primarily a network service rather than a
shared real-time game.

## Selected stack

| Concern | Selected technology | Version or approach | Why it was selected |
| --- | --- | --- | --- |
| Client graphics and input | Raylib | 6.0 | A small 2D API with windowing, textures, drawing, and input in one package |
| Networking | Standalone Asio | 1.38.2 | Portable TCP and UDP primitives without the full Boost dependency |
| Configuration | toml++ | 3.4.0 | Typed, readable TOML files for hand-authored levels |
| Testing | GoogleTest | 1.17.0 | Mature assertions, fixtures, and direct CTest discovery |
| Entity-component system | Project-owned ECS | Header-only C++23 code | A small domain-specific API with full control over storage and entity semantics |
| Dependency acquisition | CPM.cmake | CMake integration | Simple source-based setup with pinned dependency revisions and a cache |

The stack was chosen as a coherent whole: lightweight client technology, direct
transport control, readable game data, conventional testing, and no external
ECS dependency before the simulation's needs were known.


## 1. Client graphics and input

The client uses Raylib through `CRenderSystem` and UI classes. The shared
simulation deliberately depends on render-neutral components; Raylib is kept
at the client rendering boundary.

### Requirements at project start

The client needed a cross-platform 2D window, keyboard input, texture loading,
sprite drawing, and simple UI primitives. It did not initially require a full
3D engine, an editor, advanced audio, or a platform-services framework.

| Library | Strengths | Costs and risks | Fit for R-Type |
| --- | --- | --- | --- |
| **Raylib** | Small API, quick setup, built-in 2D drawing, textures, input, and window management; low ceremony for a game prototype or small game | Less extensive than SDL for platform services, device edge cases, and ecosystem integrations; application architecture remains the developer's responsibility | **Best initial fit.** It directly supports the current 2D client and the `IRenderSystem` boundary keeps the renderer isolated |
| **SDL3** | Mature platform abstraction, broad input/audio/controller support, strong ecosystem, and more low-level control | More setup code; rendering usually requires SDL GPU, OpenGL, Vulkan, or another layer; would touch window, texture, input, and draw code | Strong option for a larger cross-platform client, but not a clear improvement for the present scope |
| **SFML 3** | C++-oriented API with windowing, input, graphics, and audio in one package; approachable for 2D games | Smaller ecosystem than SDL; would affect most client-facing code; less C-style interoperability | Reasonable alternative for a C++-first client, but Raylib better matched the initial scope |
| **GLFW + OpenGL** | Very small window/input layer and maximum rendering control | Requires choosing and maintaining an image, rendering, and asset stack; substantially more code than the project needs | Poor fit unless rendering control is itself a requirement |

### Selected option: Raylib

Raylib was selected because it covered the complete initial client requirement
with minimal ceremony. It allowed the team to focus on game behavior and
networking instead of building a rendering framework. The shared simulation
remains free of Raylib types, and the rendering system boundary preserves
architectural separation if the graphics layer later grows more demanding.

SDL3 would have been the better initial choice for a broader application with
advanced controller, audio, display, or backend requirements. Those were not
part of the initial scope, so its additional setup was not justified.

## 2. Networking and asynchronous I/O

Asio is used for TCP handshakes and lobby frames, UDP gameplay packets, socket
endpoints, and networking error codes. The current server loop is deliberately
explicit and uses non-blocking sockets with a fixed-rate loop.

### Requirements at project start

The game needed reliable TCP for handshake and lobby messages, connection
management, UDP for low-latency input and snapshots, portable endpoints, and
explicit control over packet validation. The project also needed to define its
own binary protocol rather than adopt a framework-specific message format.

| Library | Strengths | Costs and risks | Fit for R-Type |
| --- | --- | --- | --- |
| **Standalone Asio** | Header-only, Boost-independent, portable TCP/UDP API, C++ integration, and direct control over synchronous or asynchronous operation | The project uses a manually polled loop, so it does not receive the full benefit of Asio's async model; header-heavy builds can affect compile time | **Best initial fit.** It supplies exactly the transport primitives needed without pulling in Boost |
| **Boost.Asio** | Same core Asio model plus the Boost ecosystem, established packaging, SSL and related integrations | Larger dependency and build surface; Boost version and configuration become part of the project's operational cost | Good if SSL, Boost.Beast, Boost.Serialization, or other Boost libraries become requirements |
| **libuv** | Mature event loop, cross-platform handles, and strong C-oriented integration | C API and callback model are less natural for this C++ code; networking abstractions are less directly aligned with the existing Asio types | Viable for a service-oriented design, but less natural for this C++ project |
| **ENet** | Game-oriented reliable UDP, channels, sequencing, and retransmission support | It changes the transport model and protocol responsibilities; less suitable when the project intentionally defines its own TCP/lobby and UDP wire protocol | Useful only if reliable-UDP features become more important than protocol control |
| **GameNetworkingSockets** | Production-oriented game networking features, reliable messages, connection management, and NAT-related capabilities | Larger conceptual and operational footprint; the current protocol and server model would need redesign | Overpowered for the current LAN or directly addressed server model |

### Selected option: standalone Asio

Standalone Asio was selected because it provided the required TCP and UDP
primitives while keeping the dependency surface small. It did not dictate the
wire format, packet sizes, server authority model, or threading boundaries.
That was important for documenting and testing the R-Type protocol explicitly.

Boost.Asio would have been appropriate if the project had already depended on
Boost or required Beast, SSL, or other Boost components. ENet and
GameNetworkingSockets would have been reasonable for a game built around
reliable UDP, but the initial design intentionally separated reliable TCP
control traffic from UDP gameplay traffic.

## 3. Configuration parsing

toml++ parses level data such as enemy spawn definitions. The code uses typed
TOML nodes and `value_or`, which keeps the configuration close to the domain
model.

### Requirements at project start

Level authors needed a readable format with comments, arrays, nested values,
and straightforward numeric and string access. The format had to be more
pleasant to maintain by hand than JSON while remaining simple to validate and
parse in C++.

| Library | Strengths | Costs and risks | Fit for R-Type |
| --- | --- | --- | --- |
| **toml++** | Modern C++ API, TOML 1.0 support, header-only distribution, good typed access, and strong diagnostics | Header-only compile cost; configuration validation still needs to be defined by the application | **Best initial fit.** Its typed table and array API match the level model |
| **toml11** | C++-friendly TOML library with a convenient API and good parsing support | Similar capabilities with different APIs and diagnostics | Viable alternative, but toml++ better matched the chosen style |
| **cpptoml** | Historically simple C++ TOML API | Less attractive for a new C++23 project than the actively maintained modern options | Weaker initial choice |
| **yaml-cpp** | Flexible hierarchical format, broad familiarity, and YAML's expressive data model | YAML's complexity and implicit typing can make game configuration less predictable; introduces a more complex format | Consider only if configuration needs YAML-specific features |
| **nlohmann/json** | Very familiar JSON API and broad ecosystem | JSON is more verbose for hand-authored levels and does not provide TOML's comments and authoring style | Useful for interchange or tooling, but less suitable for level files |

### Selected option: toml++

toml++ was selected because the level format is authored by people, not only
generated by tools. TOML makes enemy spawn definitions readable while its C++
API maps naturally to typed domain values. It was sufficient without introducing
YAML's ambiguity or JSON's verbosity.

The selection also left room for a later schema-validation layer. The parser
handles syntax and types; the game code remains responsible for validating
domain rules such as required enemy fields and valid spawn modes.

## 4. Test libraries and strategy

The project uses GoogleTest for protocol, ECS, controls, room, chat,
simulation, and sprite-atlas tests. GoogleTest defines the assertions and test
cases; CTest is the build-system test runner that discovers and executes the
compiled tests through CMake's `gtest_discover_tests` integration.

### Requirements at project start

The testing stack needed to cover several different kinds of correctness:

- **Unit tests** for ECS handles, component pools, player actions, and utility
   types.
- **Protocol tests** for binary encoding, malformed packets, size limits, and
   round trips.
- **Simulation tests** for deterministic movement, collisions, spawning, and
   snapshots.
- **Stateful feature tests** for rooms, chat, and connection-related behavior.
- **Cross-platform execution** through CMake and CTest on supported platforms.

The framework also needed readable failure messages, fixtures or helpers for
shared setup, parameterized tests for protocol variations, and a conventional
workflow that could run locally and in CI.

| Framework | Strengths | Costs and risks | Fit for R-Type |
| --- | --- | --- | --- |
| **GoogleTest** | Mature assertions, fixtures, parameterized tests, broad documentation, and direct CTest integration | More macros and framework machinery than very small header-only frameworks | **Best initial fit.** It supports the current assertions and future test growth |
| **Catch2** | Expressive test cases, readable sections, strong standalone experience, and generators | More framework conventions and features than the initial focused suite required | Good alternative for a new test suite |
| **doctest** | Very small, fast to compile, header-only, and familiar assertion style | Smaller ecosystem and fewer heavyweight test features | Attractive if compile time becomes a measured problem |
| **Boost.Test** | Mature framework with rich features and Boost integration | Adds Boost as a dependency when the project currently avoids it | Only sensible if Boost is already adopted for other reasons |

### Selected option: GoogleTest

GoogleTest was selected because it is a conventional, well-documented choice
for C++ projects and supports both small assertion-based tests and future
fixtures or parameterized cases. Its CMake and CTest integration made the
tests easy to run in local builds and CI without adding custom test tooling.

doctest would have been a strong choice if minimizing compile time were the
primary constraint. GoogleTest offered a better balance of familiarity,
documentation, and long-term test-suite flexibility for this project.

### Test runner and mocking considerations

CTest was retained as the execution layer because it is already part of CMake,
works across the supported platforms, and integrates with `gtest_discover_tests`
without requiring a second test orchestration system. This separates test
definition from test execution: GoogleTest provides the test semantics while
CMake and CTest handle discovery, filtering, and reporting.

A dedicated mocking library was not selected initially. Most important behavior
is deterministic domain logic that can be tested with real registries, protocol
buffers, and simulation state. Introducing mocks for the ECS or network layer
would have added abstractions before the ownership boundaries were stable.
Mocks can still be justified later for external services such as master-server
discovery or failure-injection tests around network disconnects.

## 5. ECS strategy

The ECS is project-owned and header-only. It uses generational entity handles,
sparse component pools, packed storage with swap removal, and lazy views. Both
the authoritative server simulation and the client render world use it.

### Requirements at project start

The server simulation and client render world needed shared entities and
components, predictable iteration, safe entity reuse, and no dependency on
rendering or networking libraries. The initial game had a small number of
component types and no need for reflection, scripting, editor tooling, or
complex entity relationships.

| Option | Strengths | Costs and risks | Fit for R-Type |
| --- | --- | --- | --- |
| **Current custom ECS** | Small API tailored to the game, no external dependency, complete control over serialization boundaries and structural rules | The team owns correctness, documentation, profiling, and future features; fewer ready-made tools | **Best initial fit.** The behavior is well covered by focused tests and matches the project's needs |
| **EnTT** | Mature C++ ECS, views and groups, signals, resource management, and a large user base | More API surface and conventions than the initial simulation required | Strong general-purpose choice for a broader initial feature set |
| **Flecs** | Feature-rich ECS with queries, relationships, modules, reflection, and optional scripting-oriented capabilities | Larger conceptual model and API; may introduce features and runtime behavior the project does not need | Best for a more data-driven game with complex relationships and tooling needs |
| **entityx** | Lightweight C++ ECS with established concepts | Less compelling for a modern C++23 project than EnTT or Flecs | Weaker initial choice than the custom ECS or EnTT |

### Selected option: project-owned ECS

A custom ECS was selected because the initial requirements were narrow and
well-defined. Its generational handles prevent stale entity aliases, packed
component pools support efficient iteration, and lazy views provide the exact
system queries needed by the simulation. Keeping it in the repository also
made the client/server shared model explicit and avoided coupling gameplay to
an external framework prematurely.

EnTT would have been the strongest general-purpose selection if the project
started with a larger or less certain ECS feature set. Flecs would have been
appropriate for a data-driven game requiring relationships, reflection, or
editor integration. Neither advantage was necessary at project start.

## 6. Dependency acquisition and build integration

CPM.cmake downloads pinned source revisions during CMake configuration and
places them in a cache. This keeps the repository's setup instructions simple
and avoids requiring users to preinstall the four libraries.

### Requirements at project start

The project needed a reproducible CMake configuration that could build on
Linux and Windows without asking developers to install four libraries by hand.
Dependencies needed pinned versions, source caching, and straightforward
integration with CMake targets.

| Tool | Strengths | Costs and risks | Fit for R-Type |
| --- | --- | --- | --- |
| **CPM.cmake** | Minimal setup, CMake-native, convenient for small source-based projects, and compatible with pinned Git tags | Configuration can require network access; source builds increase configure/build time; supply-chain policy and offline builds need explicit handling | **Best initial fit.** It kept the small dependency set self-contained and pinned |
| **FetchContent** | Built into CMake and widely understood | Similar source-fetch and reproducibility concerns; less convenient version/dependency management than CPM's wrapper | Reasonable if minimizing third-party CMake code is a priority |
| **vcpkg** | Curated ports, manifests, binary caching, and strong Windows experience | Toolchain integration and triplet configuration add project/environment complexity | Strong candidate for reproducible team and CI builds |
| **Conan** | Profiles, binary packages, versioning, and cross-platform dependency graphs | Adds a separate package-manager workflow and generated CMake integration | Strong candidate for multi-configuration or larger dependency graphs |

### Selected option: CPM.cmake

CPM.cmake was selected because the dependency graph was small and source
integration kept the project setup self-contained. Developers could configure
the project with CMake and Git, while pinned tags and `CPM_SOURCE_CACHE` kept
repeated builds practical.

vcpkg or Conan would have been stronger initial choices for a large team with
many binary dependencies, strict offline builds, or extensive CI caching. For
this project, their additional environment configuration outweighed those
benefits.

## Final selection rationale

The initial choices were driven by one consistent principle: use the smallest
technology that gives the project direct control over its core behavior.

- **Raylib** kept the first client focused on gameplay rather than rendering
  infrastructure.
- **Standalone Asio** supplied portable sockets without imposing a transport
  protocol or a large framework.
- **toml++** made hand-authored levels readable and typed.
- **GoogleTest** provided conventional, discoverable tests for shared rules.
- **The custom ECS** matched the known simulation requirements without adding
  an external abstraction prematurely.
- **CPM.cmake** made the small dependency set easy to configure and pin.

Together, these choices minimized initial complexity while leaving clear
boundaries for future growth: rendering is isolated from shared simulation,
transport is isolated behind protocol code, configuration is isolated in the
game systems, and ECS behavior is covered by its own tests.

## Selection summary

| Area | Chosen | Primary reason | Best alternative at project start |
| --- | --- | --- | --- |
| Graphics and input | Raylib | Complete small 2D client API with low setup cost | SDL3 for broader platform requirements |
| Networking | Standalone Asio | Direct portable TCP/UDP control without Boost | Boost.Asio if the Boost ecosystem was already needed |
| Configuration | toml++ | Readable hand-authored data with typed C++ access | toml11 for a similar TOML-based design |
| Testing | GoogleTest | Mature C++ testing and CTest integration | doctest if compile-time minimalism dominated |
| ECS | Custom ECS | Exact fit, no dependency, full storage control | EnTT for a broader ready-made ECS |
| Build dependencies | CPM.cmake | Simple CMake setup with pinned source revisions | vcpkg for binary caching and team-scale reproducibility |

## Conclusion

For a new R-Type project with this scope, the selected stack was a defensible
balance of simplicity, control, portability, and maintainability. The choices
were not made because alternatives were unusable; they were made because each
selected library solved the immediate problem without adding capabilities the
initial architecture did not need.