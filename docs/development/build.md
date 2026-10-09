# Build, test, and package

[Documentation index](../README.md) · [CI setup](ci.md)

Run commands from the repository root.

## Requirements

Install CMake 3.25+, Ninja, a C++17 compiler, GNU Make, Git, Perl, Python 3,
pkg-config, and OpenSSL development libraries. Linux also needs font, OpenGL,
X11/XCB, and CUPS development libraries; see the [dependency list](../../docker/Dockerfile).
Install CUPS headers before configuring to enable physical printing.
Qt and MuPDF are built from submodules; no system SDKs are needed.

## Initialize sources

```sh
git submodule update --init thirdparty/qtbase thirdparty/mupdf
git -C thirdparty/mupdf submodule update --init \
    thirdparty/freetype thirdparty/jbig2dec thirdparty/libjpeg \
    thirdparty/lcms2 thirdparty/openjpeg thirdparty/zlib
```

## Build on Linux

```sh
cmake --preset release-linux --fresh
cmake --build --preset release-linux --target jpdf-desk
.build/release-linux/gui/jpdf-desk
```

## Run tests

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

## Linux packages

After building `release-linux`, install the relevant packaging tool (`dpkg-deb`
and `dpkg-shlibdeps` for DEB, `rpmbuild` for RPM), then run:

```sh
cpack --preset release-linux-deb
cpack --preset release-linux-rpm
```

Packages appear in `.build/release-linux/packages/`.
For AppImage, install `linuxdeploy` and `linuxdeploy-plugin-appimage` on `PATH`
and run `bash script/ci-build.sh release-linux`; artifacts appear in `dist/release-linux/`.

For a standalone executable, configure and build the `release-linux-static` preset.
Linux binaries still require compatible OS desktop libraries.

## Snap packaging

Install Snapcraft and initialize LXD, then run:

```sh
snapcraft --use-lxd
```

Snap builds and tests in its own Ubuntu base; do not reuse the Debian CI binary.
See [Snap installation](../usage/installation.md#snap).

## Windows cross-build

Install MinGW-w64 and build `release-linux` first for Qt host tools, then run:

```sh
cmake --preset release-windows
cmake --build --preset release-windows --target jpdf-desk
cpack --preset release-windows-installer
```

The installer requires NSIS. The executable is `.build/release-windows/gui/jpdf-desk.exe`;
installers appear in `.build/release-windows/packages/`. Digital signing is disabled
in this preset. Test the result on Windows.

## macOS build

On Apple Silicon, install the build tools and OpenSSL development libraries, then run:

```sh
cmake --preset release-mac-arm64
cmake --build --preset release-mac-arm64
open .build/release-mac-arm64/gui/jpdf-desk.app
```

## Linux AArch64 cross-build

Follow the [ARM64 setup](ci.md#linux-aarch64-cross-builds) for the toolchain,
sysroot, and Qt host tools.

Review [dependency licenses](../../README.md#licensing) before distributing binaries.
