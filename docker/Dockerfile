FROM debian:13

# Build environment for Linux tests/packages, AArch64 cross-builds, and MinGW Windows packages.
# Keep noninteractive mode local to installation, not subsequent CI commands.
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        ca-certificates git curl \
        build-essential cmake ninja-build file perl python3 pkg-config \
        libssl-dev openssl libcups2-dev \
        libfontconfig1-dev libfreetype-dev libgl1-mesa-dev libx11-dev \
        libx11-xcb-dev libxcb1-dev libxcb-cursor-dev libxcb-glx0-dev \
        libxcb-icccm4-dev libxcb-image0-dev libxcb-keysyms1-dev \
        libxcb-randr0-dev libxcb-render-util0-dev libxcb-shape0-dev \
        libxcb-shm0-dev libxcb-sync-dev libxcb-util-dev libxcb-xfixes0-dev \
        libxcb-xkb-dev libxext-dev libxfixes-dev libxi-dev libxkbcommon-dev \
        libxkbcommon-x11-dev libxrender-dev \
        dpkg-dev rpm mingw-w64 mingw-w64-tools nsis \
        gcc-aarch64-linux-gnu g++-aarch64-linux-gnu binutils-aarch64-linux-gnu \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# CI supplies the checkout and build commands; no source or artifacts are baked in.
# Retain the default root user for Docker runners and mounted checkout access.
WORKDIR /workspace
CMD ["/bin/bash"]
