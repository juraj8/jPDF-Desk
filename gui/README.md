# GUI architecture

## Boundaries

- `main.cpp` is the composition root: application setup and optional signing
  provider selection belong here.
- `MainWindow` coordinates document operations and presents operation errors.
  It owns the document and asset stores, and connects the UI to them.
- `SignatureStore` owns application asset persistence; `SignatureManager` presents
  it. PDF signature embedding and verification belong to the signing library,
  not this store.
- `ui/` contains widgets, dialogs, graphics items, and presentation helpers.
  Document, signing, and printing libraries must not depend on these classes.

## Reusable presentation

`MetadataDialog` accepts metadata values and returns only changed editable fields.
It neither reads nor writes a PDF. Read-only date fields are excluded from changes;
an explicitly cleared field is returned with an empty value.

`PasswordDialog` owns input validation and calls an injected application callback
only for a valid request. An empty password requests removal. Callback exceptions
are displayed inline without accepting the dialog. Cancellation does not call the
callback. The callback is retained only for the lifetime of the dialog; callers
must keep anything it references alive for that lifetime.

`dialog_helpers` provides wrapped plain-text hints and standard button boxes with
rejection wired up. Acceptance is intentionally left to each caller because some
forms must validate or apply changes before closing.

`SearchControls` presents the top-bar query, result count, and previous/next
controls. `MainWindow` requests value-based matches from `PdfDocument`, navigates
the canvas, and creates transient highlights. Changing the query or rebuilding
the document clears results. Highlights never become saved annotations.
Search runs on Enter rather than on each keystroke; Ctrl+F focuses the field,
and F3/Shift+F3 navigate matches with wraparound. Large-document background search
would need its own document/context rather than sharing the UI's MuPDF context.

`buttons` provides accessible, consistently sized navigation controls.
`page_annotations` is the conversion boundary between graphics items and the
library's annotation values. Serialization should stay there, not in toolbar code.

## Styling

- `theme` owns light/dark color tokens, palettes, and appearance preferences.
- `window_style` owns the application stylesheet. Widgets should not embed local
  stylesheets or choose theme colors independently.
- Semantic properties (`primary`, `subtle`, `toolTile`, `danger`) express reusable
  visual roles. Object names identify specific widgets and support tests; prefer
  properties for styles that should apply to more than one component.
- Set visual properties before showing widgets. If a property changes at runtime,
  repolish the widget (as the sidebar signature preview does).
- PDF paper, image pixels, annotation ink, and annotation selection controls are
  deliberately independent of application light/dark mode.

## Remaining seams

`MainWindow` still owns page scene construction, visible-page raster caching,
navigation, and annotation-selection wiring. The next substantial extraction
should move those together into a document-canvas widget, exposing navigation and
annotation snapshots rather than its internal graphics items. Preserve the
existing unsaved-edit and theme-switch integration tests during that work.

`PrintDialog` also remains large. Its settings sections can be extracted into
widgets when they have clear value-based input/output APIs; avoid introducing a
parallel copy of printer state or abstractions for one-off layout statements.

The GUI is currently one in-tree CMake target, not an installed widget SDK. Keep
value-based dialogs usable without `MainWindow` or a PDF fixture. Standalone
coverage is in `test/test_document_dialogs.cpp`; window-level flows remain covered
by the existing integration tests.
