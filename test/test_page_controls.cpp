#include "ui/page_controls.h"
#include "ui/search_controls.h"

#include <QApplication>
#include <QLabel>
#include <QPushButton>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    PageControls controls;
    controls.setDocumentState(4, 20);
    controls.searchControls()->setResultState(99, 100);
    auto *label = controls.findChild<QLabel *>("pageLabel");
    if (!label) return 1;
    controls.resize(1400, 60);
    controls.show();
    app.processEvents();
    const auto centered = [&] {
        return qAbs(label->geometry().center().x() - controls.rect().center().x()) <= 1;
    };
    if (!centered()) return 2;
    controls.resize(controls.minimumSizeHint().width(), 60);
    app.processEvents();
    if (controls.previousButton()->geometry().left() <= controls.sidebarToggleButton()->geometry().right()
        || controls.nextButton()->geometry().right() >= controls.searchControls()->geometry().left()) return 3;
    // At the minimum width, search occupies the space needed for exact centering.
    if (centered()) return 4;
    controls.resize(1400, 60);
    app.processEvents();
    if (!centered()) return 5;
    controls.searchControls()->setResultState(-1, -1);
    controls.setDocumentState(0, 0);
    app.processEvents();
    if (!centered()) return 6;
    return 0;
}
