# Revocation library

The optional `JPDFDesk::Revocation` module validates OCSP and CRL evidence without
PDF or GUI dependencies. It is built when `JPDF_DESK_WITH_OPENSSL` is enabled.
`JPDFDesk::RevocationHttp` supplies a Qt Network HTTP/HTTPS transport when that
Qt module is available. No new third-party dependency is required.

## Use

Run synchronous checks on a worker thread with a Qt event dispatcher/application,
not the GUI thread. Each checker is single-threaded; keep it alive to reuse its cache.

```cpp
#include <jpdf_desk/revocation/http_transport.h>

using namespace jpdf::revocation;
Checker checker(makeHttpTransport());
const jpdf::crypto::ValidationTime time; // Capture once; reuse for every certificate in this operation.
Result result = checker.check(certificateDer, issuerDer, time);
```

The caller must first validate the issuer's chain against its chosen trust policy.
Check each non-root certificate in that chain separately, including TSA certificates
when assessing timestamps. `Good` means authenticated, fresh revocation evidence
for this certificate—not a trusted chain or a valid document signature.

Online access is explicitly opt-in. The signature dialog offers **Check revocation
online…** with a privacy confirmation; opening/checking a PDF remains offline.
The provider exports validated system/OpenSSL signer and TSA paths, leaf first and
anchor last. A joined worker checks every non-root certificate, reports certificate
progress, and commits results only after completion. Missing/unexportable paths
(including built-in-only trust) are skipped with a diagnostic, never queried using
an unvalidated issuer. Trusted-anchor-only paths are explicitly marked not applicable.
Cancellation preserves the prior report, including when a request is in flight.

The GUI never upgrades current `Good` results to historical/full signature validity.
Revoked results show revocation/invalidity dates and reasons without automatically
invalidating every older signature; TSA revocation participates in the same overall
decision. Unknown, failed, and skipped checks keep the result incomplete.

## Decisions and evidence

- OCSP is attempted first; authenticated `Good` or `Revoked` ends the check.
- Unknown, failed, or unavailable OCSP falls back to complete, direct CRLs.
- `Unknown` means an authenticated responder could not determine status.
- Network, parse, signature, authorization, freshness, or policy failures produce
  `CheckFailed`, never `Good` or `Revoked`. No advertised OCSP or supported direct-CRL
  location gives `NotChecked`; advertised but rejected/unsafe URLs give `CheckFailed`.
- A system time at/before the application's embedded/build bounds gives
  `CheckFailed` with `clockError == true`, before fetching or accepting cached evidence.
  This is an environmental failure, not evidence that a certificate is revoked.
- The issuer must cryptographically match the certificate. OCSP response signatures,
  delegated-responder authorization/EKU, certificate ID, and signed times are checked.
  A delegated responder must carry a valid `id-pkix-ocsp-nocheck` extension; otherwise
  separate responder-revocation validation is needed and this module fails closed,
  allowing CRL fallback. Duplicate statuses and unsupported critical extensions fail.
- CRLs require issuer matching, CRL-signing usage, valid signatures and fresh dates.
  Delta, scoped/indirect CRLs, unsupported critical extensions, and `removeFromCRL`
  processing are rejected rather than interpreted as an empty revocation list.
- Supported signature algorithms must map to SHA-256 or a larger digest in OpenSSL.
  Unsupported/weak algorithms fail closed. OCSP certificate IDs use standard SHA-1;
  this is an identifier hash, not the response-signature algorithm.
- OCSP requests omit nonces to permit pre-produced/cached responses. Signed freshness
  checks remain mandatory. Missing OCSP `nextUpdate` is bounded by maximum evidence age;
  CRLs require `nextUpdate`.
- Revocation time, reason, and CRL invalidity date are returned when available.
  This module does not decide whether a historical signature survived later revocation.

`verifyOcsp` and `verifyCrl` validate supplied evidence offline. Their optional time
parameter evaluates freshness at that instant; it does **not** prove that the evidence
existed then, establish historical revocation status, or implement PDF long-term
validation. Historical decisions also require trusted time/archive evidence and a
validation policy, especially for key compromise. A supplied `QDateTime` is converted
into a `ValidationTime` with a separately captured real system time for the clock
sanity check. Default operation times are captured once, not per backend/cache check.

## Transport and resource policy

Default limits: 5 seconds per request, 15 seconds per overall check, 2 MiB per response,
5 minutes of clock skew, and 24 hours maximum evidence age. Adjust `Policy` explicitly
if an issuer publishes less frequent CRLs.

Only cryptographically validated evidence is cached, for up to 5 minutes and while
fresh. The cache is bounded to 16 entries/8 MiB. `clearCache()` forces retrieval again.

The HTTP transport permits HTTP/HTTPS on their standard ports only. It rejects
credentials, fragments, redirects, proxy use, compressed bodies, and private/local,
reserved, documentation, or IPv4-mapped private destinations. DNS answers must all be
public. Connections are pinned to a checked address; HTTP Host and HTTPS peer/SNI
identity retain the original hostname. TLS verification is never bypassed. HTTP is
allowed because OCSP/CRL content is independently signed; an intermediary can still
cause denial of service. No authentication, redirect handling, AIA issuer downloading,
or system-store mutation is performed.

Certificate-controlled URLs can disclose that a document/certificate was checked.
An application should obtain consent before using this transport. Enterprise
private responders/proxies intentionally require a separately reviewed transport.
`HttpTransportOptions` supports explicit, exact private-address exceptions and extra
ports for reviewed responders/direct transport use or isolated integration tests.
The GUI supplies no exceptions, and the checker keeps its standard-port policy.
Redirects remain disabled even with exceptions. Cancellation is checked before work,
between certificates, and by 25 ms polling during DNS and HTTP requests. Policy's
`cancelled` callback must be thread-safe; custom transports receive the same callback.
Crypto operations finish before cancellation is observed again.

Custom `Transport` implementations must enforce their own destination policy,
timeouts and response bounds. Tests inject fake transports and use an explicitly
allowlisted loopback server for redirects, timeouts, compressed/oversized responses,
and cancellation. No tests contact the internet or change host trust stores.
