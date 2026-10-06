#include "ui/page_annotations.h"
#include "ui/editable_text.h"
#include "ui/mark_item.h"
#include "ui/signature_item.h"

#include <QGraphicsScene>
#include <QTextDocument>

bool isAnnotation(const QGraphicsItem *item)
{
    return item && (item->type() == EditableText::Type || item->type() == MarkItem::Type ||
                    item->type() == SignatureItem::Type);
}

PageAnnotations captureAnnotations(const QGraphicsScene &scene, const QGraphicsItem *parent)
{
    PageAnnotations annotations;
    const auto items = parent ? parent->childItems() : scene.items();
    for (const QGraphicsItem *item : items) {
        if (const auto *text = qgraphicsitem_cast<const EditableText *>(item)) {
            annotations.fields.append({QRectF(text->pos(), QSizeF(text->textWidth(),
                qMax(30.0, text->document()->size().height()))), text->toPlainText(), text->fontSize()});
        } else if (const auto *mark = qgraphicsitem_cast<const MarkItem *>(item)) {
            annotations.marks.append(mark->mark());
        } else if (const auto *signature = qgraphicsitem_cast<const SignatureItem *>(item)) {
            annotations.signatures.append(signature->signature());
        }
    }
    return annotations;
}

void addAnnotations(QGraphicsScene &scene, const PageAnnotations &annotations, QGraphicsItem *parent)
{
    const auto add = [&scene, parent](QGraphicsItem *item) {
        if (parent) item->setParentItem(parent);
        else scene.addItem(item);
    };
    for (const auto &field : annotations.fields) add(new EditableText(field));
    for (const auto &mark : annotations.marks) add(new MarkItem(mark));
    for (const auto &signature : annotations.signatures) add(new SignatureItem(signature));
}
