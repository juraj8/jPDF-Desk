# PDF libraries

[Documentation index](../README.md) · [GUI architecture](gui.md)

In-tree libraries live under `lib/`, with public headers in `include/jpdf_desk/`.
They do not depend on application widgets or styles and are not an installed SDK.

| Target | Purpose |
| --- | --- |
| `JPDFDesk::Document` | Open, render, search, edit, and save PDFs |
| `JPDFDesk::Signing` | Embed and verify digital signatures |
| `JPDFDesk::OpenSSL` | PKCS#12 signing and OpenSSL verification provider |
| `JPDFDesk::Printing` | Page layout and snapshot print jobs |
| `JPDFDesk::Image` | Non-widget source-image loading |

## Document contracts

Link `JPDFDesk::Document` and include `jpdf_desk/document/pdf_document.h`.

- Page indices are zero-based; annotation coordinates use 1.5 scene pixels per PDF point.
- Missing annotation map entries preserve existing items; empty entries delete
  application-owned items of that type. Unrelated annotations remain intact.
- Save to a different path. Saving validates and atomically publishes an
  independent snapshot, then adopts the output; failures preserve the destination.
- `snapshot()` includes current metadata and supplied edits, but does not sign,
  publish, or copy providers. Pending password changes apply on save.
- `PdfDocument` owns its MuPDF context and is non-copyable. Use each context on
  one thread at a time. Errors are C++ exceptions.
- `setFormValues()` validates and applies changes atomically. Refresh field IDs
  after opening or saving. No PDF JavaScript is executed.
- Search matches selectable text, not OCR or scene drafts. `savedSource()` exposes
  in-memory credentials for independent readers; never persist or log them.

## MuPDF error boundaries

MuPDF uses `setjmp`/`longjmp`. Use `mupdf::call` for small C-only operations and
wrap acquired handles with `mupdf::own` outside the boundary. Do not create
non-trivial C++ locals inside it. Owners must die before their context.
For direct `fz_try`, protect modified cleanup pointers with `fz_var`, clean up
in `fz_always`, and translate errors after `fz_catch`. Never let C++ exceptions
escape through MuPDF callbacks.

## Signing providers

The application selects providers in `gui/main.cpp`; document/signing libraries
do not choose a backend. Signing and verification are independent capabilities.
Providers return owned MuPDF PKCS#7 handles and must obey its callback/error
conventions. Do not persist passwords or silently switch trust policies.
Native certificate-store and token providers are not implemented.
Verification reads the saved file; integrity and certificate trust are separate.
See [signing limitations](../usage/guide.md#digital-signatures).

## Images and printing

`loadSourceImage()` returns an owned `QImage` or a null image with an error.
Keep the static Qt JPEG plugin unimported: its libjpeg ABI conflicts with MuPDF's.

`printDocumentSnapshot()` includes in-memory changes and supplied annotation
edits without modifying the source. Callers supply drafts; dialogs stay outside
the library. Qt provides OS printer integration. See [printing](../usage/guide.md#print).
