# Using jPDF Desk

[Documentation index](../README.md) · [Installation](installation.md)

## Open, annotate, and save

1. Choose **Open** (Ctrl+O; Cmd+O on macOS).
2. Choose text, a checkmark, a cross, or a signature in **Tools**.
3. Drag items into position. Double-click text to edit; select items to resize or delete.
4. Choose **Save as PDF** (Ctrl+S; Cmd+S on macOS).

Save to a different path. The saved output becomes the open document.
Closing or opening another PDF with unsaved edits offers **Save**, **Discard**, or
**Cancel**. Cancelling or failing to save keeps the current document and edits.
There is no automatic crash recovery.
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

Signature images are saved as page content, not comments or stamp annotations.
Position and resize them before saving. After reopening in jPDF Desk, they can
be selected and removed, but not moved or resized. Save to apply removal to the
PDF. Other PDF editors can still alter or extract the images. Removing an image
covered by a digital signature invalidates that signature.

Only images saved with jPDF Desk's removal tags support this workflow; older
stamp-based or untagged images are not converted.

**An image is not a digital signature.** Placed images can be extracted even
after visible signatures are deleted. Reusable signature templates stay in the
local image library; saving does not embed the selected template in PDF metadata.
Legacy embedded templates are removed from metadata on export. This is not a
secure-redaction feature for images already placed in page content.

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
Extremely large page rasters are rejected to avoid excessive memory allocation;
choose a lower printer resolution if needed.
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
Timestamp creation, hardware tokens, and long-term validation are unsupported.

### Check signatures

Choose **Check signatures…** for an overall result and separate integrity,
certificate-chain, certificate-date, signing-time, revocation, and document-coverage
checks. Expand **Details** for diagnostics. Verification reads the saved PDF.
Intact signed bytes do not establish trust or cover later unsigned revisions.

The overall result distinguishes altered signed bytes (**Invalid**), a certificate
rejected by the provider's trust policy (**Not trusted**), and incomplete evidence
(**Validity not established**). Green individual checks do not mean full signature
validity. Full historical validity is not established by this build because archived
revocation and historical chain evidence are not verified. Certificate expiry today does not automatically
invalidate an older signature; a claimed signing date within its validity period
is not proof of historical validity.

The OpenSSL provider verifies embedded RFC 3161 signature timestamp tokens offline:
the imprint must match the signature value, and the token signature and TSA chain
must validate. Timestamp hashes weaker than SHA-256 are not accepted. Timestamp
trust uses system/OpenSSL CA roots. Linux uses OpenSSL's configured system CA
paths (including `SSL_CERT_FILE` and `SSL_CERT_DIR`); Windows additionally imports
its ROOT store and macOS imports native trust anchors. Importing roots is not the
OS/viewer's full policy evaluator: native distrust constraints, application-specific
trust rules, and cached intermediate certificates are not replicated. The
TSA chain is checked at current time, not with historical revocation evidence;
expired TSA certificates may therefore leave a timestamp unverified. A verified
timestamp proves the signature existed by the TSA-reported time, not its exact
creation time. The certificate period at that time is shown separately.

Signer-chain validation accepts a chain validated by either system/OpenSSL CA
roots or MuPDF's built-in store. **Details** reports both checks and exact system
chain failures, including error depth and code. System chain checks use current
time and the S/MIME-signing certificate purpose, matching MuPDF's existing purpose.

Revocation is unchecked by default. Choose **Check revocation online…** in the
signature report and confirm the privacy notice to contact issuer/TSA services.
Requests can disclose certificate identifiers and that the document was checked.
Checks run with progress and cancellation; cancellation leaves the previous report
unchanged. Validated system/OpenSSL signer and TSA paths are checked for each
non-root certificate using OCSP with CRL fallback. Missing/unvalidated paths are
reported as skipped, and trusted anchors are not queried. Private endpoints,
redirects, proxies, and oversized responses are blocked by the default transport.

The summary distinguishes current **Good**, **Revoked**, **Unknown**, and **Failed**
evidence, with per-certificate sources, update dates, revocation reasons, and
invalidity dates in **Details**. Current Good evidence does not establish historical
validity; a later revocation does not automatically invalidate an older signature.
Key/CA compromise needs particular care. Online checks remain opt-in, and archive/
historical revocation validation is unsupported.

Missing, malformed, mismatched, and unverifiable timestamp
tokens are reported separately. Standalone PDF document timestamps (`ETSI.RFC3161`)
are not supported. Validate in the recipient's signature-aware viewer too.

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
