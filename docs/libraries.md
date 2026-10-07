# jPDF Desk reusable PDF libraries

[Back to the overview](../README.md)

Library modules live under `lib/`; source paths below are relative to the repository root.

These are in-tree CMake libraries with public headers under each module's
`include/jpdf_desk/` directory. They do not depend on `MainWindow`, application
resources, dialogs, or application styles. They are not yet an installed SDK.

## Dependencies

| Target | Responsibility | Dependencies |
| --- | --- | --- |
| `JPDFDesk::Document` | Open/authenticate, render, search, edit/save annotations and metadata, manage passwords, inspect outlines; orchestrate optional signing | QtGui, MuPDF, signing integration |
| `JPDFDesk::Signing` | Embed signatures and verify saved PDF fields | QtCore, MuPDF, provider contracts |
| `JPDFDesk::OpenSSL` | PKCS#12 private keys and MuPDF/OpenSSL verification | QtCore, MuPDF's PKCS#7 adapter, OpenSSL Crypto |
| `JPDFDesk::Printing` | Page selection/layout and snapshot print jobs | Document, Qt PrintSupport |

`jpdf-desk-document-types` carries shared values. `jpdf-desk-signature-services`
carries provider contracts. Neither interface target links a crypto backend.
The document and signing libraries never choose a platform crypto implementation.
The application selects providers in `gui/main.cpp`.

## Using the document library

```cmake
target_link_libraries(my-tool PRIVATE JPDFDesk::Document)
```

```cpp
#include <jpdf_desk/document/pdf_document.h>

PdfDocument document; // No cryptography dependency or widgets required.
document.open(inputPath);
DocumentAnnotations edits;
edits.fields[0] = {{{50, 80, 250, 50}, QStringLiteral("Example"), 12}};
document.saveSnapshot(outputPath, edits);
```

Annotations use page-local scene coordinates, at 1.5 scene pixels per PDF point.
Absent map entries preserve existing annotations of that type on that page;
empty entries delete that type of application-owned annotation. Unrelated PDF
annotations are retained. Use a different output path from the open source.
After saving successfully, the output becomes the open document.

`PdfDocument` owns its MuPDF context and is non-copyable. Use a document/context
on one thread at a time. Providers must not retain a context beyond its lifetime.
Library errors are C++ exceptions; the application decides how to report them.

## Metadata and encryption

`open(path, password)` throws `PdfPasswordRequired` when authentication fails;
a failed open preserves the current document. `metadata()` returns PDF Info
properties and `setMetadata(values)` updates editable fields while preserving
dates and private application metadata. `setPassword(password)` applies to the
next saved copy; an empty string removes encryption. Opening another document
clears pending password changes. See the [usage guide](guide.md#metadata-and-password-protection)
for encryption and password constraints.

## Text search

`PdfDocument::search(query)` performs case-insensitive selectable-text search
across all pages using MuPDF's text matching (including phrases spanning lines).
Results are ordered by page and contain one or more page-local scene rectangles
per match, using the same 1.5x coordinates as annotations. Empty/whitespace queries
return no results. Search does not modify the document or require widgets.
Scanned images are not OCR'd, and unsaved scene annotation edits are not searched.

## Selecting cryptography and trust policy

```cmake
target_link_libraries(my-tool PRIVATE JPDFDesk::Document JPDFDesk::OpenSSL)
```

```cpp
#include <jpdf_desk/document/pdf_document.h>
#include <jpdf_desk/signing/openssl_provider.h>

PdfDocument document(openSslSignatureServices());
document.open(inputPath);
document.saveSnapshot(outputPath, edits, {}, {pfxPath, password});
const auto results = document.checkDigitalSignatures();
```

`SigningProvider` accesses a private key through an opaque `SigningIdentity`.
OpenSSL interprets its identity string as a PKCS#12 path. Other providers can
interpret it as a certificate-store identifier or token identity.
`VerificationProvider` independently supplies integrity/certificate verification
and describes its trust policy. Applications may mix signing and verification
providers, or configure only one capability. Verification reads the saved file,
not pending edits. Integrity and certificate trust remain separate results.

Providers return owned MuPDF PKCS#7 handles. This deliberately adapts at MuPDF's
existing crypto boundary rather than inventing another CMS/signature engine:

- Factory methods translate errors to C++ exceptions; they must not allow a
  MuPDF longjmp to escape. The integration invokes factories outside `fz_try`.
- Returned handles must implement MuPDF's signer/verifier callbacks and support
  `pdf_drop_signer`/`pdf_drop_verifier`. Their callbacks follow MuPDF's C error
  convention; they must not throw C++ exceptions through MuPDF's `fz_try`.
- A successful factory transfers one owned reference to the integration.
  Handle cleanup occurs on both success and MuPDF failure paths. Null handles
  are treated as errors. Providers are kept alive by shared ownership in the
  document while the operation runs.
- Providers must not persist passwords. The OpenSSL adapter clears its temporary
  UTF-8 password buffer; this is not a guarantee that all password memory is erased.
- PDF embedding and atomic publication stay in `JPDFDesk::Signing`. Failed
  signing does not replace the output or change the open document path.

For Windows certificate-store/CNG support, add a separate provider implementation
and target linked to the Windows libraries it needs. Implement MuPDF's PKCS#7
signer/verifier callbacks with native certificate/key handles, and select the
providers in the application composition root. Do the same for macOS Keychain
or a hardware-token backend. Keep platform headers and compile definitions in
those implementation targets. Native identity selection needs its own GUI flow;
the current application intentionally continues to offer a `.p12/.pfx` file dialog.
Do not silently change verification trust stores when switching operating systems.

Native-store and token providers are not implemented. See [builds without OpenSSL](guide.md#builds-without-openssl)
for configuration and application behavior; document/signing libraries and their
provider-contract tests do not require the portable adapter.

## Printing

```cmake
target_link_libraries(my-tool PRIVATE JPDFDesk::Printing)
```

```cpp
#include <jpdf_desk/printing/pdf_printing.h>
#include <QPrinter>

QPrinter printer(QPrinter::HighResolution);
// Configure the printer here, or present QPrintDialog in the application.
printDocumentSnapshot(document, edits, printer, currentPage);
```

Qt supplies OS integration (CUPS on Linux, the Windows spooler on Windows, and
native print support on macOS). Static applications must include the appropriate
Qt platform plugins; this application supplies `jpdf-desk-qt-platform`, including
CUPS where available. No separate native printer wrapper is necessary today.

The print library owns snapshot creation, page selection/order/copies, sheet
layout/scaling, grayscale conversion, bounded raster resolution, and job failure
cleanup. Optional `PrintOptions` controls pages per sheet, ordering, odd/even
subsets, fit/actual-size scaling, and scale percentage. Snapshot printing
reads the saved source plus supplied edits; it does not change the source path,
annotations, or digital signatures. The caller must supply its unsaved edits.
`printDocument` prints an already prepared document without creating a snapshot.
Dialogs and error presentation stay outside this library. The app uses its own
`PrintDialog`, not `QPrintDialog`. See [printing](guide.md#printing) for available
controls and the password-protected snapshot limitation.
