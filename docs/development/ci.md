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
and ARM64 tests are only compiled. Windows builds omit digital signing and
verification; macOS bundles are unsigned and not notarized. CI builds a separate
x86-64 Ubuntu DEB and smoke-tests it on Ubuntu 22.04 and 24.04.

The native Linux package job first builds/tests with OpenSSL enabled, then
reconfigures the same build directory with `JPDF_DESK_WITH_OPENSSL=OFF` and runs
all available tests again. Reusing Qt/MuPDF avoids a second full dependency build;
packaged artifacts in `dist/` are unaffected. Diagnostics retain separate
`test-results.xml` and `test-results-no-openssl.xml` reports. Both configurations
must pass before the job can succeed or a tagged release can be published.

[`script/ci-build.sh`](../../script/ci-build.sh) collects preset artifacts and
checksums in `dist/<preset>/`. Actions artifacts are retained for 14 days.
The [GitLab pipeline](../../.gitlab-ci.yml) also builds and packages the app;
its optional Snap job requires a Snapcraft/LXD shell runner.

Update versions in `CMakeLists.txt` and `snap/snapcraft.yaml`, then push a version
tag (for example, `v1.1.0`) to publish a GitHub Release after all builds succeed.
Tags with a suffix (for example, `v1.1.0-rc1`) publish prereleases and are ignored
by the app’s update check. Match the stable tag to the version in `CMakeLists.txt`.
Snap Store publication is separate. Check workflow results and test binaries on
the target OS before distribution.

## Linux AArch64 cross-builds

The CI image includes the cross-toolchain and ARM64 development packages through
Debian multiarch. Build native Qt host tools first, then configure ARM64:

```sh
cmake --preset release-linux
cmake --build --preset release-linux --target jpdf-desk --parallel 2
cmake --preset release-linux-arm64 --fresh
cmake --build --preset release-linux-arm64 --parallel 2
```

The toolchain selects ARM64 libraries and isolates pkg-config automatically;
no separate sysroot is needed. Republish the CI image after Dockerfile changes.
