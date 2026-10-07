#!/usr/bin/env bash
# Build one public CMake preset and collect distributable artifacts.
set -euo pipefail

preset=${1:?Usage: bash script/ci-build.sh PRESET}
jobs=${BUILD_JOBS:-2}
case "$preset" in
    debug|release|release-win|linux-executable|linux-aarch64|windows-executable|linux-deb|linux-rpm|linux-appimage|windows-installer|macos-arm64) ;;
    *) printf 'Unsupported CI preset: %s\n' "$preset" >&2; exit 1 ;;
esac

git config --global --add safe.directory "$PWD"
git submodule sync
git submodule update --init thirdparty/qtbase thirdparty/mupdf
git -C thirdparty/mupdf submodule sync
git -C thirdparty/mupdf submodule update --init \
    thirdparty/freetype thirdparty/jbig2dec thirdparty/libjpeg \
    thirdparty/lcms2 thirdparty/openjpeg thirdparty/zlib

case "$preset" in
    release-win|windows-executable|windows-installer|linux-aarch64)
        # Cross-builds need native Qt code-generation tools.
        cmake --preset release --fresh
        cmake --build --preset release --target jpdf-desk --parallel "$jobs"
        ;;
esac

set --
case "$preset" in
    release-win|windows-executable|windows-installer)
        set -- -DJPDF_DESK_WITH_OPENSSL=OFF
        ;;
esac
cmake --preset "$preset" --fresh "$@"
cmake --build --preset "$preset" --parallel "$jobs"

output="dist/$preset"
mkdir -p "$output"
case "$preset" in
    debug)
        ctest --preset debug --no-tests=error --output-junit test-results.xml
        cp .build/debug/gui/jpdf-desk "$output/"
        ;;
    release)
        cp .build/linux-release/gui/jpdf-desk "$output/"
        ;;
    release-win)
        cp .build/windows-release/gui/jpdf-desk.exe "$output/"
        ;;
    windows-executable)
        cp ".build/$preset/gui/jpdf-desk.exe" "$output/"
        ;;
    linux-deb|linux-rpm|windows-installer)
        cpack --preset "$preset"
        case "$preset" in
            linux-deb) cp .build/linux-deb/packages/*.deb "$output/" ;;
            linux-rpm) cp .build/linux-rpm/packages/*.rpm "$output/" ;;
            windows-installer)
                cp .build/windows-installer/packages/*.exe "$output/"
                cp .build/windows-installer/gui/jpdf-desk.exe "$output/"
                ;;
        esac
        ;;
    linux-appimage)
        cp .build/linux-appimage/packages/*.AppImage "$output/"
        ;;
    macos-arm64)
        # Install only the application bundle, not the vendored Qt SDK.
        cmake --install .build/macos-arm64 --prefix "$output/app" --component Runtime
        tar -czf "$output/jpdf-desk-macos-arm64.tar.gz" -C "$output/app" .
        rm -rf "$output/app"
        ;;
    *) cp ".build/$preset/gui/jpdf-desk" "$output/" ;;
esac
(
    cd "$output"
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum ./* > SHA256SUMS
    else
        shasum -a 256 ./* > SHA256SUMS
    fi
)
