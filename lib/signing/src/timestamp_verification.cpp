#include "timestamp_verification.h"
#include "system_certificate_store.h"
#include "jpdf_desk/crypto/detail/openssl_support.h"

#include <QObject>
#include <QStringList>
#include <openssl/err.h>
#include <openssl/ts.h>
#include <openssl/x509v3.h>
#include <memory>

namespace {
SignatureTimestamp failure(SignatureTimestamp::Status status, const QString &description)
{
    ERR_clear_error();
    return {status, {}, description};
}
}

SignatureTimestamp verifySignatureTimestamp(PKCS7 *signature,
                                             const jpdf::crypto::ValidationTime &time)
{
    using Status = SignatureTimestamp::Status;
    ERR_clear_error();
    const auto clockError = time.clockError();
    if (!clockError.isEmpty()) return failure(Status::Failed, clockError);
    const auto *signers = PKCS7_get_signer_info(signature);
    if (!signers || sk_PKCS7_SIGNER_INFO_num(signers) != 1)
        return failure(Status::Failed, QObject::tr("Timestamp verification requires exactly one PKCS#7 signer."));
    auto *signer = sk_PKCS7_SIGNER_INFO_value(signers, 0);
    int index = X509at_get_attr_by_NID(signer->unauth_attr, NID_id_smime_aa_timeStampToken, -1);
    if (index < 0) return {Status::Absent, {}, {}};
    if (X509at_get_attr_by_NID(signer->unauth_attr, NID_id_smime_aa_timeStampToken, index) >= 0)
        return failure(Status::Failed, QObject::tr("Multiple signature timestamp attributes are not supported."));
    auto *attribute = X509at_get_attr(signer->unauth_attr, index);
    auto *value = X509_ATTRIBUTE_count(attribute) == 1 ? X509_ATTRIBUTE_get0_type(attribute, 0) : nullptr;
    if (!value || value->type != V_ASN1_SEQUENCE)
        return failure(Status::Invalid, QObject::tr("The signature timestamp attribute is malformed."));
    const auto *data = ASN1_STRING_get0_data(value->value.sequence);
    const auto length = ASN1_STRING_length(value->value.sequence);
    const auto *end = data + length;
    std::unique_ptr<PKCS7, decltype(&PKCS7_free)> token(d2i_PKCS7(nullptr, &data, length), PKCS7_free);
    if (!token || data != end)
        return failure(Status::Invalid, QObject::tr("Cannot decode the RFC 3161 timestamp token."));
    std::unique_ptr<TS_TST_INFO, decltype(&TS_TST_INFO_free)> info(
        PKCS7_to_TS_TST_INFO(token.get()), TS_TST_INFO_free);
    if (!info || TS_TST_INFO_get_version(info.get()) != 1)
        return failure(Status::Invalid, QObject::tr("The timestamp token has invalid or unsupported TSTInfo."));
    auto *imprint = TS_TST_INFO_get_msg_imprint(info.get());
    const ASN1_OBJECT *algorithm = nullptr;
    if (!imprint)
        return failure(Status::Invalid, QObject::tr("The timestamp message imprint is missing."));
    X509_ALGOR_get0(&algorithm, nullptr, nullptr, TS_MSG_IMPRINT_get_algo(imprint));
    const auto *digest = algorithm ? EVP_get_digestbyobj(algorithm) : nullptr;
    // Do not treat collision-prone timestamp hashes as trustworthy evidence.
    if (!digest || EVP_MD_size(digest) < 32)
        return failure(Status::Failed, QObject::tr("The timestamp hash algorithm is unsupported or too weak."));
    unsigned char hash[EVP_MAX_MD_SIZE];
    unsigned int hashLength = 0;
    if (!signer->enc_digest || EVP_Digest(ASN1_STRING_get0_data(signer->enc_digest),
            ASN1_STRING_length(signer->enc_digest), hash, &hashLength, digest, nullptr) != 1)
        return failure(Status::Failed, QObject::tr("Cannot compute the signature's timestamp imprint."));
    auto *expected = TS_MSG_IMPRINT_get_msg(imprint);
    if (!expected || ASN1_STRING_length(expected) != static_cast<int>(hashLength)
        || CRYPTO_memcmp(hash, ASN1_STRING_get0_data(expected), hashLength) != 0)
        return failure(Status::Invalid, QObject::tr("The timestamp imprint does not match this signature."));
    const auto verifiedTime = jpdf::crypto::detail::asn1DateTime(TS_TST_INFO_get_time(info.get()));
    if (!verifiedTime.isValid())
        return failure(Status::Invalid, QObject::tr("The timestamp time is malformed."));
    if (verifiedTime > time.systemUtc())
        return failure(Status::Failed, QObject::tr("The timestamp time is invalid or in the future."));
    QString storeError;
    auto store = loadSystemCertificateStore(storeError);
    if (!store)
        return failure(Status::Failed, storeError);
    X509_VERIFY_PARAM_set_time(X509_STORE_get0_param(store.get()), time.systemUtc().toSecsSinceEpoch());
    const auto *tokenSigners = PKCS7_get_signer_info(token.get());
    if (!tokenSigners || sk_PKCS7_SIGNER_INFO_num(tokenSigners) != 1)
        return failure(Status::Failed, QObject::tr("The timestamp token must have exactly one TSA signer."));
    const auto *tsaSigner = sk_PKCS7_SIGNER_INFO_value(tokenSigners, 0);
    const ASN1_OBJECT *signatureAlgorithm = nullptr;
    X509_ALGOR_get0(&signatureAlgorithm, nullptr, nullptr, tsaSigner->digest_alg);
    const auto *signatureDigest = signatureAlgorithm ? EVP_get_digestbyobj(signatureAlgorithm) : nullptr;
    if (!signatureDigest || EVP_MD_size(signatureDigest) < 32)
        return failure(Status::Failed, QObject::tr("The TSA signature hash algorithm is unsupported or too weak."));
    std::unique_ptr<TS_VERIFY_CTX, decltype(&TS_VERIFY_CTX_free)> verification(
        TS_VERIFY_CTX_new(), TS_VERIFY_CTX_free);
    if (!verification)
        return failure(Status::Failed, QObject::tr("Cannot initialize timestamp verification."));
#if OPENSSL_VERSION_NUMBER >= 0x30400000L
    if (TS_VERIFY_CTX_set0_store(verification.get(), store.get()) != 1)
        return failure(Status::Failed, QObject::tr("Cannot configure timestamp trust."));
#else
    TS_VERIFY_CTX_set_store(verification.get(), store.get());
#endif
    auto *validatedStore = store.release(); // The verification context owns the store.
    TS_VERIFY_CTX_set_flags(verification.get(), TS_VFY_VERSION | TS_VFY_SIGNATURE | TS_VFY_SIGNER);
    // The imprint was checked above. Verify the token signature, TSA name,
    // chain, and timeStamping EKU at current time, without inferring revocation.
    if (TS_RESP_verify_token(verification.get(), token.get()) != 1) {
        const auto errors = jpdf::crypto::detail::takeOpenSslErrors();
        return failure(Status::Failed, QObject::tr("Timestamp signature or TSA-chain validation failed: %1")
            .arg(errors.isEmpty() ? QObject::tr("No backend diagnostic available.") : errors.join(QStringLiteral("; "))));
    }
    SignatureTimestamp result{Status::Verified, verifiedTime,
        QObject::tr("RFC 3161 imprint, token signature, and TSA chain verified using system/OpenSSL CA roots. "
            "This proves the signature existed by the timestamp time, not its exact creation time. "
            "The TSA chain is checked at current time; revocation requires a separate online check.")};
    auto *tsaCertificates = PKCS7_get0_signers(token.get(), nullptr, 0);
    X509 *tsaCertificate = tsaCertificates && sk_X509_num(tsaCertificates) == 1
        ? sk_X509_value(tsaCertificates, 0) : nullptr;
    sk_X509_free(tsaCertificates);
    std::unique_ptr<X509_STORE_CTX, decltype(&X509_STORE_CTX_free)> chain(
        X509_STORE_CTX_new(), X509_STORE_CTX_free);
    if (tsaCertificate && chain && X509_STORE_CTX_init(chain.get(), validatedStore, tsaCertificate,
            token->d.sign->cert) == 1
        && X509_STORE_CTX_set_purpose(chain.get(), X509_PURPOSE_TIMESTAMP_SIGN) == 1) {
        X509_VERIFY_PARAM_set_depth(X509_STORE_CTX_get0_param(chain.get()), 16);
        X509_VERIFY_PARAM_set_time(X509_STORE_CTX_get0_param(chain.get()), time.systemUtc().toSecsSinceEpoch());
        if (X509_verify_cert(chain.get()) == 1)
            result.certificateChain = exportValidatedChain(X509_STORE_CTX_get0_chain(chain.get()));
    }
    if (result.certificateChain.isEmpty())
        result.description += QObject::tr(" The validated TSA path could not be exported for revocation checking.");
    ERR_clear_error();
    return result;
}
