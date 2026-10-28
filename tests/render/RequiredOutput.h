// Required normal output is part of verification success. Diagnostic-only output stays optional.
#pragma once
#include "Require.h"
#include <QImage>
#include <QDir>
#include <QFileInfo>
#include <QString>
using Qt::StringLiterals::operator""_s;

namespace yalahoo::review {
inline bool createFreshOutputDirectory(const QString &path)
{
    if (path.isEmpty())
        return false;
    const QFileInfo info(path);
    if (info.exists() || info.isSymLink() || !QDir().mkpath(info.absolutePath()))
        return false;
    return QDir(info.absolutePath()).mkdir(info.fileName());
}
inline void saveRequiredImage(const QImage &image, const QString &path)
{
    require(!image.isNull() && image.save(path),
            qPrintable(u"Cannot save required output: "_s + path));
}
}
