// Verifies the visible generated branding inventory before trusting per-file size/hash records.
// Hidden metadata and hidden subtrees are excluded, matching the generator and qrc consumers.
// Paths must exactly match the normalized relative paths enumerated from both asset trees.
#pragma once
#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <stdexcept>
#include <algorithm>
#include <cmath>

namespace yalahoo::assets {
inline void validateBrandingManifest(const QDir &root, const QJsonDocument &document)
{
    using namespace Qt::StringLiterals;
    const auto require = [](bool valid) {
        if (!valid)
            throw std::runtime_error("Brand manifest inventory or file content invalid");
    };
    require(document.isObject());
    const auto manifest = document.object();
    require(manifest[u"schemaVersion"_s] == 1 && manifest[u"displayName"_s] == u"yalahOO!!!"_s
            && manifest[u"entries"_s].isArray());
    QSet<QString> expected;
    for (const auto &tree : { u"resources/branding"_s, u"resources/icons/yalahoo"_s }) {
        QDirIterator files(root.filePath(tree), QDir::Files, QDirIterator::Subdirectories);
        while (files.hasNext())
            expected.insert(root.relativeFilePath(files.next()));
    }
    const auto entries = manifest[u"entries"_s].toArray();
    require(!expected.isEmpty() && entries.size() == expected.size());
    // Removing each admitted path rejects duplicates, missing files, traversal and aliases.
    for (const auto &value : entries) {
        require(value.isObject());
        const auto entry = value.toObject();
        require(entry[u"path"_s].isString());
        const auto path = entry[u"path"_s].toString();
        require(expected.remove(path));
        QFile file(root.filePath(path));
        require(file.open(QIODevice::ReadOnly));
        const auto size = entry[u"bytes"_s];
        require(size.isDouble() && size.toDouble() >= 0 && size.toDouble() < 9007199254740992.
                && std::trunc(size.toDouble()) == size.toDouble() && file.size() == size.toInteger()
                && entry[u"sha256"_s].isString());
        QCryptographicHash hash(QCryptographicHash::Sha256);
        constexpr qint64 HashChunkBytes = 64 * 1024;
        qint64 remaining = size.toInteger();
        while (remaining > 0) {
            const auto bytes = file.read(std::min(remaining, HashChunkBytes));
            require(!bytes.isEmpty() && file.error() == QFileDevice::NoError);
            remaining -= bytes.size();
            hash.addData(bytes);
        }
        require(file.read(1).isEmpty() && file.error() == QFileDevice::NoError
                && QString::fromUtf8(hash.result().toHex()) == entry[u"sha256"_s].toString());
    }
    require(expected.isEmpty());
}
}
