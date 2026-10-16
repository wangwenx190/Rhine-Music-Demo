#pragma once
#include "DirectoryScanner.h"

namespace yalahoo {
class LegacyImporter
{
public:
    // Explicit source directory only; reads index/config/rules and referenced artwork.
    static ScanResult read(const QString &sourceRoot, const QString &dataRoot,
                           const ScanCancellation &cancel);
};
}
