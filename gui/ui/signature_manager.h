#pragma once

#include <QDialog>

class SignatureStore;

// Imports/removals are immediate; accepting selects an asset for future use.
class SignatureManager : public QDialog {
public:
    explicit SignatureManager(SignatureStore &store, QWidget *parent = nullptr);
};
