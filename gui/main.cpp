#include "main_window.h"
#ifdef PDF_FILLER_HAS_OPENSSL
#include "pdf_filler/signing/openssl_provider.h"
#endif

#include <QApplication>
#include <QCommandLineParser>
#include <QTimer>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    // Keep the legacy settings/data identity so existing user assets survive
    // the branding change. Only the user-visible application name is renamed.
    QCoreApplication::setOrganizationName(QStringLiteral("PDF Filler"));
    QCoreApplication::setApplicationName(QStringLiteral("PDF Filler"));
    QGuiApplication::setApplicationDisplayName(QStringLiteral("jPDF Desk"));
    // Fusion consistently honors the palette on every supported desktop.
    QApplication::setStyle(QStringLiteral("Fusion"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("View, fill, sign and secure PDF documents"));
    parser.addHelpOption();
    parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("PDF file to open"), QStringLiteral("[file]"));
    parser.process(application);

    SignatureServices services;
#ifdef PDF_FILLER_HAS_OPENSSL
    services = openSslSignatureServices();
#endif
    MainWindow window(nullptr, services);
    application.setWindowIcon(window.windowIcon());
    window.show();

    const QStringList files = parser.positionalArguments();
    if (!files.isEmpty()) {
        // Open after the event loop starts so password/error dialogs have a
        // visible parent, just as when opening a file from the sidebar.
        QTimer::singleShot(0, &window, [&window, path = files.constFirst()] {
            window.openDocument(path);
        });
    }

    return application.exec();
}
