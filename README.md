# jPDF Desk

**View, fill, sign, and secure PDFs.**

![jPDF Desk logo](branding/jpdf-desk-logo-256.png)

jPDF Desk is a desktop app for everyday PDF tasks: reading documents, filling in forms, adding signatures, and managing document security.

## Features

- **Read and navigate:** zoom PDF pages, browse thumbnails and bookmarks, and search selectable text.
- **Fill and annotate:** place text, checkmarks, and crosses on pages, then move, resize, or edit them. Saved annotations can be edited again when reopened.
- **Add handwritten signatures:** import signature images and keep a personal library for reuse.
- **Digitally sign and verify:** sign with a certificate (`.p12` or `.pfx`) and inspect existing PDF signatures.
- **Manage document information and passwords:** edit metadata and add, remove, or change password protection.
- **Print:** print all pages, a range, or the current page, including unsaved annotations.
- **Choose your appearance:** use the system theme, light mode, or dark mode without changing the PDF's colors.

Open a PDF, choose a tool from the sidebar, and drag the added content into position. Use **Save as PDF** to save to a different file. For detailed instructions, see the [usage guide](docs/guide.md#using-the-app).

### Important notes about signatures

A handwritten signature image is **not a cryptographic digital signature**. Signature images stored in a PDF can be extracted by anyone with access to it, even after the visible signatures are deleted.

Digital signatures use certificates. A valid integrity check does not necessarily mean the signer is trusted. Finish editing before signing: later changes may invalidate signatures. Imported certificates contain private keys; use password-protected files. See [signing and certificate storage](docs/guide.md#cryptographic-signing) for security details and verification limitations.

## Platforms and package formats

The project provides build and packaging options for:

| Platform | Formats |
| --- | --- |
| Linux | Executable, AppImage, Debian/Ubuntu `.deb`, and `.rpm` packages |
| Windows | Executable (`.exe`) and installer (`.exe`) |
| macOS (Apple Silicon) | Application bundle (`.app`), built natively on macOS |

These are supported build formats, not a promise of prebuilt downloads for every platform. Linux executables still require compatible system libraries; AppImage compatibility also depends on the host distribution. Linux physical printing requires CUPS. Windows installers let you choose jPDF Desk as your default PDF app without changing your existing default automatically.

Build requirements and packaging instructions are in the [technical guide](docs/guide.md#build), including [distribution variants](docs/guide.md#distribution-variants).

## Licensing

jPDF Desk's own source code is licensed under the [MIT License](LICENSE).

Bundled libraries have their own licenses. In particular, **MuPDF is available under AGPL or commercial terms**; the project's MIT license does not override those requirements. Qt and other dependencies also have license obligations. Review the applicable licenses before redistributing the app, especially statically linked binaries.

## Documentation

- [Detailed usage, printing, and signing](docs/guide.md#using-the-app)
- [Building from source](docs/guide.md#build)
- [Packaging and platform builds](docs/guide.md#distribution-variants)
- [Architecture and tests](docs/guide.md#architecture-and-tests)
- [GUI architecture](docs/gui.md)
- [Library APIs and signing-provider contracts](docs/libraries.md)
- [CI, build containers, and ARM64 cross-builds](docs/ci.md)
