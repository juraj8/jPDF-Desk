#include "signature_store.h"
#include "app_identity.h"

#include <QDir>
#include <QFile>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QUuid>
#include <stdexcept>

namespace {
void fail(const QString &message) { throw std::runtime_error(message.toStdString()); }
void writePrivateFile(const QString &path, const QByteArray &data)
{
    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly)) fail(file.errorString());
    if (!file.setPermissions(QFile::ReadOwner | QFile::WriteOwner)) fail(file.errorString());
    if (file.write(data) != data.size() || !file.commit()) fail(file.errorString());
}
}

SignatureStore::SignatureStore(Kind kind, const QString &root) : kind_(kind)
{
    const QString base = root.isEmpty() ? applicationDataRoot() : root;
    directory_ = QDir(base).filePath(kind == Kind::Image ? QStringLiteral("signature-images")
                                                       : QStringLiteral("signing-certificates"));
}

QString SignatureStore::path(const QString &id) const
{
    return QDir(directory_).filePath(id + (kind_ == Kind::Image ? QStringLiteral(".png")
                                                             : QStringLiteral(".p12")));
}

SignatureStore::State SignatureStore::load() const
{
    QFile file(QDir(directory_).filePath(QStringLiteral("index.json")));
    if (!file.exists()) return {};
    if (!file.open(QIODevice::ReadOnly)) fail(file.errorString());
    QJsonParseError error;
    const auto document = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError || !document.isObject())
        fail(QStringLiteral("Cannot read the saved signature index."));
    State state;
    for (const auto &value : document.object().value(QStringLiteral("entries")).toArray()) {
        const auto object = value.toObject();
        const QString id = object.value(QStringLiteral("id")).toString();
        // Index contents must never be able to escape the asset directory.
        if (QUuid(id).isNull() || QUuid(id).toString(QUuid::WithoutBraces) != id) continue;
        if (QFile::exists(path(id)))
            state.entries.append({id, object.value(QStringLiteral("name")).toString()});
    }
    const QString active = document.object().value(QStringLiteral("active")).toString();
    for (const auto &entry : state.entries)
        if (entry.id == active) state.active = active;
    return state;
}

void SignatureStore::save(const State &state) const
{
    if (!QDir().mkpath(directory_)) fail(QStringLiteral("Cannot create the signature storage directory."));
#ifdef Q_OS_UNIX
    if (!QFile::setPermissions(directory_, QFile::ReadOwner | QFile::WriteOwner | QFile::ExeOwner))
        fail(QStringLiteral("Cannot restrict access to the signature storage directory."));
#endif
    QJsonArray entries;
    for (const auto &entry : state.entries)
        entries.append(QJsonObject{{QStringLiteral("id"), entry.id}, {QStringLiteral("name"), entry.name}});
    writePrivateFile(QDir(directory_).filePath(QStringLiteral("index.json")),
                     QJsonDocument(QJsonObject{{QStringLiteral("entries"), entries},
                                               {QStringLiteral("active"), state.active}}).toJson());
}

QVector<SignatureStore::Entry> SignatureStore::entries() const { return load().entries; }
QString SignatureStore::activeId() const { return load().active; }
QString SignatureStore::activePath() const
{
    const auto id = activeId();
    return id.isEmpty() ? QString() : path(id);
}
QByteArray SignatureStore::activeImage() const
{
    if (kind_ != Kind::Image) return {};
    const QString asset = activePath();
    if (asset.isEmpty()) return {};
    QFile file(asset);
    if (!file.open(QIODevice::ReadOnly)) fail(file.errorString());
    const auto bytes = file.readAll();
    if (QImage::fromData(bytes, "PNG").isNull()) fail(QStringLiteral("The saved signature image is unreadable."));
    return bytes;
}

QString SignatureStore::importAsset(const QString &name, const QByteArray &data)
{
    if (name.trimmed().isEmpty() || data.isEmpty()) fail(QStringLiteral("An asset needs a name and data."));
    if (data.size() > 5 * 1024 * 1024) fail(QStringLiteral("The asset exceeds the 5 MB limit."));
    if (kind_ == Kind::Image && QImage::fromData(data, "PNG").isNull())
        fail(QStringLiteral("The signature is not a valid PNG image."));
    auto state = load();
    // Establish restricted directory permissions before writing private key material.
    save(state);
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    writePrivateFile(path(id), data);
    state.entries.append({id, name.trimmed()});
    state.active = id;
    try { save(state); } catch (...) { QFile::remove(path(id)); throw; }
    return id;
}

void SignatureStore::select(const QString &id)
{
    auto state = load();
    for (const auto &entry : state.entries) {
        if (entry.id != id) continue;
        state.active = id;
        save(state);
        return;
    }
    fail(QStringLiteral("The selected asset no longer exists."));
}
void SignatureStore::rename(const QString &id, const QString &name)
{
    if (name.trimmed().isEmpty()) fail(QStringLiteral("The name cannot be empty."));
    auto state = load();
    for (auto &entry : state.entries) {
        if (entry.id != id) continue;
        entry.name = name.trimmed();
        save(state);
        return;
    }
    fail(QStringLiteral("The selected asset no longer exists."));
}
void SignatureStore::remove(const QString &id)
{
    auto state = load();
    for (qsizetype i = 0; i < state.entries.size(); ++i) {
        if (state.entries[i].id != id) continue;
        if (!QFile::remove(path(id))) fail(QStringLiteral("Cannot remove the saved asset."));
        state.entries.removeAt(i);
        if (state.active == id) state.active.clear();
        save(state);
        return;
    }
    fail(QStringLiteral("The selected asset no longer exists."));
}
