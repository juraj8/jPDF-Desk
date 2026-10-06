#pragma once

#include <QFrame>

class QLineEdit;
class QLabel;
class QPushButton;

class SearchControls : public QFrame {
public:
    explicit SearchControls(QWidget *parent = nullptr);
    QLineEdit *queryControl() const { return query_; }
    QPushButton *previousButton() const { return previous_; }
    QPushButton *nextButton() const { return next_; }
    void setDocumentAvailable(bool available);
    // index is zero-based; count -1 means idle, 0 means no matches.
    void setResultState(int index, int count);

private:
    QLineEdit *query_;
    QLabel *count_;
    QPushButton *previous_;
    QPushButton *next_;
};
