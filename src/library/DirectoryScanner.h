#pragma once

#include "MusicTypes.h"
#include <atomic>
#include <functional>

namespace yalahoo {
using ScanCancellation = std::shared_ptr<std::atomic_bool>;
struct ScanResult final
{
    Snapshot snapshot;
    QString error;
    bool cancelled = false;
    quint64 filesVisited = 0;
};

// All filesystem work runs on the store's single worker, never on GUI/render.
class DirectoryScanner final
{
public:
    static bool normalizeRoots(const QList<QUrl> &input, QList<QUrl> &output, QString &error);
    static QString stableId(const QString &prefix, const QString &absolutePath);
    static QString fingerprint(const QString &absolutePath, QString *error = nullptr);
    static ScanResult scan(const QList<QUrl> &roots, Snapshot previous,
                           const ScanCancellation &cancel);
};
}
