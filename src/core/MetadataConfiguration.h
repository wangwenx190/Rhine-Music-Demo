// Validates the shared metadata proxy protocol before persistence or network use.

#pragma once
#include <QUrl>
#include <QString>

namespace yalahoo::metadataConfiguration {
inline constexpr int MaximumContactCharacters = 500;
inline constexpr int DefaultHttpProxyPort = 8080;
inline constexpr int MaximumTcpPort = 65535;
inline bool validProxy(const QUrl &proxy)
{
    using namespace Qt::StringLiterals;
    return proxy.isValid() && proxy.scheme() == "http"_L1 && !proxy.host().isEmpty()
            && proxy.userInfo().isEmpty() && !proxy.hasQuery() && !proxy.hasFragment()
            && proxy.port(DefaultHttpProxyPort) >= 1
            && proxy.port(DefaultHttpProxyPort) <= MaximumTcpPort
            && (proxy.path().isEmpty() || proxy.path() == "/"_L1);
}
}
