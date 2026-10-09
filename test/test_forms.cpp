#include "main_window.h"
#include "document_view.h"
#include "ui/form_dialog.h"
#include "jpdf_desk/printing/pdf_printing.h"

#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFile>
#include <QLabel>
#include <QLineEdit>
#include <QPrinter>
#include <QPushButton>
#include <QTemporaryDir>
#include <QTimer>

namespace {
void makeForm(const QString &path)
{
    const QList<QByteArray> objects = {
        "<< /Type /Catalog /Pages 2 0 R /AcroForm << /Fields [4 0 R 5 0 R 6 0 R 7 0 R 8 0 R 13 0 R] /DR << /Font << /Helv 11 0 R >> >> /DA (/Helv 12 Tf 0 g) >> >>",
        "<< /Type /Pages /Kids [3 0 R] /Count 1 >>",
        "<< /Type /Page /Parent 2 0 R /MediaBox [0 0 400 500] /Annots [4 0 R 5 0 R 6 0 R 7 0 R 8 0 R 9 0 R 10 0 R] >>",
        "<< /Type /Annot /Subtype /Widget /FT /Tx /T (Name) /Rect [20 430 200 450] /V (Before) /MaxLen 20 /F 4 >>",
        "<< /Type /Annot /Subtype /Widget /FT /Btn /T (Agree) /Rect [20 390 40 410] /V /Off /AS /Off /F 4 /AP << /N << /Off 12 0 R /Yes 14 0 R >> >> >>",
        "<< /Type /Annot /Subtype /Widget /FT /Ch /T (Country) /Ff 131072 /Rect [20 350 200 370] /Opt [[(SK) (Slovakia)] [(CZ) (Czechia)]] /V (SK) /I [0] /F 4 >>",
        "<< /Type /Annot /Subtype /Widget /FT /Tx /T (Locked) /Ff 1 /Rect [20 310 200 330] /V (Keep) /F 4 >>",
        "<< /Type /Annot /Subtype /Widget /FT /Ch /T (Multi) /Ff 2097152 /Rect [20 260 200 300] /Opt [(A) (B)] /V [(A)] /F 4 >>",
        "<< /Type /Annot /Subtype /Widget /Parent 13 0 R /Rect [20 200 200 220] /F 4 >>",
        "<< /Type /Annot /Subtype /Widget /Parent 13 0 R /Rect [20 160 200 180] /F 4 >>",
        "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>",
        "<< /Type /XObject /Subtype /Form /BBox [0 0 20 20] /Length 0 >>\nstream\n\nendstream",
        "<< /FT /Tx /T (Repeated) /Kids [9 0 R 10 0 R] /V (Same) >>",
        "<< /Type /XObject /Subtype /Form /BBox [0 0 20 20] /Length 18 >>\nstream\n0 0 20 20 re 0 g f\nendstream"
    };
    QByteArray data("%PDF-1.7\n");
    QList<int> offsets;
    for (int i = 0; i < objects.size(); ++i) {
        offsets.append(data.size());
        data += QByteArray::number(i + 1) + " 0 obj\n" + objects[i] + "\nendobj\n";
    }
    const int xref = data.size();
    data += "xref\n0 " + QByteArray::number(objects.size() + 1) + "\n0000000000 65535 f \n";
    for (int offset : offsets) data += QByteArray::number(offset).rightJustified(10, '0') + " 00000 n \n";
    data += "trailer\n<< /Size " + QByteArray::number(objects.size() + 1) + " /Root 1 0 R >>\nstartxref\n"
        + QByteArray::number(xref) + "\n%%EOF\n";
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly) || file.write(data) != data.size()) throw std::runtime_error("Fixture write failed");
}
QString value(const PdfDocument &doc, const QString &name)
{
    for (const auto &field : doc.formFields(0)) if (field.name == name) return field.value;
    return {};
}
}

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    QTemporaryDir dir;
    const QString source = dir.filePath("form.pdf");
    makeForm(source);
    PdfDocument doc;
    doc.open(source);
    const auto fields = doc.formFields(0);
    if (fields.size() != 6 || fields[0].maxLength != 20 || !fields[3].readOnly
        || fields[4].kind != PdfFormField::Kind::Unsupported || fields.last().id != 13) return 1;
    const QImage before = doc.render(0);
    doc.setMetadata({{"Title", "Unsaved title"}});
    doc.setFormValues({{4, "After"}, {5, "Yes"}, {6, "CZ"}, {13, "Together"}});
    if (value(doc, "Name") != "After" || value(doc, "Agree") != "Yes"
        || value(doc, "Country") != "CZ" || value(doc, "Repeated") != "Together"
        || before == doc.render(0) || doc.metadata().value("Title") != "Unsaved title") return 2;
    for (const auto &invalid : QList<QMap<int, QString>>{
            {{4, "Changed"}, {7, "Read-only"}}, {{4, QString(21, 'a')}}, {{5, "Invalid"}},
            {{6, "Invalid"}}, {{8, "B"}}, {{9999, "Invalid"}}, {{4, QString(QChar(0))}}}) {
        try { doc.setFormValues(invalid); return 3; } catch (const std::runtime_error &) {}
        if (value(doc, "Name") != "After") return 4;
    }
    DocumentAnnotations edits;
    edits.fields[0] = {{{20, 30, 100, 30}, "Annotation", 12}};
    doc.setPassword("secret");
    const QString saved = dir.filePath("saved.pdf");
    doc.saveSnapshot(saved, edits);
    PdfDocument reopened;
    reopened.open(saved, "secret");
    if (value(reopened, "Country") != "CZ" || value(reopened, "Agree") != "Yes"
        || value(reopened, "Locked") != "Keep" || reopened.fields(0).size() != 1) return 5;
    reopened.setFormValues({{4, ""}, {5, "Off"}, {6, ""}});
    if (!value(reopened, "Name").isEmpty() || value(reopened, "Agree") != "Off") return 6;
    QPrinter printer;
    printer.setOutputFormat(QPrinter::PdfFormat);
    printer.setOutputFileName(dir.filePath("print.pdf"));
    printDocumentSnapshot(reopened, {}, printer, 0);
    PdfDocument printed;
    printed.open(printer.outputFileName());
    if (printed.render(0).isNull() || printed.pageCount() != 1) return 7;
    reopened.saveSnapshot(dir.filePath("cleared.pdf"), {});
    doc.open(dir.filePath("cleared.pdf"), "secret");
    if (!value(doc, "Country").isEmpty() || value(doc, "Agree") != "Off") return 8;

    // Value-only dialog: cancel has no effect, errors stay inline, export values
    // differ from the labels displayed by the combo box.
    int calls = 0;
    FormDialog dialog(fields, 0, [&](const auto &) { ++calls; throw std::runtime_error("Rejected"); });
    if (!dialog.changes().isEmpty()) return 9;
    dialog.findChild<QLineEdit *>("formField_4")->setText("Dialog edit");
    dialog.findChild<QComboBox *>("formField_6")->setCurrentIndex(2);
    if (dialog.changes().value(6) != "CZ") return 10;
    dialog.show();
    dialog.findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
    if (calls != 1 || !dialog.isVisible() || dialog.findChild<QLabel *>("formError")->text() != "Rejected") return 11;
    dialog.reject();
    if (calls != 1) return 12;

    MainWindow window;
    auto *button = window.findChild<QPushButton *>("formButton");
    if (!button || button->isEnabled()) return 13;
    window.openDocument(source);
    auto *view = window.findChild<DocumentView *>();
    view->addText();
    bool edited = false;
    QTimer::singleShot(0, [&] {
        auto *editor = window.findChild<QDialog *>("formDialog");
        if (!editor) return;
        editor->findChild<QLineEdit *>("formField_4")->setText("UI edit");
        editor->findChild<QDialogButtonBox *>()->button(QDialogButtonBox::Ok)->click();
        edited = true;
    });
    button->click();
    if (!edited || view->captureDrafts().fields[0].size() != 1) return 14;
    return 0;
}
