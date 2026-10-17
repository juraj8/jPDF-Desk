#pragma once

#include "jpdf_desk/document/pdf_types.h"
#include "jpdf_desk/crypto/validation_time.h"
#include <memory>

class QWidget;
class PdfTaskProgress;
namespace jpdf::revocation { class Transport; }

// Worker-only: DER copies and validated chains, never a live PDF or GUI widget.
void checkSignatureRevocation(QVector<DigitalSignatureStatus> &signatures,
                              const std::shared_ptr<jpdf::revocation::Transport> &transport,
                              const PdfTaskProgress &progress,
                              const jpdf::crypto::ValidationTime &time = {});

// GUI-only wrapper: joins its worker; cancellation leaves the input unchanged.
bool runOnlineSignatureRevocation(QWidget *parent, QVector<DigitalSignatureStatus> &signatures,
                                 std::shared_ptr<jpdf::revocation::Transport> transport = {});
