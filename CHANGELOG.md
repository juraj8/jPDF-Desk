# Changelog

## 1.3.0-rc2 — 2026-11-14

### Changed

- Add detailed failure diagnostics to internal-link regression tests, including
  navigation, zoom preservation, destination positioning, and editable overlays.

## 1.3.0-rc1 — 2026-11-01

### Added

- Clickable internal PDF links, including named destinations, preserving zoom
  and keeping editable annotations above link targets. External URLs and files
  are not opened.
- Regression coverage for internal links, signature-image page content and
  removal previews, and Unicode PDF paths.

### Changed

- Save signature images as page content rather than stamp annotations. Position
  and resize images before saving; saved, tagged images can be selected and
  removed, but not moved or resized. Older stamps and untagged images are not
  converted. Removing an image covered by a digital signature invalidates it.
- Update signature-image workflow hints and documentation.
- Publish native installers and packages directly as release assets, use
  tarballs for Linux standalone builds and ZIP archives for Windows standalone
  builds, and provide release checksums.
- Build both Windows installer and standalone packages in GitLab CI using the
  shared build script; omit the loose executable from installer artifacts.
- Update MuPDF to include newer Brotli dependency fixes.

### Fixed

- Use UTF-8 paths for MuPDF file operations, fixing non-ASCII filenames and
  directories on Windows when opening, saving, signing, and verifying PDFs.

## 1.2.0 — 2026-10-23

- Promote 1.2.0-rc2 to the stable release with no additional code changes.

## 1.2.0-rc2 — 2026-10-20

### Changed

- Publish Linux AppImage, DEB, and RPM packages as separate CI artifacts and
  release archives, each with its own checksums.
- Improve portability of drag-and-drop and validation-clock UI tests, accounting
  for macOS message-box titles and checking retry-error content.

## 1.2.0-rc1 — 2026-10-17

### Added

- Detailed signature reports separating integrity, certificate trust and dates,
  timestamps, revocation, and document coverage without claiming full historical
  validity.
- Offline verification of embedded RFC 3161 signature timestamps, including
  signature imprints and TSA chains; require SHA-256 or stronger timestamp hashes.
- System/OpenSSL trust-root validation alongside MuPDF's built-in certificate
  store, with platform trust-anchor imports and detailed chain diagnostics.
- Opt-in online OCSP checks with CRL fallback for validated signer and TSA chains,
  privacy confirmation, progress, cancellation, and per-certificate evidence.
- Reusable crypto and revocation libraries, bounded evidence caching, and an
  HTTP/HTTPS transport that restricts destinations, redirects, and resource use.
- Validation-clock sanity checks and an offline recheck action after correcting
  the system clock.
- Regression tests for timestamp verification, trust, revocation, HTTP transport,
  cancellation, clock validation, and signature-report behavior.
- CI coverage for builds with OpenSSL disabled, plus revocation and trust-policy
  documentation.

### Changed

- Move document actions and appearance settings into a dedicated toolbar menu
  and reorganize the sidebar.
- Simplify signature-image and certificate management with empty-state hints and
  expandable storage/privacy information.
- Allow resizing text-box widths as well as signature images.
- Keep page thumbnails and captions centered as the viewport changes size.

## 1.1.0-rc5 — 2026-10-11

- Match editable text to the PDF's Helvetica appearance and align its baseline
  using actual font metrics.
- Add text-alignment regression coverage and improve file-opening tests.

## 1.1.0-rc4 — 2026-10-10

- Prefer the configured OpenSSL installation when locating the signing-test CLI,
  avoiding incompatible system LibreSSL defaults on macOS.
- Improve portability of file-opening, main-window, and PDF-signing tests.

## 1.1.0-rc3 — 2026-10-10

- Fix MuPDF builds with Apple's archiver by disabling response files.
- Use generated C data for embedded fonts when cross-compiling or building for
  Windows, avoiding host-linker object incompatibilities.

## 1.1.0-rc2 — 2026-10-09

- Switch Linux ARM64 cross-builds to Debian multiarch dependencies instead of a
  separate sysroot, with target-specific pkg-config configuration.
- Expand and reorganize the build container's native and ARM64 dependencies.
- Add an application-owned macOS bundle property-list template.
- Update build and CI documentation.

## 1.1.0-rc1 — 2026-10-09

- Move macOS CI to macOS 15 and check the Apple toolchain for Qt compatibility.
- Clarify CI job and step names for builds, packaging, and release publication.
- Add `.nojekyll` for documentation hosting.

## 1.1.0 — 2026-10-09

### Added

- Native PDF form editing and a dedicated form dialog.
- Background PDF tasks, document-loading helpers, and a separate document view.
- File-opening diagnostics and a dedicated file-open dialog.
- Print preview and reusable print settings.
- A reusable image-loading library and expanded signature-image handling.
- Snap packaging and expanded cross-platform build and release automation.
- A documentation website, installation instructions, and reorganized usage and
  development guides.
- Tests for forms, annotations, document views, PDF tasks, file diagnostics,
  image loading, printing, and error handling.

### Changed

- Standardize application identity, build targets, library namespaces, branding,
  and packaging around jPDF Desk.
- Retain compatibility coverage for legacy documents and application identity.
- Refactor document, signing, and printing code and improve MuPDF error handling.
- Disable the build-time Git check by default.

## 1.0.0 — 2026-10-06

- Initial desktop application for viewing PDFs with page navigation, thumbnails,
  bookmarks, text search, zoom, and light/dark themes.
- Add movable text, checkmarks, crosses, and signature images to PDFs.
- Import certificate-based signing identities and sign or check PDF signatures.
- Edit document metadata and password protection, save PDFs, and print.
- Introduce document, signing, and printing libraries with automated tests.
- Add CMake build presets, Linux and Windows packaging, and CI workflows.
