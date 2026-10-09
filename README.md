# jPDF Desk

![jPDF Desk logo](branding/jpdf-desk-logo-256.png)

A desktop app to view, fill, sign, and secure PDFs.

- Browse pages, thumbnails, and bookmarks; search selectable text.
- Fill native forms or add text, checkmarks, crosses, and signature images.
- Sign with `.p12`/`.pfx` certificates and check digital signatures.
- Edit metadata and password protection, print, and choose light/dark themes.

Open a PDF, choose a sidebar tool, and move the content into position.
Use **Save as PDF** with a different filename. See the [user guide](docs/usage/guide.md).

## Platforms

Linux: executable, DEB, RPM, AppImage, or Snap.
Windows: executable or installer. Apple Silicon macOS: app bundle.
Prebuilt downloads may not cover every platform.

See [installation](docs/usage/installation.md) or [build instructions](docs/development/build.md).

## Signature safety

Signature images are not digital signatures and can be extracted from PDFs.
Finish editing before digitally signing; later changes may invalidate signatures.
Integrity does not establish signer trust. Imported certificates contain private
keys; use password-protected files. See [signing details](docs/usage/guide.md#digital-signatures).

## Licensing

The project's source uses the [MIT License](LICENSE). Dependencies have separate
obligations: **MuPDF uses AGPL or commercial terms**. Review MuPDF, Qt, and other
dependency licenses before redistributing binaries; MIT does not override them.

## Documentation

See the [documentation index](docs/README.md) for usage, builds, architecture, and CI.
