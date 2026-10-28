// Claims a fresh diagnostic directory before any captures or reports can replace old evidence.
#pragma once
#include <QDir>
#include <QFileInfo>
#include <QString>

namespace yalahoo::review {
inline bool prepareFreshOutput(const QString &output)
{
    if (output.isEmpty())
        return false;
    const QFileInfo target(QDir::cleanPath(QFileInfo(output).absoluteFilePath()));
    if (target.exists() || target.isSymLink() || target.fileName().isEmpty())
        return false;
    // mkdir claims only the final component; a concurrent claimant must fail as well.
    return QDir().mkpath(target.absolutePath())
            && QDir(target.absolutePath()).mkdir(target.fileName());
}
}
