# CI setup

[Documentation index](../README.md) · [Build instructions](build.md)

## Build container

Initialize submodules as described in the build instructions, then run:

```sh
docker build --pull -f docker/Dockerfile -t jpdf-desk-ci:debian13 docker
docker run --rm -v "$PWD:/workspace" jpdf-desk-ci:debian13 \
    bash -lc 'cmake --preset debug --fresh && cmake --build --preset debug --parallel 2 && ctest --preset debug --no-tests=error'
```

Mounted outputs are root-owned by default.

## Publish the CI image

Run **Actions → Publish CI image → Run workflow** before starting builds.
The [workflow](../../.github/workflows/ci-image.yml) also runs when its files or
the Dockerfile change on `release`. Make the GHCR package public for fork access.
After Dockerfile changes, wait for publication and rerun builds.

## Automated builds and artifacts

The [GitHub workflow](../../.github/workflows/build.yml) builds Linux, Windows,
Apple Silicon macOS, and Snap packages. Native Linux/macOS tests run; Windows
and ARM64 tests are only compiled. Windows builds omit digital signing;
macOS bundles are unsigned and not notarized.

[`script/ci-build.sh`](../../script/ci-build.sh) collects preset artifacts and
checksums in `dist/<preset>/`. Actions artifacts are retained for 14 days.
The [GitLab pipeline](../../.gitlab-ci.yml) also builds and packages the app;
its optional Snap job requires a Snapcraft/LXD shell runner.

Update versions in `CMakeLists.txt` and `snap/snapcraft.yaml`, then push a version
tag (for example, `v1.1.0`) to publish a GitHub Release after all builds succeed.
Tags with a prerelease suffix (for example, `v1.1.0-rc1`) publish prereleases.
Snap Store publication is separate. Check workflow results and test binaries on
the target OS before distribution.

## Linux AArch64 cross-builds

Inside the Debian CI container, prepare the sysroot as root and build native Qt
host tools before configuring ARM64:

```sh
bash script/setup-aarch64-sysroot.sh
cmake --preset release-linux
cmake --build --preset release-linux --target jpdf-desk --parallel 2
env -u PKG_CONFIG_PATH \
    PKG_CONFIG_SYSROOT_DIR=/opt/sysroot \
    PKG_CONFIG_LIBDIR=/opt/sysroot/usr/lib/aarch64-linux-gnu/pkgconfig:/opt/sysroot/usr/share/pkgconfig \
    cmake --preset release-linux-arm64 --fresh
cmake --build --preset release-linux-arm64 --parallel 2
```

The sysroot script extracts target packages into `/opt/sysroot` and may overwrite
existing files. Keep pkg-config isolated from host libraries as above; the CI
helper does not currently set this isolation itself.
