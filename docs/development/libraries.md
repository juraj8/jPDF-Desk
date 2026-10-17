# PDF libraries

[Documentation index](../README.md) · [GUI architecture](gui.md)

In-tree libraries live under `lib/`, with public headers in `include/jpdf_desk/`.
They do not depend on application widgets or styles and are not an installed SDK.

| Target | Purpose |
| --- | --- |
| `JPDFDesk::Document` | Open, render, search, edit, and save PDFs |
| `JPDFDesk::Signing` | Embed and verify digital signatures |
| `JPDFDesk::OpenSSL` | PKCS#12 signing and OpenSSL verification provider |
| `JPDFDesk::Revocation` | OCSP/CRL validation and opt-in retrieval orchestration |
| `JPDFDesk::RevocationHttp` | Bounded, public-destination HTTP/HTTPS transport |
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
See [signing limitations](../usage/guide.md#digital-signatures) and the
[independent revocation module](revocation.md).

## Signing and revocation boundaries

These are related security capabilities, not interchangeable validators:

- `lib/signing/src/pdf_signatures.cpp` owns PDF field traversal and atomic signed
  output publication. It uses provider contracts, not OpenSSL directly.
- The OpenSSL provider owns key access, signer-chain trust and embedded RFC 3161
  timestamp verification. System root loading and validated-path export remain
  private to that backend.
- `lib/revocation/` validates OCSP/CRL evidence against a caller-supplied issuer.
  It does **not** establish root trust, verify PDF integrity, or access signing keys.
  HTTP is a separate optional target; offline validators require no network.
- `lib/crypto/` supplies internal OpenSSL date/diagnostic helpers to both
  implementations and the shared clock sanity check. `crypto/validation_time.h`
  exposes the operation time used by signing/revocation contracts; backend-only
  helpers remain under `detail/`.

Keep the signing and revocation targets separate: merging them would couple PDF
and key handling to evidence retrieval without removing those distinct policies.
If a future non-GUI client needs a single entry point, add a thin orchestration
layer over these targets rather than combining their implementations. Online
revocation must remain explicit and must not be inferred from offline trust checks.

## Validation clock bounds

`lib/crypto/generate_validation_clock.cmake` defines a fixed source timestamp in
UTC and captures a second timestamp at each build invocation. The build fails
unless its timestamp is strictly later than the fixed timestamp. CMake honors
`SOURCE_DATE_EPOCH`; reproducible builds must choose an epoch later than the fixed
bound. Changing the bound is a deliberate source change, not a user setting.

Before certificate/timestamp validation and OCSP/CRL checking (including cached
evidence), the current system UTC time must be strictly later than **both**
captured timestamps. Otherwise those checks are skipped/failed with clock
diagnostics; no certificate path is accepted and no revocation fetch is started.
PDF signed-byte integrity and document coverage are checked independently, even
with a bad clock. Neither cryptographic signing nor integrity checking imposes a
clock/trust decision. Provider factories must make the cryptographic verifier
available without requiring a valid clock; trust is gated separately.
`jpdf::crypto::ValidationTime` captures system UTC once per operation and carries
it through PDF fields, OpenSSL signer/TSA paths, certificate dates, evidence
freshness, cache age and the online signer/TSA chain loops. The dialog displays the
captured offline/online check times and does not silently re-evaluate dates when
opened or repopulated. OpenSSL verification parameters are set explicitly, rather
than letting each chain check independently read the wall clock.

For offline historical evidence evaluation, passing a `QDateTime` to revocation
validators still captures the real system time independently; that evaluation
date cannot bypass the clock gate. `ValidationTime::atSystemTime()` provides
explicit clock injection for tests/embedding callers, not a GUI preference or a
persisted override. No tests change the OS clock.

MuPDF's built-in chain checker remains an independent, backend-controlled policy
and exposes no clock-injection API. Its implementation is not patched. Legacy
metadata providers can keep their original overload; time-aware providers use the
new operation-time overload. Thus snapshot consistency covers application-owned
checks, not opaque checks inside third-party/native verifiers.

PDF results carry a separate `clockError` diagnostic; trust and timestamp states
remain `NotChecked`, without hiding intact/altered signed bytes. Metadata/chain
callbacks (including MuPDF's built-in policy) are not invoked when the clock is
unacceptable. A metadata failure also cannot erase an already-established digest
result. Revocation results carry a `clockError` flag to distinguish environmental
failures from revocation, unknown status or other backend errors.

The report offers **Recheck saved PDF** after correcting the clock. It captures a
new time, replaces the offline report, clears previous online evidence and updates
online-check eligibility. Rechecking never contacts issuer services; online access
still needs a separate confirmation. Failed rechecks preserve the previous report.
`ClockValidationError` remains available to explicit clock-requiring callers;
provider-factory clock failures are shown as a warning rather than signature
invalidity.

Integration tests cover intact and tampered real signed PDFs under a bad clock,
skipped metadata/chain callbacks, rejected cached evidence, zero network requests,
captured dates, recheck failure/recovery and cleared online results. The shared
contracts and UI tests also run with `JPDF_DESK_WITH_OPENSSL=OFF`; in that build the
application has no default crypto provider, but injected providers remain usable.

These bounds detect a clock that is too early; they do not authenticate time,
detect a clock that is too far ahead, or establish historical validity. They are
not compared to the PDF's signing date or an RFC 3161 token's time: legitimate
signatures and evidence may predate this application's build.

## Malformed signature handling

The OpenSSL metadata provider requires a signed PKCS#7 envelope with exactly one
signer info and a matching signer certificate. Empty/truncated DER, unsupported
content types, duplicate/missing signers and nonzero trailing data fail closed.
Zero padding reserved by PDF `Contents` remains supported. A metadata `error`
prevents fallback to another chain policy and clears timestamp trust/path exports;
an independently computed signed-byte digest result is retained.

Embedded RFC 3161 attributes with the wrong ASN.1 type, duplicate attributes,
invalid tokens, mismatched imprints, damaged signatures or multiple TSA signers
cannot become verified time evidence. Integration tests isolate their trust
stores within the test process, without modifying host roots or using the network.

PDF verification reports unreadable/recursive/depth-limited field branches as
separate diagnostic entries and continues with siblings. Invalid `Kids` arrays
and non-dictionary field entries are not silently treated as unsigned fields.
Backend digest/certificate exceptions remain failed/not established, never an
acceptance; per-field metadata failures do not hide other signatures. Tests cover
malformed fields preceding real intact/trusted signatures and context/handle
recovery after both MuPDF and C++ backend errors.

## Images and printing

`loadSourceImage()` returns an owned `QImage` or a null image with an error.
Keep the static Qt JPEG plugin unimported: its libjpeg ABI conflicts with MuPDF's.

`printDocumentSnapshot()` includes in-memory changes and supplied annotation
edits without modifying the source. Callers supply drafts; dialogs stay outside
the library. Qt provides OS printer integration. See [printing](../usage/guide.md#print).
