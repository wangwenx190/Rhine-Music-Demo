// Stages a complete asset batch on the destination filesystem. Publication keeps originals
// by rename until every replacement succeeds; failed rollback retains its recovery directory.
#pragma once
#include "GeometryAudit.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QTemporaryDir>
#include <QStringList>
#include <QList>
#include <utility>
#include <functional>
#include <stdexcept>

namespace yalahoo::assets {
using namespace Qt::StringLiterals;
class AssetTransaction final
{
    Q_DISABLE_COPY_MOVE(AssetTransaction)
public:
    explicit AssetTransaction(const QDir &root)
        : m_root(root), m_staging(root.filePath(u".asset-transaction-XXXXXX"_s))
    {
        const auto canonical = m_root.canonicalPath();
        if (canonical.isEmpty())
            throw std::runtime_error("Invalid asset repository root");
        m_root.setPath(canonical);
        if (!m_staging.isValid())
            throw std::runtime_error("Cannot create asset staging directory");
    }
    QString importDirectory() const
    {
        return m_staging.filePath(u"import"_s);
    }
    void stage(const QString &relative, const QByteArray &bytes)
    {
        if (relative.isEmpty() || relative == "."_L1 || relative == ".."_L1
            || QDir::isAbsolutePath(relative) || relative != QDir::cleanPath(relative)
            || relative.startsWith(u"../"_s) || m_paths.contains(relative))
            throw std::runtime_error("Invalid or duplicate staged asset path");
        saveFile(m_staging.filePath(u"new/"_s + relative), bytes);
        m_paths.append(relative);
    }
    using Rename = std::function<bool(const QString &, const QString &)>;
    void publish(const Rename &rename = [](const QString &from, const QString &to) {
        return QFile::rename(from, to);
    })
    {
        struct Replacement final
        {
            QString destination, backup;
            bool installed = false;
        };
        QList<Replacement> replacements;
        try {
            for (const auto &relative : std::as_const(m_paths))
                validateDestination(relative);
            for (const auto &relative : std::as_const(m_paths)) {
                validateDestination(relative);
                const auto destination = m_root.filePath(relative);
                const QFileInfo existing(destination);
                if (existing.isSymLink() || (existing.exists() && !existing.isFile()))
                    throw std::runtime_error("Asset destination is not a regular file");
                const auto backup = m_staging.filePath(u"old/"_s + relative);
                if (!QDir().mkpath(QFileInfo(destination).absolutePath())
                    || !QDir().mkpath(QFileInfo(backup).absolutePath()))
                    throw std::runtime_error("Cannot prepare asset publication directories");
                validateDestination(relative);
                replacements.append({ destination, { }, false });
                auto &replacement = replacements.last();
                if (existing.exists()) {
                    if (!rename(destination, backup))
                        throw std::runtime_error("Cannot retain original asset");
                    replacement.backup = backup;
                }
                if (!rename(m_staging.filePath(u"new/"_s + relative), destination))
                    throw std::runtime_error("Cannot publish staged asset");
                replacement.installed = true;
            }
        } catch (...) {
            bool restored = true;
            for (auto it = replacements.rbegin(); it != replacements.rend(); ++it) {
                if (it->installed && !QFile::remove(it->destination))
                    restored = false;
                if (!it->backup.isEmpty() && !QFile::rename(it->backup, it->destination))
                    restored = false;
            }
            if (!restored) {
                m_staging.setAutoRemove(false);
                throw std::runtime_error((u"Asset rollback incomplete; retained recovery data: "_s
                                          + m_staging.path())
                                                 .toStdString());
            }
            throw;
        }
    }

private:
    void validateDestination(const QString &relative) const
    {
        QString current = m_root.absolutePath();
        const auto components = relative.split('/'_L1);
        for (qsizetype index = 0; index < components.size(); ++index) {
            current = QDir(current).filePath(components[index]);
            const QFileInfo info(current);
            if (info.isSymLink() || info.isJunction())
                throw std::runtime_error("Asset destination has a symbolic link component");
            if (!info.exists())
                continue;
            const auto canonical = info.canonicalFilePath();
            if (!canonical.startsWith(m_root.absolutePath() + '/'_L1)
                || (index + 1 < components.size() && !info.isDir()))
                throw std::runtime_error(
                        "Asset destination escapes the repository or is not a directory");
        }
    }
    QDir m_root;
    QTemporaryDir m_staging;
    QStringList m_paths;
};
}
