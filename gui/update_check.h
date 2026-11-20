#pragma once

#include <QString>
#include <functional>

class QByteArray;
class QObject;
class QUrl;
class QVersionNumber;
class QWidget;

// Returns a newer stable tag, or an empty string. Invalid responses also set error.
QString newerStableReleaseTag(const QByteArray &response, const QVersionNumber &current, QString &error);
QUrl releasePageUrl(const QString &tag);
// Calls onAvailable only for a newer stable release and only while context is alive.
void checkForUpdates(QObject *context, std::function<void(const QString &tag)> onAvailable);
void showUpdateNotification(QWidget *parent);
