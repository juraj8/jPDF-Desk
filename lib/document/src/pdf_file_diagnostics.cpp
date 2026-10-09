#include "jpdf_desk/document/pdf_file_diagnostics.h"

#include <QCryptographicHash>
#include <QDateTime>
#include <QFile>
#include <QFileInfo>
#include <QObject>
#include <QRegularExpression>
#include <QStringList>

namespace {
// This is deliberately not a second PDF parser. Only bounded, plain-text hints
// are exposed when the normal engine cannot open the security handler.
constexpr qint64 scanLimit = 32 * 1024 * 1024;
constexpr qsizetype objectLimit = 64 * 1024;

QString capture(const QString &text, const QString &pattern)
{
    return QRegularExpression(pattern).match(text).captured(1);
}

QString reference(const QString &text, const QString &key)
{
    return capture(text, QStringLiteral("/%1\\s+(\\d+\\s+\\d+)\\s+R\\b").arg(key));
}

QString object(const QString &text, const QString &ref)
{
    if (ref.isEmpty()) return {};
    const QStringList parts = ref.split(QRegularExpression(QStringLiteral("\\s+")));
    if (parts.size() != 2) return {};
    const QRegularExpression header(QStringLiteral("(?:^|[\\r\\n])%1\\s+%2\\s+obj\\b")
                                       .arg(parts[0], parts[1]));
    // Prefer the last visible definition for incrementally saved documents.
    auto matches = header.globalMatch(text);
    qsizetype start = -1;
    while (matches.hasNext()) start = matches.next().capturedEnd();
    if (start < 0) return {};
    const QString chunk = text.mid(start, objectLimit);
    const qsizetype end = chunk.indexOf(QStringLiteral("endobj"));
    if (end < 0) return {};
    return chunk.left(end);
}

QString integer(const QString &text, const QString &key)
{
    return capture(text, QStringLiteral("/%1\\s+([0-9]{1,10})(?=[\\s/<>()\\[\\]]|$)").arg(key));
}

QString name(const QString &text, const QString &key)
{
    return capture(text, QStringLiteral("/%1\\s*/([A-Za-z0-9_.-]{1,128})(?=[\\s/<>()\\[\\]]|$)").arg(key));
}

QString plainString(const QString &text, const QString &key)
{
    // Only simple printable literal strings; escaped/hex/encrypted values are
    // omitted rather than incorrectly decoded. Never include the opaque INFO blob.
    return capture(text, QStringLiteral("/%1\\s*\\(([\\x20-\\x27\\x2a-\\x5b\\x5d-\\x7e]{1,160})\\)").arg(key));
}
} // namespace

QString pdfFileDiagnostics(const QString &path)
{
    QStringList lines;
    const auto add = [&](const QString &label, const QString &value) {
        if (!value.isEmpty()) lines.append(label + QStringLiteral(": ") + value);
    };
    const QFileInfo info(path);
    add(QObject::tr("File"), info.fileName());
    add(QObject::tr("Path"), info.absoluteFilePath());
    add(QObject::tr("Size (bytes)"), QString::number(info.size()));
    add(QObject::tr("Filesystem created"), info.birthTime().toString(Qt::ISODate));
    add(QObject::tr("Filesystem modified"), info.lastModified().toString(Qt::ISODate));

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        add(QObject::tr("Inspection unavailable"), file.errorString());
        return lines.join(QLatin1Char('\n'));
    }
    if (file.size() > scanLimit) {
        const QString header = QString::fromLatin1(file.read(1024));
        add(QObject::tr("PDF header version"), capture(header, QStringLiteral("%PDF-([0-9]\\.[0-9])")));
        lines.append(QObject::tr("Structural inspection and SHA-256 skipped: file exceeds the 32 MiB inspection limit."));
        return lines.join(QLatin1Char('\n'));
    }
    const QByteArray bytes = file.read(scanLimit + 1);
    if (file.error() != QFileDevice::NoError || bytes.size() != file.size() || bytes.size() > scanLimit) {
        lines.append(QObject::tr("Inspection incomplete: the file could not be fully read or changed during inspection."));
        return lines.join(QLatin1Char('\n'));
    }
    add(QObject::tr("SHA-256"), QString::fromLatin1(QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex()));
    const QString text = QString::fromLatin1(bytes);
    add(QObject::tr("PDF header version"), capture(text.left(1024), QStringLiteral("%PDF-([0-9]\\.[0-9])")));
    lines.append(QString());
    lines.append(QObject::tr("Raw structural hints (unauthenticated, best effort; not a validated PDF parse). "
                            "Compressed objects and escaped values are not inspected; incremental updates may leave stale values."));
    // Restrict lookups to a small final-trailer region, then follow only explicit
    // object references. Do not search arbitrary encrypted streams for metadata.
    const qsizetype trailerPos = text.lastIndexOf(QStringLiteral("trailer"));
    const QString trailer = trailerPos < 0 ? QString() : text.mid(trailerPos, objectLimit);
    if (trailer.isEmpty()) {
        lines.append(QObject::tr("No plain-text trailer found; structural details unavailable."));
        return lines.join(QLatin1Char('\n'));
    }
    add(QObject::tr("Trailer /Size (object-number limit)"), integer(trailer, QStringLiteral("Size")));
    const QString encryptRef = reference(trailer, QStringLiteral("Encrypt"));
    add(QObject::tr("Encryption object"), encryptRef.isEmpty() ? QString() : encryptRef + QStringLiteral(" R"));
    const QString encryption = object(text, encryptRef);
    add(QObject::tr("Security handler"), name(encryption, QStringLiteral("Filter")));
    add(QObject::tr("Encryption version (/V)"), integer(encryption, QStringLiteral("V")));
    add(QObject::tr("Declared key length (bits)"), integer(encryption, QStringLiteral("Length")));
    add(QObject::tr("FileOpen version (/VEID)"), plainString(encryption, QStringLiteral("VEID")));
    add(QObject::tr("FileOpen build (/BUILD)"), plainString(encryption, QStringLiteral("BUILD")));
    add(QObject::tr("Service ID (/SVID)"), plainString(encryption, QStringLiteral("SVID")));
    add(QObject::tr("Document ID (/DUID)"), plainString(encryption, QStringLiteral("DUID")));
    if (encryption.contains(QStringLiteral("/INFO")))
        lines.append(QObject::tr("FileOpen /INFO: opaque authorization data present (not displayed)."));
    const QString ids = capture(trailer, QStringLiteral("/ID\\s*\\[([^\\]]{1,1024})\\]"));
    auto idMatches = QRegularExpression(QStringLiteral("<([0-9A-Fa-f]{1,128})>")).globalMatch(ids);
    int idIndex = 0;
    while (idMatches.hasNext() && idIndex < 2)
        add(QObject::tr("Trailer ID %1 (hex)").arg(++idIndex), idMatches.next().captured(1));

    const QString root = object(text, reference(trailer, QStringLiteral("Root")));
    const QString pages = object(text, reference(root, QStringLiteral("Pages")));
    add(QObject::tr("Declared page count"), integer(pages, QStringLiteral("Count")));
    QString pageNode = pages;
    QString mediaBox;
    for (int depth = 0; depth < 16 && !pageNode.isEmpty(); ++depth) {
        const QString box = capture(pageNode, QStringLiteral(R"(/MediaBox\s*\[([0-9. +\-\r\n]{1,128})\])"));
        if (!box.isEmpty()) mediaBox = box.simplified();
        if (name(pageNode, QStringLiteral("Type")) == QStringLiteral("Page")) {
            add(QObject::tr("First page MediaBox (PDF units)"), mediaBox);
            break;
        }
        pageNode = object(text, capture(pageNode, QStringLiteral(R"(/Kids\s*\[\s*(\d+\s+\d+)\s+R\b)")));
    }
    add(QObject::tr("Page layout"), name(root, QStringLiteral("PageLayout")));
    add(QObject::tr("Page mode"), name(root, QStringLiteral("PageMode")));
    for (const auto &key : {QStringLiteral("Outlines"), QStringLiteral("StructTreeRoot"), QStringLiteral("Metadata")}) {
        const QString ref = reference(root, key);
        if (!ref.isEmpty()) add(QObject::tr("Catalog /%1 object").arg(key), ref + QStringLiteral(" R"));
    }
    if (!encryptRef.isEmpty()) {
        lines.append(QObject::tr("Title, author, subject, keywords, creator, producer, PDF creation/modification dates, "
                                "text, and permissions: unavailable without a compatible authorized reader. "
                                "Filesystem dates above are not PDF metadata dates."));
    }
    return lines.join(QLatin1Char('\n'));
}
