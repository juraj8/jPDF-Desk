#!/usr/bin/env bash
# Build one public CMake preset and collect distributable artifacts.
set -euo pipefail

preset=${1:?Usage: bash script/ci-build.sh PRESET}
jobs=${BUILD_JOBS:-2}
case "$preset" in
    release-linux|release-windows|release-linux-static|release-linux-arm64|release-windows-static|release-mac-arm64) ;;
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
    release-windows|release-windows-static|release-linux-arm64)
        # Cross-builds need native Qt code-generation tools.
        cmake --preset release-linux --fresh
        cmake --build --preset release-linux --target jpdf-desk --parallel "$jobs"
        ;;
esac

set --
case "$preset" in
    release-windows|release-windows-static)
        set -- -DJPDF_DESK_WITH_OPENSSL=OFF
        ;;
    release-linux)
        set -- -DJPDF_DESK_BUILD_APPIMAGE=ON
        ;;
esac
cmake --preset "$preset" --fresh -DBUILD_TESTING=ON "$@"
cmake --build --preset "$preset" --parallel "$jobs"

build_dir=".build/$preset"
# Distribution presets target only the app; also compile every test target.
cmake --build "$build_dir" --parallel "$jobs"
case "$preset" in
    release-windows|release-windows-static|release-linux-arm64)
        printf 'Tests compiled only for %s: execution requires a target runner.\n' "$preset"
        ;;
    *)
        ctest --test-dir "$build_dir" --output-on-failure --no-tests=error \
            --output-junit test-results.xml
        ;;
esac

output="dist/$preset"
mkdir -p "$output"
case "$preset" in
    release-linux)
        cpack --preset release-linux-deb
        cpack --preset release-linux-rpm
        cmake --build "$build_dir" --target appimage --parallel "$jobs"
        cp "$build_dir"/packages/*.deb "$build_dir"/packages/*.rpm \
            "$build_dir"/packages/*.AppImage "$output/"
        ;;
    release-windows)
        cpack --preset release-windows-installer
        cp "$build_dir"/packages/*.exe "$build_dir/gui/jpdf-desk.exe" "$output/"
        ;;
    release-windows-static)
        cp "$build_dir/gui/jpdf-desk.exe" "$output/"
        ;;
    release-mac-arm64)
        # Install only the application bundle, not the vendored Qt SDK.
        cmake --install "$build_dir" --prefix "$output/app" --component Runtime
        tar -czf "$output/jpdf-desk-macos-arm64.tar.gz" -C "$output/app" .
        rm -rf "$output/app"
        ;;
    *) cp "$build_dir/gui/jpdf-desk" "$output/" ;;
esac
(
    cd "$output"
    if command -v sha256sum >/dev/null 2>&1; then
        sha256sum ./* > SHA256SUMS
    else
        shasum -a 256 ./* > SHA256SUMS
    fi
)
