#include "main_window.h"
#include "app_identity.h"
#ifdef JPDF_DESK_HAS_OPENSSL
#include "jpdf_desk/signing/openssl_provider.h"
#endif

#include <QApplication>
#include <QCommandLineParser>
#include <QTimer>
#include <QMessageBox>
#include <exception>

int main(int argc, char *argv[])
{
    QApplication application(argc, argv);
    try {
        initializeApplicationIdentity();
    } catch (const std::exception &e) {
        QMessageBox::warning(nullptr, QStringLiteral("jPDF Desk — Data migration"),
                             QString::fromUtf8(e.what()));
    }
    // Fusion consistently honors the palette on every supported desktop.
    QApplication::setStyle(QStringLiteral("Fusion"));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("View, fill, sign and secure PDF documents"));
    parser.addHelpOption();
    parser.addPositionalArgument(QStringLiteral("file"), QStringLiteral("PDF file to open"), QStringLiteral("[file]"));
    parser.process(application);

    SignatureServices services;
#ifdef JPDF_DESK_HAS_OPENSSL
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
