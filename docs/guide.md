# jPDF Desk: usage and technical guide

[Back to the overview](../README.md)

- [Using the app](#using-the-app)
- [Appearance](#appearance)
- [Printing](#printing)
- [Cryptographic signing](#cryptographic-signing)
- [Build](#build)
- [Architecture and tests](#architecture-and-tests)
- [Distribution variants](#distribution-variants)
- [Windows cross-build](#windows-cross-build)
- [macOS build](#macos-build)
- [Branding and application identity](#branding-and-application-identity)

Paths and commands below are relative to the repository root.

## Using the app

Open a PDF and use the left-hand **Tools** section to add text, checkmarks, crosses, or a signature. Double-click text to edit it; select it to change its size or drag its handle. Drag marks and signatures to their positions. Select a signature to resize or delete it. Use Previous/Next to work across pages, then **Save as PDF**. Edits are PDF annotations and can be moved or removed after reopening a saved PDF; unrelated annotations are left intact.

PDF bookmarks appear in a collapsible **Outline** panel on the right. Expand or collapse nested sections and click a bookmark (or press Enter) to jump to its page. Use the arrow button at the right of the top header to hide or show it. Page navigation stays centered in the header, between the tools and outline toggles. PDFs without an outline keep the panel collapsed; external bookmarks are listed but do not launch links.

The sidebar separates file actions from a compact **Text / Checkmark / Cross / Signature** tool palette. **Save as PDF** is the primary action; file metadata is under **More…**. Open and Save support Ctrl+O / Ctrl+S (Cmd on macOS).

To add a handwritten signature, choose **Manage signature images…**, import a PNG/JPEG of dark ink on light paper, and choose **Use selected**. The app removes the light background and crops to the ink. Click the **Signature** tile to place it. The manager supports multiple images, previews, renaming, and removal; your selected image is remembered across sessions and PDFs. On saving, the processed PNG is also stored in the PDF's `PdfFillerSignature` metadata, and each placed signature carries its image. An embedded template is used when opening a PDF if no local image is selected; it is not automatically added to your library. **This is not a cryptographic digital signature. Anyone with access to the PDF can extract the signature image**, even if all placed signatures are subsequently deleted. Do not share the PDF if that is a concern.

## Appearance

Use **Appearance** at the bottom of the sidebar to choose **System**, **Light**, or **Dark**. The choice is remembered across sessions; System follows OS appearance changes when supported by Qt (Qt versions older than 6.5 fall back to Light). Dark mode uses charcoal surfaces, readable text, and distinct selection, keyboard-focus, hover, and disabled states throughout the app's menus and dialogs.

PDF pages, thumbnails, and annotation colors are not inverted or modified, and switching themes preserves unsaved edits. Signature previews retain a white background so ink remains legible. Native OS file and print dialogs follow the platform's appearance rather than the app's override.

## Printing

Choose **Print…** (or Ctrl+P / Cmd+P) to select a printer and print all pages, a page range, or the current page. Current unsaved text, marks, and signature images are included in a temporary print snapshot; the open PDF is not saved or modified. Pages are rasterized at up to 300 DPI and fitted proportionally inside the printer's printable area using the selected paper size and orientation. Printing does not transfer a cryptographic signature to paper or to the printer's PDF output.

Printer access stays behind Qt: Windows uses the Windows spooler, macOS uses Qt's native print support, and Linux uses CUPS. No PDF code calls an OS printer API directly.

On Linux, physical printers require Qt's CUPS backend and access to a configured CUPS server. Install the CUPS development package (for example `libcups2-dev` on Debian/Ubuntu) **before configuring Qt**, then reconfigure with `--fresh` if Qt was previously built without CUPS. Builds without this backend support PDF output only and emit a CMake warning.

## Cryptographic signing

Use **Manage certificates…** to import, select, rename, or remove PKCS#12 certificates (`.p12`/`.pfx`) containing your private key. The app keeps its own copy and remembers the selected certificate across sessions. Choose **Sign PDF…**, confirm the identity in the certificate manager, enter its password, and choose a different output PDF. Certificates are validated by the signing provider when used, not during import. All current edits are included in an invisible cryptographic signature; no handwritten signature image is required. Passwords are never saved in application settings or PDF metadata. Failed signing does not replace the output file.

Both libraries are stored in Qt's per-user `AppLocalDataLocation`, in `signature-images` and `signing-certificates` subdirectories. Each manager displays the exact location. This normally resides under `$XDG_DATA_HOME` (or `~/.local/share`) on Linux, `%LOCALAPPDATA%` on Windows, and `~/Library/Application Support` on macOS. Imports and removals take effect immediately; removing a stored copy does not alter the original file or existing PDFs. Assets are limited to 5 MB each. On Unix, directories and files are restricted to the owner; on Windows, access relies on the user profile's inherited permissions. **Certificate copies contain private keys: import password-protected PKCS#12 files. The app does not add encryption or use an OS keychain.**

Choose **Check signatures…** and expand **Details** to see each signature's signer, integrity check, certificate check, and whether later PDF revisions exist. Unsigned signature fields are also listed. Verification reads the saved PDF and excludes unsaved edits. Intact signed bytes do not mean subsequent revisions are signed or that the signer is trusted. With the default OpenSSL provider, certificate checking uses MuPDF's built-in certificate store, not the system or your PDF viewer's trust store, and does not perform online revocation or timestamp validation. The verification dialog describes the configured provider's trust policy. You can also validate the result in a signature-aware PDF viewer. Certificate trust depends on the recipient's trusted certificates; self-signed certificates will generally be untrusted. Finish editing before signing: later saves may invalidate signatures, and this workflow does not preserve earlier signatures when re-signing. Timestamping, hardware tokens, and long-term validation are not supported.

## About

The **About…** button at the bottom of the left menu shows the project name, version, description, author, license, platform, and Qt version. Project information is embedded at build time from CMake. Set `PDF_FILLER_PROJECT_URL` to the project's public homepage to enable its link; `PDF_FILLER_AUTHOR` defaults to Juraj Giertl. For example, pass `-DPDF_FILLER_PROJECT_URL=https://your-project-homepage` when configuring. No homepage is assumed by default.

## Build

Requires CMake 3.25+, Ninja (used by the presets), a C++17 compiler, GNU Make, and the QtBase and MuPDF source submodules in `thirdparty/` (pinned to Qt 6.12.0 and MuPDF 1.28.5). The default cryptography provider also requires OpenSSL development libraries (for example `libssl-dev` on Debian/Ubuntu); its integration test requires the `openssl` command-line tool. No system MuPDF installation is required. Install Qt/X11 build dependencies for your distribution.

```sh
git submodule update --init thirdparty/qtbase thirdparty/mupdf
# Only the MuPDF third-party sources required for PDF support:
git -C thirdparty/mupdf submodule update --init \
    thirdparty/freetype thirdparty/jbig2dec thirdparty/libjpeg \
    thirdparty/lcms2 thirdparty/openjpeg thirdparty/zlib
cmake --preset release --fresh
cmake --build --preset release --target pdf-filler
.build/linux-release/gui/pdf-filler
```

The first build compiles QtBase and a static MuPDF (with its required third-party code) locally. The executable does not depend on `libmupdf.so` at runtime. MuPDF's optional HTML, JavaScript, SVG, XPS, and export engines are disabled for this PDF-only application. Saving requires a **different path** from the open PDF; after saving, the output becomes the open document so it can be edited and saved again under another name. Pages with edits are saved together. The unrelated simulated firmware CLI and its library have been removed; the build now contains only the PDF application and its tests.

## Architecture and tests

- `lib/document/`: annotation values and the MuPDF document backend (`PdfFiller::Document`), linked to QtGui but not QtWidgets or OpenSSL.
- `lib/signing/`: PDF signature embedding/verification (`PdfFiller::Signing`), separate signing and verification provider interfaces, and an optional portable OpenSSL adapter (`PdfFiller::OpenSSL`).
- `lib/printing/`: Qt-based print jobs, page layout, and isolated print snapshots (`PdfFiller::Printing`). Printer-selection dialogs remain in the GUI.
- `gui/ui/`: reusable controls, annotation scene items, and scene/model conversion. Shared navigation button setup lives in `buttons.*`; semantic theme colors, palettes, and persisted appearance preferences live in `theme.*`, with shared visual rules in `window_style.*`.
- `gui/signature_store.*`: per-user signature-image and certificate libraries, atomic file writes, and persistent selection. The GUI managers live in `gui/ui/signature_manager.*`; no passwords are persisted.
- `gui/main_window.*`: application workflow and document view orchestration. The scene owns unsaved edits; saving and printing capture local snapshots, including empty pages to preserve deletions.
- `test/`: application tests, separate from production targets.

Release/distribution presets disable tests and prefer static OpenSSL libraries. Run tests with the development preset:

```sh
cmake --preset debug
cmake --build --preset debug
ctest --preset debug
```

Widget and printing tests run with Qt's offscreen platform. Signing/provider tests use no widgets, and the provider-contract test does not link OpenSSL. See [`lib/README.md`](../lib/README.md) for library usage, provider contracts, and platform extension points.

### Builds without OpenSSL

```sh
cmake --preset release -DPDF_FILLER_WITH_OPENSSL=OFF
cmake --build --preset release --target pdf-filler
```

Opening, editing, saving, and printing still work. Digital signing/verification controls are disabled unless providers are injected. Signature images remain available: they are not cryptographic signatures. Reconfigure with `-DPDF_FILLER_WITH_OPENSSL=ON` to restore the default provider.

### Distribution variants

All distribution presets build only the application and its dependencies (no tests or Qt SDK installation). They use separate `.build/<preset>` directories. The Windows variants currently cross-compile on Linux with MinGW-w64, like `release-win`; they are not native MSVC presets.

| Preset | Result | Extra packaging tools |
| --- | --- | --- |
| `macos-arm64` | `gui/pdf-filler.app` (Apple Silicon) | Native macOS build; no installer preset |
| `linux-executable` | `gui/pdf-filler` | None |
| `windows-executable` | `gui/pdf-filler.exe` | MinGW-w64 |
| `windows-installer` | Windows installer `.exe` | MinGW-w64, NSIS (`makensis`) |
| `linux-deb` | `.deb` | `dpkg-deb`, `dpkg-shlibdeps` (Debian/Ubuntu) |
| `linux-appimage` | `.AppImage` | `linuxdeploy` and its AppImage output plugin |
| `linux-rpm` | `.rpm` | `rpmbuild` (optional) |

For example:

```sh
cmake --preset linux-executable
cmake --build --preset linux-executable
.build/linux-executable/gui/pdf-filler

cmake --preset linux-deb
cmake --build --preset linux-deb
cpack --preset linux-deb

cmake --preset linux-appimage
cmake --build --preset linux-appimage  # also creates the AppImage
```

Set the package contact explicitly with `-DPDF_FILLER_AUTHOR_EMAIL=you@example.com` when configuring, or in a preset's `cacheVariables`. The default is set in `CMakeLists.txt`; it is never taken from Git configuration.

Use the same configure/build/`cpack` sequence for `linux-rpm` and `windows-installer`. Packages appear under `.build/<preset>/packages/`. Installer packages include the executable, Linux desktop entry/icon where applicable, and uninstall support on Windows; they do not include Qt development files. The Windows installer registers PDF support in **Open with** and **Settings → Apps → Default apps** without changing your existing default. After installation, choose jPDF Desk there to make it your default PDF app. Uninstall removes the app's registrations.

“Single executable” means Qt, MuPDF, and OpenSSL are linked into the application. **Linux still needs OS desktop libraries** (X11, OpenGL, CUPS, libc, etc.); it is not a fully static, universally portable binary. AppImage bundles redistributable shared dependencies using linuxdeploy, but still requires a compatible host (build on the oldest Linux distribution you intend to support). Install `linuxdeploy` and `linuxdeploy-plugin-appimage` on `PATH`, or set `PDF_FILLER_LINUXDEPLOY_EXECUTABLE` explicitly. Packaging automatically enables extract-and-run mode for AppImage-based tools, so FUSE is not required in containers.

Windows presets additionally link the MinGW runtimes statically. Build Qt host tools first with `release`. Supply **static Windows-target OpenSSL** via `-DOPENSSL_ROOT_DIR=...` to each Windows configure command, or use `-DPDF_FILLER_WITH_OPENSSL=OFF`. The executable should not require Qt, OpenSSL or MinGW DLLs; verify this and exercise the installer on Windows before distribution.

### Windows cross-build

The `release-win` preset uses MinGW-w64 and requires Qt host tools built with the `release` preset. MuPDF is also compiled with the target compiler; the application owns its Windows manifest. For cryptographic signing, provide **Windows-target** OpenSSL headers and libraries, not the Linux installation:

```sh
cmake --preset release-win -DPDF_FILLER_WITH_OPENSSL=ON -DOPENSSL_ROOT_DIR=/path/to/windows-openssl
cmake --build --preset release-win --target pdf-filler
```

For an editing/printing build without a cryptography dependency:

```sh
cmake --preset release-win -DPDF_FILLER_WITH_OPENSSL=OFF
cmake --build --preset release-win --target pdf-filler
```

Native Windows certificate-store/CNG and macOS Keychain providers are extension points, not implemented backends. The current provider uses `.p12/.pfx` files on every OS. Cross-compilation checks do not replace testing native printer dialogs, physical printing, and signing on the target OS.

## macOS build

The `macos-arm64` preset builds an application bundle natively on Apple Silicon macOS. It does not provide a DMG installer or an Intel macOS preset. Initialize the source submodules as described under [Build](#build), install the build tools and OpenSSL development libraries, then run:

```sh
cmake --preset macos-arm64 -DOPENSSL_ROOT_DIR=/path/to/openssl
cmake --build --preset macos-arm64
open .build/macos-arm64/gui/pdf-filler.app
```

Use `-DPDF_FILLER_WITH_OPENSSL=OFF` instead of the OpenSSL path for an editing/printing build without digital signing or verification.

## Branding and application identity

The app logo is available as an editable vector in `branding/pdf-filler-logo.svg`, with 256px and 512px PNG exports. The 256px icon is embedded in the GUI for its sidebar and window icon; no external image is needed at runtime.

The executable remains `pdf-filler`, and the existing settings/data identity is preserved so preferences and stored signature images and certificates remain available after upgrading.

## Licensing

The project's own code is covered by the [MIT license](../LICENSE). Bundled dependencies have separate terms: MuPDF is distributed under AGPL/commercial terms, and Qt and other dependencies also have license obligations. Static linking does not remove those obligations. Review the applicable dependency licenses before distributing binaries; see the [overview's licensing notes](../README.md#licensing).
