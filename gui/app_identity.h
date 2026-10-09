#pragma once

#include <QString>

// Stable per-user asset root, independent of the Qt organization/display name.
QString applicationDataRoot();

// Select the current Qt identity.
void initializeApplicationIdentity();
