#pragma once

#include <QImage>
#include <QString>

// Decode photos via MuPDF: the static Qt build does not import its JPEG plugin,
// and importing it alongside MuPDF's libjpeg causes conflicting JPEG symbols.
QImage loadSignatureSource(const QString &path, QString *error = nullptr);

// Remove a light paper background and crop to the ink. Returns null for blank/oversized input.
QImage extractSignature(const QImage &source);
