#include "system_certificate_store.h"

#include <QObject>
#include <openssl/err.h>
#ifdef Q_OS_WIN
#include <windows.h>
#include <wincrypt.h>
#elif defined(Q_OS_MACOS)
#include <Security/Security.h>
#endif

namespace {
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
void addCertificate(X509_STORE *store, const unsigned char *data, long size)
{
    std::unique_ptr<X509, decltype(&X509_free)> certificate(d2i_X509(nullptr, &data, size), X509_free);
    if (certificate) X509_STORE_add_cert(store, certificate.get());
    // Duplicate roots, or individual unparseable native certificates, are skipped.
    ERR_clear_error();
}
#endif
}

ValidatedCertificateChain exportValidatedChain(STACK_OF(X509) *chain)
{
    ValidatedCertificateChain result;
    const int count = chain ? sk_X509_num(chain) : 0;
    if (count < 1 || count > 18) return result;
    for (int i = 0; i < count; ++i) {
        auto *certificate = sk_X509_value(chain, i);
        const int length = i2d_X509(certificate, nullptr);
        if (length < 1 || length > 1024 * 1024) return {};
        QByteArray der(length, '\0');
        auto *data = reinterpret_cast<unsigned char *>(der.data());
        if (i2d_X509(certificate, &data) != length) return {};
        std::unique_ptr<BIO, decltype(&BIO_free)> name(BIO_new(BIO_s_mem()), BIO_free);
        if (!name || X509_NAME_print_ex(name.get(), X509_get_subject_name(certificate), 0, XN_FLAG_RFC2253) < 0) return {};
        char *text = nullptr;
        const auto size = BIO_get_mem_data(name.get(), &text);
        result.push_back({der, QString::fromUtf8(text, size)});
    }
    return result;
}

CertificateStore loadSystemCertificateStore(QString &error)
{
    ERR_clear_error();
    error.clear();
    CertificateStore store(X509_STORE_new(), X509_STORE_free);
    if (!store) {
        error = QObject::tr("Cannot initialize the system certificate store.");
        return store;
    }
    const bool defaultsLoaded = X509_STORE_set_default_paths(store.get()) == 1;
    ERR_clear_error();
#ifdef Q_OS_WIN
    // ROOT is a logical store encompassing the current user's system root stores.
    HCERTSTORE roots = CertOpenSystemStoreW(0, L"ROOT");
    if (!roots) {
        error = QObject::tr("Cannot open the Windows ROOT certificate store (error %1).")
            .arg(GetLastError());
        return CertificateStore(nullptr, X509_STORE_free);
    }
    PCCERT_CONTEXT certificate = nullptr;
    while ((certificate = CertEnumCertificatesInStore(roots, certificate)) != nullptr)
        addCertificate(store.get(), certificate->pbCertEncoded, certificate->cbCertEncoded);
    CertCloseStore(roots, 0);
#elif defined(Q_OS_MACOS)
    CFArrayRef roots = nullptr;
    const OSStatus status = SecTrustCopyAnchorCertificates(&roots);
    if (status != errSecSuccess || !roots) {
        error = QObject::tr("Cannot load macOS trust anchors (error %1).").arg(status);
        return CertificateStore(nullptr, X509_STORE_free);
    }
    for (CFIndex i = 0; i < CFArrayGetCount(roots); ++i) {
        auto certificate = static_cast<SecCertificateRef>(const_cast<void *>(CFArrayGetValueAtIndex(roots, i)));
        CFDataRef data = SecCertificateCopyData(certificate);
        if (data) {
            addCertificate(store.get(), CFDataGetBytePtr(data), CFDataGetLength(data));
            CFRelease(data);
        }
    }
    CFRelease(roots);
#else
    if (!defaultsLoaded) {
        error = QObject::tr("Cannot load OpenSSL's configured system CA paths.");
        return CertificateStore(nullptr, X509_STORE_free);
    }
#endif
    (void)defaultsLoaded;
    return store;
}
