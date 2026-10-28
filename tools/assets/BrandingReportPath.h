// Keeps diagnostic writes outside generated asset trees, resolving existing symlink ancestors
// before generation creates the destination. The same check protects failure reports.
#pragma once
#include <QDir>
#include <QFileInfo>
#include <QStringList>

namespace yalahoo::assets {
inline bool brandingReportPathAllowed(const QDir &root, const QString &report)
{
    using namespace Qt::StringLiterals;
    QString current = QFileInfo(report).absoluteFilePath();
    QStringList suffix;
    for (;;) {
        const QFileInfo info(current);
        const auto canonical = info.canonicalFilePath();
        if (!canonical.isEmpty()) {
            if (!suffix.isEmpty() && !info.isDir())
                return false;
            current = canonical;
            break;
        }
        if (info.isSymLink() || info.isJunction())
            return false;
        suffix.prepend(info.fileName());
        const auto parent = info.absolutePath();
        if (parent == current)
            return false;
        current = parent;
    }
    for (const auto &part : suffix)
        current = QDir(current).filePath(part);
    current = QDir::cleanPath(current);
    const auto canonicalRoot = root.canonicalPath();
    if (canonicalRoot.isEmpty())
        return false;
    // Protect future paths on case-insensitive volumes before canonicalization is possible.
    constexpr auto sensitivity = Qt::CaseInsensitive;
    const QDir resolvedRoot(canonicalRoot);
    for (const auto &tree : { u"resources"_s, u"docs/media/branding"_s }) {
        const auto protectedPath = resolvedRoot.filePath(tree);
        if (current.compare(protectedPath, sensitivity) == 0
            || current.startsWith(protectedPath + '/'_L1, sensitivity))
            return false;
    }
    return !current.startsWith(resolvedRoot.filePath(u".asset-transaction-"_s), sensitivity);
}
}
