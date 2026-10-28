// Classifies canonical Windows module paths by package/system boundaries. Qt and media
// dependencies must come from the package even if a copy exists under the Windows directory.
#pragma once
#include <QDir>
#include <QFileInfo>

namespace yalahoo::deployment {
inline bool windowsImageUnder(const QString &path, const QString &root)
{
    if (path.isEmpty() || root.isEmpty())
        return false;
    const auto normalized = QDir::cleanPath(QDir::fromNativeSeparators(path));
    const auto directory = QDir::cleanPath(QDir::fromNativeSeparators(root));
    return normalized.startsWith(directory + u'/', Qt::CaseInsensitive);
}
inline bool windowsSystemImage(const QString &path, const QString &windows)
{
    using namespace Qt::StringLiterals;
    const auto name = QFileInfo(QDir::fromNativeSeparators(path)).fileName().toLower();
    for (const auto prefix : { "qt6"_L1, "avcodec-"_L1, "avformat-"_L1, "avutil-"_L1,
                               "swresample-"_L1, "swscale-"_L1, "avfilter-"_L1, "avdevice-"_L1 })
        if (name.startsWith(prefix))
            return false;
    return windowsImageUnder(path, windows + u"/System32"_s)
            || windowsImageUnder(path, windows + u"/SysWOW64"_s)
            || windowsImageUnder(path, windows + u"/WinSxS"_s);
}
}
