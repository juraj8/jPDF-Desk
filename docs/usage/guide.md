# Using jPDF Desk

[Documentation index](../README.md) · [Installation](installation.md)

## Open, annotate, and save

1. Choose **Open** (Ctrl+O; Cmd+O on macOS).
2. Choose text, a checkmark, a cross, or a signature in **Tools**.
3. Drag items into position. Double-click text to edit; select items to resize or delete.
4. Choose **Save as PDF** (Ctrl+S; Cmd+S on macOS).

Save to a different path. The saved output becomes the open document.
Save before closing or opening another PDF: there is no unsaved-change prompt or recovery.
Annotations overlay the page rather than replacing original text.

You can also launch with `jpdf-desk "/path/to/document.pdf"`.
Cancelling a background open preserves the previous document and edits;
cancellation may wait for the current page operation.

## Fill native PDF forms

Choose **Fill form fields…** on the current page, edit the fields, and choose
**OK**, then **Save as PDF**. Text fields, checkboxes, dropdowns, and
single-selection lists are supported; fields remain interactive after saving.
For flat or scanned forms, use annotation tools instead.
Radio buttons, multi-select lists, XFA, JavaScript, and submit actions are unsupported.
Editing a signed PDF may invalidate its signatures.

## Navigate and search

Use **Outline**, **Thumbnails**, or **Previous/Next** to change pages.
Use **+ / −** or Ctrl+mouse wheel to zoom.

### Text search

Press Ctrl+F (Cmd+F on macOS), enter text, and press Enter.
Use F3 / Shift+F3 or the match buttons to navigate results.
Search is case-insensitive and reads selectable text in the saved file, not
scanned images or unsaved edits. Highlights are temporary. **Cancel** discards
partial results without changing edits.

## Handwritten signatures

Choose **Manage signature images…**, import dark ink on light paper, and choose
**Use selected**. Click the **Signature** tool to place it.

**An image is not a digital signature.** Embedded images can be extracted even
after visible signatures are deleted. The processed image is also embedded as
a reusable template when saving.

## Metadata and passwords

Choose **More… → File metadata…** to edit properties; dates are read-only.
Choose **More… → PDF password…** to set, replace, or remove protection, then save
to a different file. Existing protection is retained unless changed.
New protection uses AES-256; passwords must fit within 127 UTF-8 bytes and contain
no null characters. Changing encryption may invalidate signatures.

## Print

Choose **Print…** (Ctrl+P; Cmd+P on macOS), select a printer or **Print to File (PDF)**,
choose pages and layout, then use **Preview** before printing.
Ranges use syntax such as `1-3, 5, 8`. Unsupported printer settings are disabled.

Printing includes unsaved edits and rasterizes pages at up to 300 DPI.
**PDF print output does not retain password protection or digital signatures.**
Use **Save as PDF** to preserve configured password protection.
Linux physical printing requires CUPS and an app built with its backend;
Snap users should check [interface connections](installation.md#snap).

## Digital signatures

Digital signing requires a cryptography-enabled build; Windows presets disable it.
Handwritten images work independently.

1. Choose **Manage certificates…** and import a password-protected `.p12`/`.pfx`
   containing your private key.
2. Select it, choose **Sign PDF…**, and confirm the identity.
3. Enter its password and choose a different output PDF.

Signing includes current edits in an invisible digital signature.
Passwords are not saved. **Finish editing before signing:** later saves may
invalidate signatures, and re-signing does not preserve earlier signatures.
Timestamping, hardware tokens, and long-term validation are unsupported.

### Check signatures

Choose **Check signatures… → Details** for integrity, certificate checks,
later revisions, and unsigned fields. Verification reads the saved PDF.
Intact signed bytes do not establish trust or cover later unsigned revisions.
The default provider uses MuPDF's certificate store, not the OS trust store,
and does not check online revocation or timestamps. Validate in the recipient's
signature-aware viewer too.

## Stored images and certificates

Managers show their storage location in the per-user `jPDF-Desk` data folder.
Each asset is limited to 5 MB. Imports/removals take effect immediately;
removing a stored copy does not change the original file or existing PDFs.

**Stored certificates contain private keys.** Use password-protected PKCS#12
files: the app copies them without adding encryption or using an OS keychain.

### Upgrading from older releases

Reimport missing assets and reset preferences; old application identities are
not migrated. Older annotations may remain visible but not editable.

## Appearance and About

Choose **Appearance → System / Light / Dark**. The choice is remembered;
PDF content and unsaved edits are unchanged. **About…** shows version and license information.

## FileOpen-protected PDFs

FileOpen-protected PDFs cannot be opened or edited. Use **Choose external reader…**
to select a compatible reader (usually Adobe Reader with the FileOpen plug-in),
follow the provider's authorization instructions, or request an unprotected copy.
This does not bypass protection.

**Show Details…** displays local properties and unauthenticated structural hints,
not a validated PDF parse. Review paths and document identifiers before sharing.
