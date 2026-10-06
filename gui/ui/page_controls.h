#pragma once

#include <QFrame>

class SearchControls;
class QLabel;
class QPushButton;

class PageControls : public QFrame {
public:
    explicit PageControls(QWidget *parent = nullptr);
    void setDocumentState(int page, int count);
    QPushButton *sidebarToggleButton() const { return sidebarToggle_; }
    QPushButton *outlineToggleButton() const { return outlineToggle_; }
    QPushButton *previousButton() const { return previous_; }
    QPushButton *nextButton() const { return next_; }
    SearchControls *searchControls() const { return search_; }

private:
    QPushButton *sidebarToggle_;
    QPushButton *outlineToggle_;
    QPushButton *previous_;
    QPushButton *next_;
    QLabel *label_;
    SearchControls *search_;
};
