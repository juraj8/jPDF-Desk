#pragma once

#include <QByteArray>
#include <QString>
#include <QVector>

// Local user assets only: never stores certificate passwords or PDF state.
class SignatureStore {
public:
    enum class Kind { Image, Certificate };
    struct Entry { QString id; QString name; };
    explicit SignatureStore(Kind kind, const QString &root = {});
    QVector<Entry> entries() const;
    QString activeId() const;
    QString activePath() const;
    QByteArray activeImage() const;
    QString importAsset(const QString &name, const QByteArray &data);
    void select(const QString &id);
    void rename(const QString &id, const QString &name);
    void remove(const QString &id);
    QString directory() const { return directory_; }
    Kind kind() const { return kind_; }

private:
    struct State { QVector<Entry> entries; QString active; };
    State load() const;
    void save(const State &state) const;
    QString path(const QString &id) const;
    Kind kind_;
    QString directory_;
};
