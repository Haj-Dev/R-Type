# R-Type
Right now it's nothing more than a C++ file running a Raylib demo.

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
