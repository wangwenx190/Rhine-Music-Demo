// Verifies the complete branding file inventory before trusting per-file size/hash records.
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
        const auto bytes = file.readAll();
        require(file.error() == QFileDevice::NoError && entry[u"bytes"_s].isDouble()
                && bytes.size() == entry[u"bytes"_s].toDouble() && entry[u"sha256"_s].isString()
                && QString::fromUtf8(
                           QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex())
                        == entry[u"sha256"_s].toString());
    }
    require(expected.isEmpty());
}
}
