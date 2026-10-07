# jPDF Desk CI and cross-build setup

[Back to the overview](../README.md) · [Build and packaging guide](guide.md#build)

Paths and commands are relative to the repository root. The build guide owns
application build requirements and packaging instructions; this document covers
the CI environment, artifact collection, and ARM64 sysroot setup.

## Build container

[`docker/Dockerfile`](../docker/Dockerfile) defines the Debian 13 Linux x86_64
build environment. It includes Linux desktop/OpenSSL/CUPS development libraries,
DEB/RPM tools, MinGW/NSIS, and the AArch64 cross-toolchain. It contains no source,
Qt SDK, application binaries, or AppImage tooling. QtBase and MuPDF are built
from the checkout's pinned submodules.

```sh
docker build --pull -f docker/Dockerfile -t jpdf-desk-ci:debian13 docker
# Initialize source submodules as described in the build guide first.
docker run --rm -v "$PWD:/workspace" jpdf-desk-ci:debian13 \
    bash -lc 'cmake --preset debug --fresh && cmake --build --preset debug --parallel 2 && ctest --preset debug --no-tests=error'
```

The container runs as root, so mounted build outputs are root-owned. Use
`--user "$(id -u):$(id -g)"` locally if needed (with writable build directories).

## Publish the CI image

Commit the Dockerfile and [image workflow](../.github/workflows/ci-image.yml).
**Publish CI image** runs automatically on pushes to `release` that change the
Dockerfile or the image workflow. It also supports **Actions → Publish CI image
→ Run workflow**. It uses `GITHUB_TOKEN` with `packages: write`; no personal token
secret is required.

The workflow publishes Linux x86_64 images to:

```text
ghcr.io/OWNER/REPOSITORY/ci-build:debian13
ghcr.io/OWNER/REPOSITORY/ci-build:sha-FULL_COMMIT_SHA
```

Owner and repository names are lowercased. The `debian13` tag is mutable; use an
image digest for reproducible CI. Rerun publishing periodically for base-image
security updates.

**Bootstrap:** publish the image before running builds. New GHCR packages may be
private even for a public repository. Make the package public in **Packages →
Package settings** for unauthenticated/fork CI access. Private packages require
Actions access and registry authentication before pulling the job container.
Image publishing and builds run independently: after changing the Dockerfile,
wait for publishing to finish, then rerun builds that used the old image.

For manual publishing, authenticate with a personal access token (classic) with
`write:packages`, authorized for organization SSO if applicable. Never commit it:

```sh
export IMAGE=ghcr.io/owner/repository/ci-build  # replace with lowercase names
read -rsp 'GitHub packages token: ' GHCR_TOKEN; echo
printf '%s' "$GHCR_TOKEN" | docker login ghcr.io -u OWNER --password-stdin
unset GHCR_TOKEN
docker buildx build --pull --platform linux/amd64 \
    -f docker/Dockerfile -t "$IMAGE:debian13" --push docker
docker logout ghcr.io
```

## Automated builds and artifacts

The [build workflow](../.github/workflows/build.yml) runs on pushes, pull requests,
and manual requests. It covers all 11 non-hidden configure presets:

- `debug`: Linux build and tests.
- `release`, `linux-executable`: Linux executables.
- `linux-deb`, `linux-rpm`, `linux-appimage`: Linux packages.
- `linux-aarch64`: ARM64 cross-build with a provisioned sysroot.
- `release-win`, `windows-executable`, `windows-installer`: Windows cross-builds.
- `macos-arm64`: native Apple Silicon build on `macos-14`.

The `default` build preset aliases `release`, so it is not built separately.
[`script/ci-build.sh`](../script/ci-build.sh) initializes required submodules,
builds the selected preset, runs debug tests, and collects files in
`dist/<preset>/` with `SHA256SUMS`. Cross-builds first build native Qt host tools.
macOS artifacts contain the installed app bundle in a `.tar.gz`. Each preset
uploads an artifact retained for 14 days; debug also uploads test diagnostics.
The workflow does **not** publish GitHub Releases automatically.

Windows CI builds disable OpenSSL; signature images still work, but digital
signing/verification does not. macOS uses Homebrew OpenSSL; bundles are unsigned
and not notarized. Linux AppImage tooling is downloaded from mutable `continuous`
releases. Pin image digests and tool versions/checksums for reproducible releases.
Cross-builds do not test runtime behavior on target systems. Check workflow run
results and test artifacts on the target OS before distribution; configured
coverage is not evidence that builds have passed.

The existing [GitLab pipeline](../.gitlab-ci.yml) uses `debian:13` and installs
its dependencies in jobs. To reuse GHCR, update `default.image` and remove the
redundant dependency-install steps while retaining Git setup and selective
submodule initialization. Its release-publication job uses a separate image.

## Linux AArch64 cross-builds

The CI image includes `gcc-aarch64-linux-gnu`, `g++-aarch64-linux-gnu`, and
`binutils-aarch64-linux-gnu`. A compiler alone is insufficient:
[`cmake/aarch64-toolchain.cmake`](../cmake/aarch64-toolchain.cmake) expects target
headers and libraries in `/opt/sysroot`, including libc, OpenSSL, CUPS, OpenGL,
fonts, and X11/XCB. The Dockerfile does not populate this sysroot.

Inside the Debian 13 CI container, run:

```sh
bash script/setup-aarch64-sysroot.sh
cmake --preset release
cmake --build --preset release --target jpdf-desk --parallel 2
# Apply these only to the cross-build, not to the native host-tools build.
export PKG_CONFIG_SYSROOT_DIR=/opt/sysroot
export PKG_CONFIG_LIBDIR=/opt/sysroot/usr/lib/aarch64-linux-gnu/pkgconfig:/opt/sysroot/usr/share/pkgconfig
unset PKG_CONFIG_PATH
cmake --preset linux-aarch64 --fresh
cmake --build --preset linux-aarch64 --parallel 2
```

Run the sysroot script as root (or with `sudo` outside the CI image). It downloads
and extracts Debian 13 ARM64 packages using isolated APT metadata; it does not
install them into the host package database or require ARM64 emulation. Existing
sysroot files can be overwritten. Use `--help` for prerequisites. Alternatively,
mount a prepared sysroot at `/opt/sysroot` when starting the container.

The toolchain directs compiler and CMake library/header lookup into the sysroot;
it does not configure pkg-config isolation automatically. The environment
variables above prevent pkg-config from selecting host x86_64 dependencies.
The current CI workflow/helper does not set these variables for ARM64, so its
dependency resolution needs validation; use the explicit manual recipe above
when diagnosing cross-build failures. Native `release` supplies Qt host tools;
the result is `.build/linux-aarch64/gui/jpdf-desk`.

Review the [licensing notes](../README.md#licensing) before distributing artifacts.
