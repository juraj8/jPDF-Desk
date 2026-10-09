# GUI architecture

[Documentation index](../README.md) · [Build and test](build.md)

Sources live in `gui/`.

## Responsibilities

- `main.cpp`: application setup and signing-provider selection.
- `MainWindow`: document ownership, file operations, dialogs, and errors.
- `DocumentView`: page scene, navigation, raster caching, search highlights,
  and unsaved annotation edits. The document must outlive the view.
- `SignatureStore`: asset persistence; `SignatureManager`: asset presentation.
  Neither stores passwords or performs PDF signing.
- `ui/`: widgets, dialogs, graphics items, and presentation helpers.
  Libraries under `lib/` must not depend on them.

Keep dialogs value-based, with changes applied through injected callbacks.
Keep annotation conversion in `page_annotations` and shared dialog primitives
in `dialog_helpers`. Saving and printing use `captureDrafts()`; empty page
entries preserve deletions.

## Background work

`runPdfTask` runs exclusive work on a joined thread. Progress and cancellation
use atomics; errors return to the caller. Never share a live MuPDF context
between threads or create Qt pixmaps/graphics items off the GUI thread.

Loading adopts an independent document only after success; failure or cancellation
preserves the current document and drafts. Search uses an independent reader of
the saved file, excluding unsaved edits. Never persist or log its credentials.
Cancellation is cooperative between page operations.

## Styling and tests

`theme` owns colors and preferences; `window_style` owns the stylesheet.
Use semantic widget properties rather than local stylesheets. PDF content and
annotation colors stay independent of the application theme.

Keep dialog tests in `test/test_document_dialogs.cpp`, view tests in
`test/test_document_view.cpp`, worker tests in `test/test_pdf_tasks.cpp`, and
window-level flows in integration tests. See [running tests](build.md#run-tests).
