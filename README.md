# R-Type
R-Type is a C++ multiplayer game with a Raylib client, an Asio server, and a
shared entity-component-system simulation.

## Environment
This project uses Nix devenv to manage dependencies and build the project. To set up the environment, you need to have [Nix](https://nixos.org/download/) and [devenv](https://devenv.sh/) installed on your system. Once installed, you can enter the development environment by running:

```bash
devenv shell
```

## Building the project
Since we are using Nix/devenv, building the project is done on a UNIX-like system or on Windows via WSL2.
Building the project is done by running the following command in the root directory of the project:

```bash
build-linux # to build the Linux binary
build-windows # to build the Windows binary via cross-compilation
```

You can clean the build artifacts by running:

```bash
clean-all
```

## Running

Start the server, then provide the server's IPv4 address when launching a
client:

```bash
./build/R-Type_server
./build/R-Type_client 127.0.0.1
```

The server logs its TCP handshake port, UDP gameplay port, accepted clients,
rejected connections, and malformed or unauthorized input packets.

The client reads keyboard input from the render thread, sends the latest input
state over UDP, and renders the authoritative snapshots received from the
server.

## Tests and linting

Run the test suite with:

```bash
./coverage.sh
```

Run the repository lint checks with:

```bash
./lint.sh
```

## Technical stack

### Around the project

- markdown
- github actions
- github project
- cmake
- nix
- devenv

### Project stack

- c++
- raylib
- asio
- google test
