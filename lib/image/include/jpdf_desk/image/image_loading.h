#pragma once

#include <QImage>
#include <QString>

// Load a source image without widgets. Reject files over 32 MiB and JPEGs over
// 16 megapixels. Returns a detached image, or null with an optional error.
// JPEG uses MuPDF: importing Qt's static JPEG plugin alongside MuPDF's libjpeg
// causes conflicting JPEG symbols because the libraries have different ABIs.
QImage loadSourceImage(const QString &path, QString *error = nullptr);
