#include "main_window.h"

#include <QApplication>
#include <QFile>
#include <QFileDialog>
#include <QMessageBox>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTextEdit>
#include <QTimer>
#include <iostream>

// Synthetic security dictionaries: no real protected document or authorization needed.
static bool writePdf(const QString &path, const QByteArray &handler)
{
    QList<QByteArray> objects = {
        "<< /Type /Catalog /Pages 2 0 R >>",
        "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
        "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 100 100] >>"
    };
    if (!handler.isEmpty()) objects.append("<< /Filter /" + handler + " /V 1 /R 2 >>");
    QByteArray bytes = "%PDF-1.4\n";
    QList<int> offsets;
    for (int i = 0; i < objects.size(); ++i) {
        offsets.append(bytes.size());
        bytes += QByteArray::number(i + 1) + " 0 obj\n" + objects[i] + "\nendobj\n";
    }
    const int xref = bytes.size();
    bytes += "xref\n0 " + QByteArray::number(objects.size() + 1) + "\n0000000000 65535 f \n";
    for (int offset : offsets)
        bytes += QByteArray::number(offset).rightJustified(10, '0') + " 00000 n \n";
    bytes += "trailer\n<< /Size " + QByteArray::number(objects.size() + 1) + " /Root 1 0 R";
    if (!handler.isEmpty()) bytes += " /Encrypt 4 0 R";
    bytes += " >>\nstartxref\n" + QByteArray::number(xref) + "\n%%EOF\n";
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}

static int runTest(int argc, char **argv)
{
    QApplication app(argc, argv);
    app.setAttribute(Qt::AA_DontUseNativeDialogs);
    QTemporaryDir dir;
    if (!dir.isValid()) return 1;
    MainWindow window;
    const QString plain = dir.filePath("plain.pdf");
    if (!writePdf(plain, {})) return 2;
    window.openDocument(plain);
    const QString title = window.windowTitle();
    if (!title.contains("plain.pdf")) return 3;

    for (const QByteArray handler : {QByteArray("FOPN_foweb"), QByteArray("OtherHandler")}) {
        const bool fileOpen = handler == "FOPN_foweb";
        const QString path = dir.filePath(QString::fromLatin1(handler) + ".pdf");
        if (!writePdf(path, handler)) return 4;
        bool valid = false;
        QTimer warningTimer;
        QObject::connect(&warningTimer, &QTimer::timeout, [&] {
            auto *dialog = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
            if (!dialog) return;
            warningTimer.stop();
            if (fileOpen) {
                bool hasReader = false;
                for (auto *button : dialog->buttons())
                    if (button->text() == QStringLiteral("Choose external reader…")) hasReader = true;
                valid = dialog->windowTitle() == QStringLiteral("FileOpen-protected PDF")
                    && dialog->text().contains("FileOpen plug-in")
                    && dialog->detailedText().contains("Security handler: FOPN_foweb")
                    && dialog->detailedText().contains("Declared page count: 1")
                    && dialog->detailedText().contains("SHA-256:")
                    && dialog->detailedText().contains("Original error:")
                    && dialog->defaultButton() == dialog->button(QMessageBox::Cancel)
                    && hasReader;
                auto *donate = dialog->findChild<QPushButton *>(QStringLiteral("donateButton"));
                valid = valid && donate && donate->text() == QStringLiteral("Donate")
                    && donate->toolTip().contains(QStringLiteral("does not unlock"));
                auto *details = dialog->findChild<QTextEdit *>();
                valid = valid && details && !details->isVisible() && details->isReadOnly();
                bool toggledDetails = false;
                for (auto *button : dialog->buttons()) {
                    if (button->text() == QStringLiteral("Show Details...")) {
                        toggledDetails = true;
                        button->click();
                        valid = valid && details && details->isVisible();
                        button->click();
                        valid = valid && !details->isVisible();
                        break;
                    }
                }
                valid = valid && toggledDetails;
            } else {
                valid = dialog->windowTitle() == QStringLiteral("Cannot open PDF")
                    && dialog->text().contains("OtherHandler");
            }
            if (!valid) {
                std::cerr << "Unexpected warning dialog for " << handler.constData()
                          << ": " << dialog->windowTitle().toStdString() << '\n'
                          << dialog->text().toStdString() << '\n'
                          << dialog->detailedText().toStdString() << '\n';
                for (auto *button : dialog->buttons())
                    std::cerr << "Button: " << button->text().toStdString() << '\n';
                std::cerr << "Cancel is default: "
                          << (dialog->defaultButton() == dialog->button(QMessageBox::Cancel))
                          << ", details widget exists: " << (dialog->findChild<QTextEdit *>() != nullptr)
                          << '\n';
            }
            dialog->reject();
        });
        warningTimer.start(1);
        window.openDocument(path);
        if (!valid || window.windowTitle() != title) return 5;
        auto *print = window.findChild<QPushButton *>(QStringLiteral("printButton"));
        if (!print || !print->isEnabled()) return 6;
    }
    // Choosing an external reader is optional; cancelling its chooser is harmless.
    bool chooserShown = false;
    QTimer readerTimer;
    QObject::connect(&readerTimer, &QTimer::timeout, [&] {
        auto *dialog = qobject_cast<QMessageBox *>(QApplication::activeModalWidget());
        if (!dialog) return;
        readerTimer.stop();
        for (auto *button : dialog->buttons()) {
            if (button->text() != QStringLiteral("Choose external reader…")) continue;
            QTimer::singleShot(0, [&] {
                auto *chooser = qobject_cast<QFileDialog *>(QApplication::activeModalWidget());
                if (!chooser) return;
                chooserShown = chooser->windowTitle() == QStringLiteral("Choose external PDF reader");
                chooser->reject();
            });
            button->click();
            return;
        }
        dialog->reject();
    });
    readerTimer.start(1);
    window.openDocument(dir.filePath("FOPN_foweb.pdf"));
    if (!chooserShown || window.windowTitle() != title) return 7;
    return 0;
}

int main(int argc, char **argv)
{
    const int result = runTest(argc, argv);
    if (result != 0)
        std::cerr << "fileopen test failed with code " << result << '\n';
    return result;
}
