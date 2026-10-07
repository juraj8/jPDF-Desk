# jPDF Desk: usage and technical guide

[Back to the overview](../README.md)

- [Using the app](#using-the-app)
- [Text search](#text-search)
- [Metadata and password protection](#metadata-and-password-protection)
- [Appearance](#appearance)
- [Printing](#printing)
- [Cryptographic signing](#cryptographic-signing)
- [About](#about)
- [Build](#build)
- [Architecture and tests](#architecture-and-tests)
- [Builds without OpenSSL](#builds-without-openssl)
- [Distribution variants](#distribution-variants)
- [Linux AArch64 cross-build](#linux-aarch64-cross-build)
- [Windows cross-build](#windows-cross-build)
- [macOS build](#macos-build)
- [Branding and application identity](#branding-and-application-identity)
- [Licensing](#licensing)

Paths and commands below are relative to the repository root.

## Using the app

Open a PDF and use the left-hand **Tools** section to add text, checkmarks, crosses, or a signature near the center of the current page's visible area. Double-click text to edit it; select it to change its size or drag its handle. Drag marks and signatures to their positions. Select a signature to resize or delete it. Use Previous/Next to work across pages, then **Save as PDF**. Edits are PDF annotations and can be moved or removed after reopening a saved PDF; unrelated annotations are left intact.

The collapsible panel on the right offers **Outline** and **Thumbnails** views. Expand nested bookmarks and click one (or press Enter) to jump to its page, or choose a page thumbnail. PDFs without bookmarks use thumbnails instead; external bookmarks are listed but do not launch links. Use the arrow at the right of the header to hide or show the panel. Page navigation stays centered when space allows, with search controls to its right.

Use the **+ / −** controls at the lower right or Ctrl+mouse wheel to zoom between 25% and 400%.

Open and Save support Ctrl+O / Ctrl+S (Cmd on macOS). **Save as PDF** requires a different path from the open PDF. After saving, the output becomes the open document, so subsequent saves also need a different name.

To add a handwritten signature, choose **Manage signature images…**, import a PNG/JPEG of dark ink on light paper, and choose **Use selected**. The app removes the light background and crops to the ink. Click the **Signature** tile to place it. The manager supports multiple images, previews, renaming, and removal; your selected image is remembered across sessions and PDFs. On saving, the processed PNG is also stored in the PDF's `JPDFDeskSignature` metadata, and each placed signature carries its image. An embedded template is used when opening a PDF if no local image is selected; it is not automatically added to your library. **This is not a cryptographic digital signature. Anyone with access to the PDF can extract the signature image**, even if all placed signatures are subsequently deleted. Do not share the PDF if that is a concern.

## Text search

Focus the search field with Ctrl+F (Cmd+F on macOS), enter a query, and press Enter. Use the previous/next match buttons or F3 / Shift+F3 to navigate with wraparound. Search is case-insensitive and searches selectable document text, not scanned images (no OCR) or unsaved annotations. Highlights are temporary and are not saved. Changing the query clears existing results.

## Metadata and password protection

Choose **More… → File metadata…** to edit document properties. Date fields are read-only; clearing an editable field removes its value. Changes are included in the next saved copy.

Password-protected PDFs prompt for a password when opened. Choose **More… → PDF password…** to set or replace a password, or select **Remove password protection**. Confirm the new password and save to a different PDF. New protection uses AES-256; passwords are limited to 127 UTF-8 bytes and cannot contain null characters. Existing protection is retained unless you change it. The original file is unchanged, and changing encryption may invalidate digital signatures.

## Appearance

Use **Appearance** at the bottom of the sidebar to choose **System**, **Light**, or **Dark**. The choice is remembered across sessions; System follows OS appearance changes when supported by Qt (Qt versions older than 6.5 fall back to Light). Dark mode uses charcoal surfaces, readable text, and distinct selection, keyboard-focus, hover, and disabled states throughout the app's menus and dialogs.

PDF pages, thumbnails, and annotation colors are not inverted or modified, and switching themes preserves unsaved edits. Signature previews retain a white background so ink remains legible. Native OS file dialogs follow the platform's appearance rather than the app's override; the app's custom print dialog follows its theme.

## Printing

Choose **Print…** (or Ctrl+P / Cmd+P) to open the app's print dialog. Select a printer or **Print to File (PDF)**, then choose all pages, the current page, or ranges such as `1-3, 5, 8`. **Preview** shows the selected layout before printing.

Options include copies/collation, reverse order, odd/even pages, 1/2/4/6/9/16 pages per side, page ordering, paper size/orientation, grayscale, fit or actual-size scaling, and a 10–200% scale adjustment. Duplex, paper-source, and resolution options depend on the selected printer. Unsupported backend settings are disabled.

Current unsaved text, marks, and signature images are included in a temporary print snapshot; the open PDF is not saved or modified. Pages are rasterized at up to 300 DPI. Fit is the default; actual-size or enlarged output may crop content. Printing does not transfer a cryptographic signature to paper or to the printer's PDF output.

On Linux, physical printers require CUPS and a configured server. See [Build](#build) for backend dependencies. **Current limitation:** snapshot printing reopens the source without a password, so password-protected source PDFs cannot currently be printed through this workflow.

## Cryptographic signing

Use **Manage certificates…** to import, select, rename, or remove PKCS#12 certificates (`.p12`/`.pfx`) containing your private key. The app keeps its own copy and remembers the selected certificate across sessions. Choose **Sign PDF…**, confirm the identity in the certificate manager, enter its password, and choose a different output PDF. Certificates are validated by the signing provider when used, not during import. All current edits are included in an invisible cryptographic signature; no handwritten signature image is required. Passwords are never saved in application settings or PDF metadata. Failed signing does not replace the output file.

Both libraries are stored in Qt's per-user `AppLocalDataLocation`, in `signature-images` and `signing-certificates` subdirectories. Each manager displays the exact location. This normally resides under `$XDG_DATA_HOME` (or `~/.local/share`) on Linux, `%LOCALAPPDATA%` on Windows, and `~/Library/Application Support` on macOS. Imports and removals take effect immediately; removing a stored copy does not alter the original file or existing PDFs. Assets are limited to 5 MB each. On Unix, directories and files are restricted to the owner; on Windows, access relies on the user profile's inherited permissions. **Certificate copies contain private keys: import password-protected PKCS#12 files. The app does not add encryption or use an OS keychain.**

Choose **Check signatures…** and expand **Details** to see each signature's signer, integrity check, certificate check, and whether later PDF revisions exist. Unsigned signature fields are also listed. Verification reads the saved PDF and excludes unsaved edits. Intact signed bytes do not mean subsequent revisions are signed or that the signer is trusted. With the default OpenSSL provider, certificate checking uses MuPDF's built-in certificate store, not the system or your PDF viewer's trust store, and does not perform online revocation or timestamp validation. The verification dialog describes the configured provider's trust policy. You can also validate the result in a signature-aware PDF viewer. Certificate trust depends on the recipient's trusted certificates; self-signed certificates will generally be untrusted. Finish editing before signing: later saves may invalidate signatures, and this workflow does not preserve earlier signatures when re-signing. Timestamping, hardware tokens, and long-term validation are not supported.

## About

The **About…** button at the bottom of the left menu shows the project name, version, description, author, license, platform, and Qt version. Project information is embedded at build time from CMake. Set `JPDF_DESK_PROJECT_URL` to the project's public homepage to enable its link; `JPDF_DESK_AUTHOR` defaults to Juraj Giertl. For example, pass `-DJPDF_DESK_PROJECT_URL=https://your-project-homepage` when configuring. No homepage is assumed by default.

## Build

The executable and build target are `jpdf-desk`; configuration keys use the `JPDF_DESK_*` prefix. If upgrading an existing build directory, configure with `--fresh` and update any locally supplied CMake options to the new prefix.

Requires CMake 3.25+, Ninja (used by the presets), a C++17 compiler, GNU Make, and the QtBase and MuPDF source submodules in `thirdparty/` (pinned to Qt 6.12.0 and MuPDF 1.28.5). The default cryptography provider also requires OpenSSL development libraries (for example `libssl-dev` on Debian/Ubuntu); its integration test requires the `openssl` command-line tool. No system MuPDF installation is required. Also install Git, Perl, Python 3, and pkg-config. Linux builds need font, OpenGL, and X11/XCB development libraries; the [CI Dockerfile](../docker/Dockerfile) is the maintained Debian 13 dependency list.

For Linux physical printing, install the CUPS development package (for example `libcups2-dev`) **before configuring Qt**. If already configured without CUPS, reconfigure with `--fresh`; builds without the backend support PDF output only and emit a warning.

```sh
git submodule update --init thirdparty/qtbase thirdparty/mupdf
# Only the MuPDF third-party sources required for PDF support:
git -C thirdparty/mupdf submodule update --init \
    thirdparty/freetype thirdparty/jbig2dec thirdparty/libjpeg \
    thirdparty/lcms2 thirdparty/openjpeg thirdparty/zlib
cmake --preset release --fresh
cmake --build --preset release --target jpdf-desk
.build/linux-release/gui/jpdf-desk
```

The first build compiles QtBase and a static MuPDF (with its required third-party code) locally. The executable does not depend on `libmupdf.so` at runtime. MuPDF's optional HTML, JavaScript, SVG, XPS, and export engines are disabled for this PDF-only application. Build directories are local to each preset; no system Qt SDK installation is needed.

## Architecture and tests

Architecture is documented in [GUI architecture](gui.md) and [Reusable PDF libraries](libraries.md). Application tests live in `test/`, separate from production targets.

Release/distribution presets disable tests and prefer static OpenSSL libraries. Run tests with the development preset:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Widget and printing tests run with Qt's offscreen platform. Signing/provider tests use no widgets, and the provider-contract test does not link OpenSSL. [CI and cross-build setup](ci.md) describes automated coverage and artifact collection.

## Builds without OpenSSL

```sh
cmake --preset release -DJPDF_DESK_WITH_OPENSSL=OFF
cmake --build --preset release --target jpdf-desk
```

Opening, editing, saving, and printing still work. Digital signing/verification controls are disabled unless providers are injected. Signature images remain available: they are not cryptographic signatures. Reconfigure with `-DJPDF_DESK_WITH_OPENSSL=ON` to restore the default provider.

## Distribution variants

Distribution presets build only the application and its dependencies (no tests or Qt SDK installation), in separate `.build/<preset>` directories. Windows variants cross-compile on Linux with MinGW-w64; see [Windows cross-build](#windows-cross-build).

| Preset | Result | Extra packaging tools |
| --- | --- | --- |
| `macos-arm64` | `gui/jpdf-desk.app` (Apple Silicon) | Native macOS build; no installer preset |
| `linux-executable` | `gui/jpdf-desk` | None |
| `linux-aarch64` | ARM64 `gui/jpdf-desk` | AArch64 toolchain, sysroot, Qt host tools |
| `windows-executable` | `gui/jpdf-desk.exe` | MinGW-w64 |
| `windows-installer` | Windows installer `.exe` | MinGW-w64, NSIS (`makensis`) |
| `linux-deb` | `.deb` | `dpkg-deb`, `dpkg-shlibdeps` (Debian/Ubuntu) |
| `linux-appimage` | `.AppImage` | `linuxdeploy` and its AppImage output plugin |
| `linux-rpm` | `.rpm` | `rpmbuild` (optional) |

For example:

```sh
cmake --preset linux-executable
cmake --build --preset linux-executable
.build/linux-executable/gui/jpdf-desk

cmake --preset linux-deb
cmake --build --preset linux-deb
cpack --preset linux-deb

cmake --preset linux-appimage
cmake --build --preset linux-appimage  # also creates the AppImage
```

Set the package contact explicitly with `-DJPDF_DESK_AUTHOR_EMAIL=you@example.com` when configuring, or in a preset's `cacheVariables`. The default is set in `CMakeLists.txt`; it is never taken from Git configuration.

Use the same configure/build/`cpack` sequence for `linux-rpm` and `windows-installer`. Packages appear under `.build/<preset>/packages/`. Installer packages include the executable, Linux desktop entry/icon where applicable, and uninstall support on Windows; they do not include Qt development files. The Windows installer registers PDF support in **Open with** and **Settings → Apps → Default apps** without changing your existing default. After installation, choose jPDF Desk there to make it your default PDF app. Uninstall removes the app's registrations.

“Single executable” means Qt, MuPDF, and OpenSSL (when enabled) are linked into the application. **Linux still needs OS desktop libraries** (X11, OpenGL, CUPS, libc, etc.); it is not a fully static, universally portable binary. AppImage bundles redistributable shared dependencies using linuxdeploy, but still requires a compatible host (build on the oldest Linux distribution you intend to support). Install `linuxdeploy` and `linuxdeploy-plugin-appimage` on `PATH`, or set `JPDF_DESK_LINUXDEPLOY_EXECUTABLE` explicitly. Packaging automatically enables extract-and-run mode for AppImage-based tools, so FUSE is not required in containers.

## Linux AArch64 cross-build

The `linux-aarch64` preset requires an AArch64 GCC/G++ toolchain, target libraries in `/opt/sysroot`, and native Qt host tools from a `release` build. See [sysroot setup and build commands](ci.md#linux-aarch64-cross-builds).

## Windows cross-build

The `release-win`, `windows-executable`, and `windows-installer` presets use MinGW-w64 on Linux and require Qt host tools built with `release`. Windows builds default to **OpenSSL disabled**. To enable cryptographic signing, provide **static Windows-target** OpenSSL headers and libraries, not the Linux installation:

```sh
cmake --preset release
cmake --build --preset release --target jpdf-desk
```

Then configure the Windows target:

```sh
cmake --preset release-win -DJPDF_DESK_WITH_OPENSSL=ON -DOPENSSL_ROOT_DIR=/path/to/windows-openssl
cmake --build --preset release-win --target jpdf-desk
```

For an editing/printing build without a cryptography dependency:

```sh
cmake --preset release-win -DJPDF_DESK_WITH_OPENSSL=OFF
cmake --build --preset release-win --target jpdf-desk
```

Native Windows certificate-store/CNG and macOS Keychain providers are extension points, not implemented backends. The current provider uses `.p12/.pfx` files on every OS. MinGW runtimes are linked statically. The executable should not require Qt, OpenSSL, or MinGW DLLs; verify this and test the installer, printer integration, and signing on Windows before distribution. Cross-compilation alone does not validate runtime behavior.

## macOS build

The `macos-arm64` preset builds an application bundle natively on Apple Silicon macOS. It does not provide a DMG installer or an Intel macOS preset. Initialize the source submodules as described under [Build](#build), install the build tools and OpenSSL development libraries, then run:

```sh
cmake --preset macos-arm64 -DOPENSSL_ROOT_DIR=/path/to/openssl
cmake --build --preset macos-arm64
open .build/macos-arm64/gui/jpdf-desk.app
```

Use `-DJPDF_DESK_WITH_OPENSSL=OFF` instead of the OpenSSL path for an editing/printing build without digital signing or verification.

## Branding and application identity

The app logo is available as an editable vector in `branding/jpdf-desk-logo.svg`, with 256px and 512px PNG exports. The 256px icon is embedded in the GUI for its sidebar and window icon; no external image is needed at runtime.

The app uses **jPDF Desk** for its Qt settings/data identity. On first launch, preferences are copied from older releases without overwriting current choices, and signature-image/certificate libraries are moved to the new location if no current library exists. Existing private-file permissions are preserved. If both versions already have libraries, the current library is kept and the old one remains untouched; reimport missing assets through the managers. Migration failures show a warning and leave unmoved assets at their original location.

Previously saved annotations and embedded signature templates remain readable. New annotations and signature-template metadata use the jPDF Desk identity.

## Licensing

See the [overview's licensing notes](../README.md#licensing) before distributing binaries. Static linking does not remove dependency license obligations.
