#pragma once

#include "revocation.h"
#include <QHostAddress>

namespace jpdf::revocation {

// Conservative public-address allowlist, including IPv4-mapped IPv6 handling.
bool isPublicRevocationAddress(const QHostAddress &address);
// Explicit, exact-address exceptions for reviewed enterprise responders or local
// integration tests. GUI defaults never supply exceptions; redirects stay blocked.
struct HttpTransportOptions {
    QList<QHostAddress> approvedPrivateAddresses;
    QList<int> additionalPorts;
};
std::shared_ptr<Transport> makeHttpTransport(HttpTransportOptions options = {});

} // namespace jpdf::revocation
