FROM docker.io/epitechcontent/epitest-docker:latest

# Install clang-format and clang-tidy
RUN apt-get update
RUN apt-get install -y clang-format clang-tidy
