// Reads external input with a hard byte ceiling, including files that grow after stat.
#pragma once
#include <QFile>
#include <optional>
#include <limits>

namespace yalahoo {
inline std::optional<QByteArray> readBoundedFile(const QString &path, qint64 limit)
{
    QFile file(path);
    if (limit < 0 || limit == (std::numeric_limits<qint64>::max)()
        || !file.open(QIODevice::ReadOnly) || file.size() > limit)
        return std::nullopt;
    const auto bytes = file.read(limit + 1);
    if (bytes.size() > limit || file.error() != QFileDevice::NoError)
        return std::nullopt;
    return bytes;
}
}
