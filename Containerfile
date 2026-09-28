FROM docker.io/nixos/nix:latest

# devenv requires flakes + the new nix CLI (set for runtime use too)
RUN echo "experimental-features = nix-command flakes" >> /etc/nix/nix.conf && \
    echo "accept-flake-config = true" >> /etc/nix/nix.conf

RUN nix profile add \
      --extra-experimental-features "nix-command flakes" \
      nixpkgs#devenv

ENV PATH="/root/.nix-profile/bin:${PATH}"

WORKDIR /workspace
CMD ["bash"]
