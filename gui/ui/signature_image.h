#pragma once

#include <QImage>
#include <QString>

// Load a signature source via the non-widget image utility.
// Backend decoding and the static libjpeg workaround live in lib/image/.
QImage loadSignatureSource(const QString &path, QString *error = nullptr);

// Remove a light paper background and crop to the ink. Returns null for blank/oversized input.
QImage extractSignature(const QImage &source);
