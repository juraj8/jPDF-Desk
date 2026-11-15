#pragma once

#include "signature_timestamp.h"
#include "certificate_validation.h"
#include "jpdf_desk/revocation/revocation_types.h"

#include <QByteArray>
#include <QDateTime>
#include <QStringList>
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

    friend bool operator==(const TextField &a, const TextField &b)
    { return a.rect == b.rect && a.text == b.text && a.fontSize == b.fontSize; }
};

struct OptionMark {
    enum Kind { Check, Cross } kind;
    QPointF center; // Scene coordinates, at the center of the option box.

    friend bool operator==(const OptionMark &a, const OptionMark &b)
    { return a.kind == b.kind && a.center == b.center; }
};

struct Signature {
    QRectF rect; // Scene coordinates.
    QByteArray png; // Cropped transparent PNG for a new, unsaved image.
    QString id; // Saved page-content stream ID; removal only, no PNG copy.

    friend bool operator==(const Signature &a, const Signature &b)
    { return a.rect == b.rect && a.png == b.png && a.id == b.id; }
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

    friend bool operator==(const DocumentAnnotations &a, const DocumentAnnotations &b)
    { return a.fields == b.fields && a.marks == b.marks && a.signatures == b.signatures; }
};

// Internal navigation only; coordinates use the same scene units as annotations.
struct PdfPageLink {
    QRectF rect;
    int page = -1;
    QPointF position;
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

struct SignatureRevocationCheck {
    bool timestampAuthority = false;
    bool trustAnchorOnly = false;
    int depth = 0;
    QString subject;
    jpdf::revocation::Result result;
};

struct DigitalSignatureStatus {
    enum class Integrity { NotChecked, Intact, Invalid, Failed };
    using CertificateValidation = CertificateChainValidation;
    QString fieldName;
    bool signedField = false;
    QString signer;
    bool digestValid = false;
    QString digestStatus;
    Integrity integrity = Integrity::NotChecked;
    bool certificateTrusted = false;
    QString certificateStatus;
    CertificateValidation certificateValidation = CertificateValidation::NotChecked;
    QDateTime certificateValidFrom;
    QDateTime certificateValidUntil;
    QDateTime signingTime; // Claimed PDF signing time, not a verified timestamp.
    QDateTime verificationTime; // Captured UTC time used by the offline checks.
    QDateTime revocationTime; // Captured UTC time used by the online checks.
    SignatureTimestamp timestamp;
    ValidatedCertificateChain signerChain;
    bool revocationAttempted = false;
    QVector<SignatureRevocationCheck> revocationChecks;
    QStringList certificateProblems;
    QString certificateDescription;
    bool coverageChecked = false;
    bool changedSinceSigning = false;
    QString error;
    QString clockError; // Trust/time checks were skipped; integrity can still be established.
};
