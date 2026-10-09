#include "jpdf_desk/document/detail/mupdf_call.h"
#include "jpdf_desk/document/pdf_document.h"

#include <mupdf/pdf.h>
#include <QCoreApplication>
#include <QFile>
#include <QTemporaryDir>
#include <iostream>

namespace {
template<typename Action>
bool fails(Action action)
{
    try { action(); }
    catch (const std::runtime_error &) { return true; }
    return false;
}
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir directory;
    if (!directory.isValid()) return 1;
    try {
        std::unique_ptr<fz_context, decltype(&fz_drop_context)> context(
            fz_new_context(nullptr, nullptr, FZ_STORE_DEFAULT), fz_drop_context);
        fz_context *ctx = context.get();
        if (!ctx) return 2;
        // Fail after acquisition, not just before a handle is returned. Both
        // MuPDF and C++ exceptions must unwind owners outside the C boundary.
        int drops = 0;
        for (int iteration = 0; iteration < 20; ++iteration) {
            if (!fails([&] {
                auto buffer = mupdf::own(ctx, mupdf::call(ctx, [&]() noexcept {
                    return fz_new_buffer(ctx, 32);
                }), [&](fz_context *c, fz_buffer *b) noexcept {
                    ++drops;
                    fz_drop_buffer(c, b);
                });
                mupdf::call(ctx, [&]() noexcept {
                    fz_throw(ctx, FZ_ERROR_FORMAT, "Injected MuPDF failure");
                });
            })) return 3;
            if (!fails([&] {
                auto buffer = mupdf::own(ctx, mupdf::call(ctx, [&]() noexcept {
                    return fz_new_buffer(ctx, 32);
                }), [&](fz_context *c, fz_buffer *b) noexcept {
                    ++drops;
                    fz_drop_buffer(c, b);
                });
                throw std::runtime_error("C++ conversion failure");
            })) return 4;
            if (drops != 2 * (iteration + 1)) return 5;
            if (mupdf::call(ctx, []() noexcept { return 42; }) != 42) return 6;
        }
        auto source = mupdf::own(ctx, mupdf::call(ctx, [&]() noexcept {
            return pdf_create_document(ctx);
        }), pdf_drop_document);
        auto page = mupdf::own(ctx, mupdf::call(ctx, [&]() noexcept {
            return pdf_add_page(ctx, source.get(), {0, 0, 300, 400}, 0, nullptr, nullptr);
        }), pdf_drop_obj);
        mupdf::call(ctx, [&]() noexcept { pdf_insert_page(ctx, source.get(), -1, page.get()); });
        const QString path = directory.filePath("input.pdf");
        const QByteArray name = QFile::encodeName(path);
        mupdf::call(ctx, [&]() noexcept {
            pdf_save_document(ctx, source.get(), name.constData(), &pdf_default_write_options);
        });
        PdfDocument document;
        document.open(path);
        for (int iteration = 0; iteration < 10; ++iteration) {
            for (int invalid : {-1, 99}) {
                if (!fails([&] { document.pageSize(invalid); }) ||
                    !fails([&] { document.render(invalid); }) ||
                    !fails([&] { document.renderForPrint(invalid, 72); }) ||
                    !fails([&] { document.fields(invalid); }) ||
                    !fails([&] { document.marks(invalid); }) ||
                    !fails([&] { document.signatures(invalid); })) return 7;
            }
            // Image decoding fails after the snapshot page and buffer exist.
            if (!fails([&] {
                document.snapshot({{}, {}, {{0, {{{10, 10, 40, 20}, "invalid PNG"}}}}});
            })) return 8;
            if (document.pageSize(0).isEmpty() || document.render(0).isNull() ||
                document.renderForPrint(0, 72).isNull() || !document.fields(0).isEmpty() ||
                !document.marks(0).isEmpty() || !document.signatures(0).isEmpty()) return 9;
        }
    } catch (const std::exception &error) {
        std::cerr << error.what() << '\n';
        return 10;
    }
    return 0;
}
