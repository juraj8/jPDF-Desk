#include "jpdf_desk/signing/openssl_provider.h"
#include "jpdf_desk/crypto/detail/validation_clock.h"

#include <QCoreApplication>
#include <QFile>
#include <QProcess>
#include <QTemporaryDir>
#include <openssl/pem.h>
#include <openssl/pkcs7.h>
#include <iostream>
#include <memory>

namespace {
bool writeFile(const QString &path, const QByteArray &data)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(data) == data.size();
}
QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
QByteArray encode(PKCS7 *signature)
{
    const int length = i2d_PKCS7(signature, nullptr);
    if (length <= 0) return {};
    QByteArray bytes(length, '\0');
    auto *data = reinterpret_cast<unsigned char *>(bytes.data());
    return i2d_PKCS7(signature, &data) == length ? bytes : QByteArray();
}
bool duplicateSigner(PKCS7 *signature)
{
    auto *infos = PKCS7_get_signer_info(signature);
    auto *copy = infos && sk_PKCS7_SIGNER_INFO_num(infos) > 0
        ? static_cast<PKCS7_SIGNER_INFO *>(ASN1_item_dup(ASN1_ITEM_rptr(PKCS7_SIGNER_INFO),
              sk_PKCS7_SIGNER_INFO_value(infos, 0))) : nullptr;
    if (!copy) return false;
    if (sk_PKCS7_SIGNER_INFO_push(infos, copy) > 0) return true;
    PKCS7_SIGNER_INFO_free(copy);
    return false;
}
bool attach(PKCS7_SIGNER_INFO *signer, const QByteArray &token)
{
    sk_X509_ATTRIBUTE_pop_free(signer->unauth_attr, X509_ATTRIBUTE_free);
    signer->unauth_attr = nullptr;
    return X509at_add1_attr_by_NID(&signer->unauth_attr, NID_id_smime_aa_timeStampToken,
                                 V_ASN1_SEQUENCE, reinterpret_cast<const unsigned char *>(token.constData()), token.size()) != nullptr;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    if (!dir.isValid() || argc != 2) return 1;
    const auto run = [&](const QStringList &arguments) {
        QProcess process;
        process.start(QString::fromLocal8Bit(argv[1]), arguments);
        if (process.waitForFinished(30000) && process.exitStatus() == QProcess::NormalExit
            && process.exitCode() == 0) return true;
        std::cerr << process.readAllStandardError().constData() << '\n';
        return false;
    };
    const auto keyPath = dir.filePath("tsa-key.pem");
    const auto certPath = dir.filePath("tsa.pem");
    if (!run({"req", "-x509", "-newkey", "rsa:2048", "-nodes", "-keyout", keyPath,
              "-out", certPath, "-subj", "/CN=Test Timestamp Authority", "-days", "1",
              "-addext", "extendedKeyUsage=critical,timeStamping",
              "-addext", "keyUsage=critical,digitalSignature",
              "-addext", "basicConstraints=critical,CA:FALSE"})) return 2;
    using Bio = std::unique_ptr<BIO, decltype(&BIO_free)>;
    const QByteArray certData = readFile(certPath), keyData = readFile(keyPath);
    Bio certBio(BIO_new_mem_buf(certData.constData(), certData.size()), BIO_free);
    Bio keyBio(BIO_new_mem_buf(keyData.constData(), keyData.size()), BIO_free);
    std::unique_ptr<X509, decltype(&X509_free)> cert(
        PEM_read_bio_X509(certBio.get(), nullptr, nullptr, nullptr), X509_free);
    std::unique_ptr<EVP_PKEY, decltype(&EVP_PKEY_free)> key(
        PEM_read_bio_PrivateKey(keyBio.get(), nullptr, nullptr, nullptr), EVP_PKEY_free);
    Bio input(BIO_new_mem_buf("signed data", 11), BIO_free);
    if (!cert || !key || !input) return 3;
    std::unique_ptr<PKCS7, decltype(&PKCS7_free)> signature(
        PKCS7_sign(cert.get(), key.get(), nullptr, input.get(), PKCS7_BINARY | PKCS7_DETACHED), PKCS7_free);
    if (!signature) return 4;
    const auto services = openSslSignatureServices();
    using Status = SignatureTimestamp::Status;
    if (services.verification->certificateDetails(encode(signature.get())).timestamp.status != Status::Absent) return 5;
    auto *signer = sk_PKCS7_SIGNER_INFO_value(PKCS7_get_signer_info(signature.get()), 0);
    const QByteArray signedValue(reinterpret_cast<const char *>(ASN1_STRING_get0_data(signer->enc_digest)),
                                 ASN1_STRING_length(signer->enc_digest));
    const auto dataPath = dir.filePath("signature.bin");
    const auto queryPath = dir.filePath("request.tsq");
    const auto tokenPath = dir.filePath("token.der");
    const auto configPath = dir.filePath("tsa.cnf");
    const QByteArray config = QStringLiteral(
        "[tsa]\nserial=%1\nsigner_cert=%2\nsigner_key=%3\nsigner_digest=sha256\n"
        "default_policy=1.2.3.4.1\ndigests=sha256,sha384,sha512\naccuracy=secs:1\n"
        "ordering=yes\ntsa_name=yes\ness_cert_id_chain=no\n")
        .arg(dir.filePath("serial"), certPath, keyPath).toUtf8();
    if (!writeFile(dataPath, signedValue) || !writeFile(dir.filePath("serial"), "01\n")
        || !writeFile(configPath, config)
        || !run({"ts", "-query", "-data", dataPath, "-sha256", "-cert", "-out", queryPath})
        || !run({"ts", "-reply", "-queryfile", queryPath, "-config", configPath,
                 "-section", "tsa", "-token_out", "-out", tokenPath})) return 6;
    const auto token = readFile(tokenPath);
    if (token.isEmpty() || !attach(signer, token)) return 7;
    // A cryptographically correct token from an untrusted TSA is not verified.
    if (services.verification->certificateDetails(encode(signature.get())).timestamp.status != Status::Failed) return 8;
    // Isolate this test process's OpenSSL trust store; never modify the host store.
    qputenv("SSL_CERT_FILE", QFile::encodeName(certPath));
    qputenv("SSL_CERT_DIR", QFile::encodeName(dir.path()));
    const auto verified = services.verification->certificateDetails(encode(signature.get())).timestamp;
    if (verified.status != Status::Verified || !verified.time.isValid()
        || verified.certificateChain.size() != 1 || verified.certificateChain[0].der.isEmpty()) {
        std::cerr << verified.description.toStdString() << '\n';
        return 9;
    }
    const auto badTime = jpdf::crypto::ValidationTime::atSystemTime(
        jpdf::crypto::detail::validationClockBounds().build);
    const auto blocked = services.verification->certificateDetails(encode(signature.get()), badTime);
    if (blocked.systemChain != CertificateChainValidation::Failed || !blocked.signerChain.isEmpty()
        || blocked.timestamp.status != Status::Failed || !blocked.timestamp.certificateChain.isEmpty()
        || !blocked.description.contains("system clock")) return 16;
    const auto expiredTime = jpdf::crypto::ValidationTime::atSystemTime(verified.time.addDays(2));
    const auto expired = services.verification->certificateDetails(encode(signature.get()), expiredTime).timestamp;
    if (expired.status != Status::Failed || !expired.description.contains("TSA-chain")) return 17;
    QByteArray changedValue = signedValue;
    changedValue[0] = static_cast<char>(changedValue[0] ^ 1);
    ASN1_OCTET_STRING_set(signer->enc_digest,
        reinterpret_cast<const unsigned char *>(changedValue.constData()), changedValue.size());
    const auto mismatched = services.verification->certificateDetails(encode(signature.get())).timestamp;
    if (mismatched.status != Status::Invalid || mismatched.time.isValid()
        || !mismatched.description.contains("does not match")) return 10;
    ASN1_OCTET_STRING_set(signer->enc_digest,
        reinterpret_cast<const unsigned char *>(signedValue.constData()), signedValue.size());
    const auto *tokenData = reinterpret_cast<const unsigned char *>(token.constData());
    std::unique_ptr<PKCS7, decltype(&PKCS7_free)> damaged(
        d2i_PKCS7(nullptr, &tokenData, token.size()), PKCS7_free);
    if (!damaged) return 11;
    auto *tsaSigner = sk_PKCS7_SIGNER_INFO_value(PKCS7_get_signer_info(damaged.get()), 0);
    QByteArray brokenValue(reinterpret_cast<const char *>(ASN1_STRING_get0_data(tsaSigner->enc_digest)),
                           ASN1_STRING_length(tsaSigner->enc_digest));
    brokenValue[0] = static_cast<char>(brokenValue[0] ^ 1);
    ASN1_OCTET_STRING_set(tsaSigner->enc_digest,
        reinterpret_cast<const unsigned char *>(brokenValue.constData()), brokenValue.size());
    if (!attach(signer, encode(damaged.get()))) return 12;
    const auto broken = services.verification->certificateDetails(encode(signature.get())).timestamp;
    if (broken.status != Status::Failed || broken.time.isValid()) return 13;
    // A timestamp attribute of the wrong ASN.1 type must not become "absent".
    sk_X509_ATTRIBUTE_pop_free(signer->unauth_attr, X509_ATTRIBUTE_free);
    signer->unauth_attr = nullptr;
    if (!X509at_add1_attr_by_NID(&signer->unauth_attr, NID_id_smime_aa_timeStampToken,
            V_ASN1_OCTET_STRING, reinterpret_cast<const unsigned char *>(token.constData()), token.size())) return 18;
    if (services.verification->certificateDetails(encode(signature.get())).timestamp.status != Status::Invalid) return 19;
    if (!attach(signer, token)) return 20;
    auto *duplicate = X509_ATTRIBUTE_dup(sk_X509_ATTRIBUTE_value(signer->unauth_attr, 0));
    if (!duplicate) return 20;
    if (sk_X509_ATTRIBUTE_push(signer->unauth_attr, duplicate) <= 0) {
        X509_ATTRIBUTE_free(duplicate);
        return 20;
    }
    const auto duplicateAttribute = services.verification->certificateDetails(encode(signature.get())).timestamp;
    if (duplicateAttribute.status != Status::Failed || !duplicateAttribute.description.contains("Multiple")
        || duplicateAttribute.time.isValid() || !duplicateAttribute.certificateChain.isEmpty()) return 21;
    // Restore a valid TSA signature, then duplicate its signer info.
    brokenValue[0] = static_cast<char>(brokenValue[0] ^ 1);
    ASN1_OCTET_STRING_set(tsaSigner->enc_digest,
        reinterpret_cast<const unsigned char *>(brokenValue.constData()), brokenValue.size());
    if (!duplicateSigner(damaged.get()) || !attach(signer, encode(damaged.get()))) return 22;
    const auto duplicateTsa = services.verification->certificateDetails(encode(signature.get())).timestamp;
    if (duplicateTsa.status != Status::Failed || !duplicateTsa.description.contains("exactly one TSA")
        || duplicateTsa.time.isValid() || !duplicateTsa.certificateChain.isEmpty()) return 23;
    if (!attach(signer, QByteArray::fromHex("3000"))) return 14;
    if (services.verification->certificateDetails(encode(signature.get())).timestamp.status != Status::Invalid) return 15;
    if (!attach(signer, token)) return 24;
    const auto valid = encode(signature.get());
    const auto padded = services.verification->certificateDetails(valid + QByteArray(32, '\0'));
    if (!padded.error.isEmpty() || padded.timestamp.status != Status::Verified) return 25;
    for (const auto &invalid : {QByteArray(), QByteArray("not PKCS7"), valid.left(valid.size() / 2), valid + "junk"}) {
        const auto metadata = services.verification->certificateDetails(invalid);
        if (metadata.error.isEmpty() || metadata.systemChain != CertificateChainValidation::Failed
            || !metadata.signerChain.isEmpty() || metadata.timestamp.status == Status::Verified) return 26;
    }
    std::unique_ptr<PKCS7, decltype(&PKCS7_free)> plainData(PKCS7_new(), PKCS7_free);
    if (!plainData || PKCS7_set_type(plainData.get(), NID_pkcs7_data) != 1) return 32;
    const auto unsupported = services.verification->certificateDetails(encode(plainData.get()));
    if (unsupported.error.isEmpty() || unsupported.systemChain != CertificateChainValidation::Failed) return 33;
    const auto *validData = reinterpret_cast<const unsigned char *>(valid.constData());
    std::unique_ptr<PKCS7, decltype(&PKCS7_free)> missingCertificate(
        d2i_PKCS7(nullptr, &validData, valid.size()), PKCS7_free);
    if (!missingCertificate) return 34;
    sk_X509_pop_free(missingCertificate->d.sign->cert, X509_free);
    missingCertificate->d.sign->cert = nullptr;
    const auto missing = services.verification->certificateDetails(encode(missingCertificate.get()));
    if (missing.error.isEmpty() || missing.systemChain != CertificateChainValidation::Failed
        || !missing.signerChain.isEmpty()) return 35;
    // Duplicate signer infos can reference the same certificate; counting distinct
    // certificates is not enough to establish a unique supported signer.
    if (!duplicateSigner(signature.get())) return 27;
    const auto duplicateOuter = services.verification->certificateDetails(encode(signature.get()));
    if (duplicateOuter.error.isEmpty() || duplicateOuter.systemChain != CertificateChainValidation::Failed
        || !duplicateOuter.signerChain.isEmpty() || duplicateOuter.timestamp.status == Status::Verified) return 28;
    sk_PKCS7_SIGNER_INFO_pop_free(signature->d.sign->signer_info, PKCS7_SIGNER_INFO_free);
    signature->d.sign->signer_info = sk_PKCS7_SIGNER_INFO_new_null();
    if (!signature->d.sign->signer_info) return 29;
    const auto noSigner = services.verification->certificateDetails(encode(signature.get()));
    if (noSigner.error.isEmpty() || noSigner.systemChain != CertificateChainValidation::Failed) return 30;
    // A preceding malformed decode must not poison the next valid validation.
    const auto recovered = services.verification->certificateDetails(valid);
    if (!recovered.error.isEmpty() || recovered.timestamp.status != Status::Verified) return 31;
    return 0;
}
