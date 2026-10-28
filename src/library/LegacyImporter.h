// Reads an explicitly selected legacy library into current value records. Import writes only the
// destination data root and preserves unrelated JSON fields.

#pragma once
#include "DirectoryScanner.h"
#include <functional>

namespace yalahoo {
class LegacyImporter final
{
public:
    // Explicit source directory only; reads index/config/rules and referenced artwork.
    using CoverCopy = std::function<QUrl(const QString &, const QString &, QString *)>;
    // A synchronous worker-side copier keeps cancellation observable between individual files.
    static ScanResult read(const QString &sourceRoot, const QString &dataRoot,
                           const ScanCancellation &cancel, const CoverCopy &copy = { });
};
}
