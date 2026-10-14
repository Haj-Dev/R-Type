FROM docker.io/epitechcontent/epitest-docker:latest

# Install clang-format and clang-tidy
RUN apt-get update
RUN apt-get install -y clang-format clang-tidy

# Install Ninja build system
RUN apt-get install -y ninja-build

# Install OpenGL development libraries
RUN apt-get install -y libopengl-dev libglx-dev libglu1-mesa-dev

# Install zip
RUN apt-get install -y zip unzip
