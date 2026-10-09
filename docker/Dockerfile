FROM debian:13

# Build environment for Linux tests/packages, AArch64 cross-builds, and MinGW Windows packages.
# Keep noninteractive mode local to installation, not subsequent CI commands.

# Common build tools.
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        ca-certificates git curl \
        build-essential cmake ninja-build file perl python3 pkg-config \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Native cryptography and printing.
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        libssl-dev libzstd-dev openssl libcups2-dev \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Native Qt desktop dependencies.
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        libfontconfig1-dev libfreetype-dev libgl1-mesa-dev libx11-dev \
        libx11-xcb-dev libxcb1-dev libxcb-cursor-dev libxcb-glx0-dev \
        libxcb-icccm4-dev libxcb-image0-dev libxcb-keysyms1-dev \
        libxcb-randr0-dev libxcb-render-util0-dev libxcb-shape0-dev \
        libxcb-shm0-dev libxcb-sync-dev libxcb-util-dev libxcb-xfixes0-dev \
        libxcb-xkb-dev libxext-dev libxfixes-dev libxi-dev libxkbcommon-dev \
        libxkbcommon-x11-dev libxrender-dev \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Linux packaging tools.
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        dpkg-dev rpm \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# Windows cross-compilation and installer tools.
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        mingw-w64 mingw-w64-tools nsis \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# ARM64 cross-toolchain and multiarch support.
RUN dpkg --add-architecture arm64 \
    && apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        gcc-aarch64-linux-gnu g++-aarch64-linux-gnu binutils-aarch64-linux-gnu \
        libc6-dev:arm64 libstdc++-14-dev:arm64 \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# ARM64 cryptography and printing.
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        libssl-dev:arm64 libzstd-dev:arm64 libcups2-dev:arm64 \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# ARM64 Qt desktop dependencies.
RUN apt-get update \
    && DEBIAN_FRONTEND=noninteractive apt-get install -y --no-install-recommends \
        libfontconfig1-dev:arm64 libfreetype-dev:arm64 \
        libgl-dev:arm64 libegl-dev:arm64 libgles-dev:arm64 \
        libx11-dev:arm64 libx11-xcb-dev:arm64 \
        libxcb1-dev:arm64 libxcb-cursor-dev:arm64 libxcb-glx0-dev:arm64 \
        libxcb-icccm4-dev:arm64 libxcb-image0-dev:arm64 libxcb-keysyms1-dev:arm64 \
        libxcb-randr0-dev:arm64 libxcb-render-util0-dev:arm64 libxcb-shape0-dev:arm64 \
        libxcb-shm0-dev:arm64 libxcb-sync-dev:arm64 libxcb-util-dev:arm64 libxcb-xfixes0-dev:arm64 \
        libxcb-xkb-dev:arm64 libxext-dev:arm64 libxfixes-dev:arm64 libxi-dev:arm64 \
        libxkbcommon-dev:arm64 libxkbcommon-x11-dev:arm64 libxrender-dev:arm64 \
    && apt-get clean \
    && rm -rf /var/lib/apt/lists/*

# CI supplies the checkout and build commands; no source or artifacts are baked in.
# Retain the default root user for Docker runners and mounted checkout access.
WORKDIR /workspace
CMD ["/bin/bash"]
