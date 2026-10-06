#pragma once

#include "pdf_filler/document/pdf_types.h"

class QGraphicsItem;
class QGraphicsScene;

// Scene/model conversion belongs here, rather than in the window controller.
bool isAnnotation(const QGraphicsItem *item);
// With a parent, capture only that page, in page-local coordinates.
PageAnnotations captureAnnotations(const QGraphicsScene &scene, const QGraphicsItem *parent = nullptr);
void addAnnotations(QGraphicsScene &scene, const PageAnnotations &annotations,
                    QGraphicsItem *parent = nullptr);
