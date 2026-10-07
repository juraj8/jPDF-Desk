#pragma once

#include <QString>

class QSettings;

// Select the current Qt identity and migrate settings/assets from older releases.
void initializeApplicationIdentity();

// Separate from identity setup so migration can be tested with isolated paths.
void migrateApplicationData(const QString &legacyRoot, const QString &currentRoot,
                            QSettings &legacySettings, QSettings &currentSettings);
