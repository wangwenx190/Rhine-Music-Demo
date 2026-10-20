// Reads an explicitly selected legacy library into current value records. Import writes only the
// destination data root and preserves unrelated JSON fields.

#pragma once
#include "DirectoryScanner.h"

namespace yalahoo {
class LegacyImporter final
{
public:
    // Explicit source directory only; reads index/config/rules and referenced artwork.
    static ScanResult read(const QString &sourceRoot, const QString &dataRoot,
                           const ScanCancellation &cancel);
};
}
