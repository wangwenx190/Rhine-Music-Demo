#pragma once
#include <QVariantMap>

namespace yalahoo {
// Derive display fields without changing cached payload, provenance or query state.
QString metadataStatusText(const QString &status);
QVariantMap metadataPresentation(QVariantMap result, const QString &albumTitle);
}
