#include "jpdf_desk/document/pdf_file_diagnostics.h"

#include <QCoreApplication>
#include <QCryptographicHash>
#include <QFile>
#include <QTemporaryDir>
#include <iostream>

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir dir;
    if (!dir.isValid()) return 1;
    // Structural fixture only: diagnostics must not require successful PDF opening.
    const QByteArray data = "%PDF-1.6\r\n"
        "1 0 obj<</Type/Catalog/Pages 2 0 R/PageLayout/SinglePage/PageMode/UseOutlines>>\rendobj\r"
        "2 0 obj<</Type/Pages/Count 188/Kids[3 0 R]>>\rendobj\r"
        "3 0 obj<</Type/Page/Parent 2 0 R/MediaBox[0 0 595 842]>>\rendobj\r"
        "4 0 obj<</Filter/FOPN_foweb/V 2/Length 128/VEID(9.1)/BUILD(926)"
        "/SVID(TESTSERVICE)/DUID(123456)/INFO(secret-auth-data)>>\rendobj\r"
        "trailer\r<</Root 1 0 R/Encrypt 4 0 R/Size 5/ID[<ABC123><DEF456>]>>\r%%EOF\r";
    const QString path = dir.filePath("protected.pdf");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size()) return 2;
    file.close();
    const QString report = pdfFileDiagnostics(path);
    for (const auto *expected : {"PDF header version: 1.6", "Declared page count: 188",
             "First page MediaBox (PDF units): 0 0 595 842", "Security handler: FOPN_foweb",
             "Encryption version (/V): 2", "Declared key length (bits): 128",
             "FileOpen version (/VEID): 9.1", "FileOpen build (/BUILD): 926",
             "Service ID (/SVID): TESTSERVICE", "Document ID (/DUID): 123456",
             "Trailer ID 1 (hex): ABC123", "Trailer ID 2 (hex): DEF456",
             "Page mode: UseOutlines", "unauthenticated", "unavailable without"}) {
        if (!report.contains(QString::fromLatin1(expected))) {
            std::cerr << "Missing: " << expected << '\n' << report.toStdString();
            return 3;
        }
    }
    if (report.contains("secret-auth-data")
        || !report.contains(QString::fromLatin1(QCryptographicHash::hash(data, QCryptographicHash::Sha256).toHex()))) return 4;
    if (!file.open(QIODevice::ReadOnly) || file.readAll() != data) return 5;
    file.close();
    if (!pdfFileDiagnostics(dir.filePath("missing.pdf")).contains("Inspection unavailable")) return 6;

    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate) || file.write("%PDF-1.7\ntruncated") < 0) return 7;
    file.close();
    const QString truncated = pdfFileDiagnostics(path);
    if (!truncated.contains("No plain-text trailer found") || truncated.contains("Declared page count:")) return 8;

    // Bound memory and work on oversized files (resize creates a sparse fixture).
    if (!file.open(QIODevice::WriteOnly) || !file.resize(32 * 1024 * 1024 + 1)) return 9;
    file.close();
    const QString large = pdfFileDiagnostics(path);
    if (!large.contains("32 MiB inspection limit") || large.contains("SHA-256:")) return 10;
    return 0;
}
