#include "ui/signature_manager.h"
#include "ui/dialog_helpers.h"
#include "signature_store.h"
#include "ui/signature_image.h"

#include <QBuffer>
#include <QDialogButtonBox>
#include <QDir>
#include <QPixmap>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QVBoxLayout>
#include <exception>
#include <stdexcept>

SignatureManager::SignatureManager(SignatureStore &store, QWidget *parent) : QDialog(parent)
{
    const bool image = store.kind() == SignatureStore::Kind::Image;
    setObjectName(image ? QStringLiteral("signatureImagesDialog") : QStringLiteral("certificatesDialog"));
    setWindowTitle(image ? tr("Signature images") : tr("Signing certificates"));
    resize(550, 490);
    auto *layout = new QVBoxLayout(this);
    auto *hint = dialogHint(image
        ? tr("Import once, then use your signature in any session. Signature images are not digital signatures. "
             "Images used in a PDF are stored in its metadata and can be extracted by anyone with the PDF.")
        : tr("Import a PKCS#12 certificate (.p12/.pfx) containing your private key. The app stores its own copy, "
             "so use a password-protected file. Passwords are never saved and are requested each time you sign. "
             "Certificates are validated when signing."), this);
    layout->addWidget(hint);
    auto *list = new QListWidget(this);
    list->setObjectName(QStringLiteral("savedAssets"));
    layout->addWidget(list, 1);
    auto *preview = new QLabel(this);
    preview->setObjectName(QStringLiteral("signaturePreview"));
    preview->setAlignment(Qt::AlignCenter);
    preview->setMinimumHeight(image ? 85 : 0);
    preview->setVisible(image);
    layout->addWidget(preview);
    auto *row = new QHBoxLayout;
    auto *import = new QPushButton(tr("Import…"), this);
    import->setObjectName(QStringLiteral("importAsset"));
    auto *rename = new QPushButton(tr("Rename…"), this);
    auto *remove = new QPushButton(tr("Remove"), this);
    remove->setProperty("danger", true);
    row->addWidget(import);
    row->addWidget(rename);
    row->addWidget(remove);
    layout->addLayout(row);
    auto *location = new QLabel(tr("Stored in: %1\nChanges to this library are saved immediately. Removing an item does not change existing PDFs or the original file.").arg(store.directory()), this);
    location->setWordWrap(true);
    location->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(location);
    auto *buttons = dialogButtons(this, QDialogButtonBox::Close);
    auto *use = buttons->addButton(tr("Use selected"), QDialogButtonBox::AcceptRole);
    use->setObjectName(QStringLiteral("useAsset"));
    layout->addWidget(buttons);
    const auto report = [this](const std::exception &e) {
        QMessageBox::warning(this, tr("Signature library"), QString::fromUtf8(e.what()));
    };
    const auto refresh = [list, &store] {
        list->clear();
        const QString active = store.activeId();
        for (const auto &entry : store.entries()) {
            auto *item = new QListWidgetItem(entry.name, list);
            item->setData(Qt::UserRole, entry.id);
            item->setData(Qt::UserRole + 1, entry.name);
            if (entry.id == active) {
                item->setText(entry.name + tr(" (current)"));
                list->setCurrentItem(item);
            }
        }
    };
    connect(list, &QListWidget::currentItemChanged, this, [=, &store](QListWidgetItem *item) {
        use->setEnabled(item != nullptr);
        rename->setEnabled(item != nullptr);
        remove->setEnabled(item != nullptr);
        preview->clear();
        if (item && image) {
            // Preview without changing the persisted selection.
            const QString path = QDir(store.directory()).filePath(item->data(Qt::UserRole).toString() + QStringLiteral(".png"));
            preview->setPixmap(QPixmap(path).scaled(420, 85, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    });
    use->setEnabled(false);
    rename->setEnabled(false);
    remove->setEnabled(false);
    connect(use, &QPushButton::clicked, this, [=, &store] {
        if (!list->currentItem()) return;
        try { store.select(list->currentItem()->data(Qt::UserRole).toString()); accept(); }
        catch (const std::exception &e) { report(e); }
    });
    connect(import, &QPushButton::clicked, this, [=, &store] {
        const QString path = QFileDialog::getOpenFileName(this, image ? tr("Import signature image") : tr("Import signing certificate"), {},
            image ? tr("Images (*.png *.jpg *.jpeg *.bmp)") : tr("PKCS#12 certificates (*.p12 *.pfx)"));
        if (path.isEmpty()) return;
        try {
            QByteArray data;
            if (image) {
                QString error;
                const auto source = loadSignatureSource(path, &error);
                if (source.isNull()) throw std::runtime_error(error.toStdString());
                const auto processed = extractSignature(source);
                if (processed.isNull()) throw std::runtime_error("No dark ink was found on a light background.");
                QBuffer buffer(&data);
                buffer.open(QIODevice::WriteOnly);
                if (!processed.save(&buffer, "PNG")) throw std::runtime_error("Cannot encode the signature image.");
            } else {
                QFile file(path);
                if (!file.open(QIODevice::ReadOnly)) throw std::runtime_error(file.errorString().toStdString());
                if (file.size() > 5 * 1024 * 1024) throw std::runtime_error("The certificate exceeds the 5 MB limit.");
                data = file.readAll();
            }
            store.importAsset(QFileInfo(path).completeBaseName(), data);
            refresh();
        } catch (const std::exception &e) { report(e); }
    });
    connect(rename, &QPushButton::clicked, this, [=, &store] {
        if (!list->currentItem()) return;
        bool ok = false;
        const QString name = QInputDialog::getText(this, tr("Rename asset"), tr("Name:"), QLineEdit::Normal,
            list->currentItem()->data(Qt::UserRole + 1).toString(), &ok);
        if (!ok) return;
        try { store.rename(list->currentItem()->data(Qt::UserRole).toString(), name); refresh(); }
        catch (const std::exception &e) { report(e); }
    });
    connect(remove, &QPushButton::clicked, this, [=, &store] {
        if (!list->currentItem()) return;
        if (QMessageBox::question(this, tr("Remove saved asset"), tr("Remove this app's stored copy? This cannot be undone.")) != QMessageBox::Yes) return;
        try { store.remove(list->currentItem()->data(Qt::UserRole).toString()); refresh(); }
        catch (const std::exception &e) { report(e); }
    });
    try { refresh(); } catch (const std::exception &e) {
        // Avoid a nested message box during construction; leave the library read-only.
        hint->setText(tr("Cannot read the signature library: %1").arg(QString::fromUtf8(e.what())));
        import->setEnabled(false);
    }
}
