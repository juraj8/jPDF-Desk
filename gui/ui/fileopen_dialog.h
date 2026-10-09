#pragma once

class QString;
class QWidget;

// Explains unsupported FileOpen protection and optionally launches a reader
// explicitly chosen by the user. Cancellation leaves the document untouched.
void showFileOpenWarning(QWidget *parent, const QString &path, const QString &error);
