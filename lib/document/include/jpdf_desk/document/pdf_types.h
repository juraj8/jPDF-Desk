#pragma once

#include <QByteArray>
#include <QMap>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

// PDF-independent values shared by the backend and scene items.
struct TextField {
    QRectF rect;
    QString text;
    float fontSize = 12;
};

struct OptionMark {
    enum Kind { Check, Cross } kind;
    QPointF center; // Scene coordinates, at the center of the option box.
};

struct Signature {
    QRectF rect; // Scene coordinates.
    QByteArray png; // Cropped transparent PNG, stored with its stamp annotation.
};

struct PageAnnotations {
    QVector<TextField> fields;
    QVector<OptionMark> marks;
    QVector<Signature> signatures;
};

// Page-local annotation snapshots. Empty entries explicitly represent deletion.
struct DocumentAnnotations {
    QMap<int, QVector<TextField>> fields;
    QMap<int, QVector<OptionMark>> marks;
    QMap<int, QVector<Signature>> signatures;
};

// Native AcroForm fields, separate from application-owned annotations.
struct PdfFormField {
    enum class Kind { Text, CheckBox, Choice, Unsupported };
    int id = 0; // Document-local object number; refresh after opening/saving.
    QString name;
    QString value;
    Kind kind = Kind::Unsupported;
    bool readOnly = false;
    bool multiline = false;
    bool password = false;
    bool editableChoice = false;
    int maxLength = 0; // Unicode characters; zero means unspecified.
    QString onValue;
    QVector<QPair<QString, QString>> options; // Export value, display label.
};

struct TextSearchMatch {
    int page = -1; // Zero-based.
    QVector<QRectF> rects; // Page-local scene coordinates; a hit may span lines.
};

struct OutlineEntry {
    QString title;
    int page = -1; // Zero-based; -1 for external or unresolved destinations.
    bool expanded = false;
    QVector<OutlineEntry> children;
};

struct DigitalSignatureStatus {
    QString fieldName;
    bool signedField = false;
    QString signer;
    bool digestValid = false;
    QString digestStatus;
    bool certificateTrusted = false;
    QString certificateStatus;
    bool changedSinceSigning = false;
    QString error;
};
