#include "PreferencesStore.h"
#include <QtConcurrentRun>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTimer>
#include <cmath>

namespace yalahoo {
PreferencesStore::PreferencesStore(QString dataRoot, QObject *parent)
    : QObject(parent), m_path(QDir(dataRoot).filePath(QStringLiteral("preferences.json")))
{
    m_worker.setMaxThreadCount(1);
    QFile file(m_path);
    if (file.exists()) {
        QJsonParseError parse;
        const bool opened = file.open(QIODevice::ReadOnly);
        const auto document = opened && file.size() <= 1024 * 1024
                ? QJsonDocument::fromJson(file.readAll(), &parse)
                : QJsonDocument();
        if (!opened || document.isNull() || !document.isObject()) {
            m_writable = false;
            m_error = QStringLiteral("偏好文件无法读取或已损坏，保留原文件。");
        } else {
            m_document = document.object();
            m_error = validate(m_document);
            m_writable = m_error.isEmpty();
            if (!m_writable)
                m_document = { };
        }
    }
    m_document.insert("schemaVersion", 1);
    connect(&m_save, &QFutureWatcher<QString>::finished, this, [this] {
        m_saving = false;
        const auto error = m_save.result();
        reportError(error);
        if (!error.isEmpty())
            m_dirty = false;
        else
            emit saved();
        schedule();
    });
}
PreferencesStore::~PreferencesStore()
{
    // Teardown only: finish the one in-flight save and commit the latest coalesced value.
    m_worker.waitForDone();
    if (m_dirty && m_writable)
        write(m_path, m_document);
}
bool PreferencesStore::rememberColumnPosition() const
{
    return m_document.value("rememberColumnPosition").toBool(true);
}
bool PreferencesStore::realistic() const
{
    return m_document.value("arrayMode").toString() == QLatin1String("realistic");
}
QJsonObject PreferencesStore::audio() const
{
    QJsonObject result;
    for (const auto &key : { "volume", "bgm", "bgmVolume", "sound", "soundVolume" })
        if (m_document.contains(key))
            result.insert(key, m_document.value(key));
    return result;
}
QVariantMap PreferencesStore::values() const
{
    QJsonObject values{ { "theme", "day" },        { "sortMode", "genre" },
                        { "arrayMode", "filled" }, { "rememberColumnPosition", true },
                        { "reduced", false },      { "motionSpeed", 1.0 },
                        { "volume", .65 },         { "bgm", true },
                        { "bgmVolume", .18 },      { "sound", true },
                        { "soundVolume", .22 } };
    for (auto it = m_document.begin(); it != m_document.end(); ++it)
        values.insert(it.key(), it.value());
    return values.toVariantMap();
}
QString PreferencesStore::validate(const QJsonObject &document)
{
    if (document.contains("schemaVersion") && document.value("schemaVersion") != 1)
        return QStringLiteral("偏好 schemaVersion 不受支持，保留原文件。");
    for (auto key : { "rememberColumnPosition", "reduced", "bgm", "sound" })
        if (document.contains(key) && !document.value(key).isBool())
            return QStringLiteral("偏好开关类型无效：") + key;
    const QList<QPair<QString, QStringList>> choices{
        { "theme", { "day", "night" } },
        { "quality", { "performance", "original", "high", "ultra", "custom" } },
        { "sortMode", { "genre", "artist", "album" } },
        { "arrayMode", { "filled", "realistic" } }
    };
    for (const auto &choice : choices)
        if (document.contains(choice.first)
            && (!document.value(choice.first).isString()
                || !choice.second.contains(document.value(choice.first).toString())))
            return QStringLiteral("偏好选项无效：") + choice.first;
    for (auto key : { "volume", "bgmVolume", "soundVolume", "motionSpeed" }) {
        const auto value = document.value(key);
        if (!value.isUndefined()
            && (!value.isDouble() || !std::isfinite(value.toDouble())
                || value.toDouble() < (QString(key) == "motionSpeed" ? .25 : 0)
                || value.toDouble() > (QString(key) == "motionSpeed" ? 3 : 1)))
            return QStringLiteral("偏好数值无效：") + key;
    }
    if (document.contains("renderQuality")) {
        if (!document.value("renderQuality").isObject())
            return QStringLiteral("画质偏好必须是对象。");
        const auto quality = document.value("renderQuality").toObject();
        for (auto key : { "scale", "depthOfField", "pixelRatio", "shadows", "aoSamples",
                          "aoResolution", "transmission", "anisotropy" })
            if (quality.contains(key)
                && (!quality.value(key).isDouble()
                    || !std::isfinite(quality.value(key).toDouble())))
                return QStringLiteral("画质数值类型无效：") + key;
        if (quality.contains("antialias") && quality.value("antialias") != "off"
            && quality.value("antialias") != "smaa")
            return QStringLiteral("抗锯齿选项无效。");
    }
    return { };
}
void PreferencesStore::reportError(const QString &error)
{
    if (m_error != error) {
        m_error = error;
        emit errorChanged();
    }
    if (!error.isEmpty())
        emit saveError(error);
}
bool PreferencesStore::update(const QVariantMap &patch)
{
    if (!m_writable) {
        emit saveError(m_error);
        return false;
    }
    const auto input = QJsonObject::fromVariantMap(patch);
    const auto error = validate(input);
    if (!error.isEmpty()) {
        reportError(error);
        return false;
    }
    bool modified = false;
    for (auto it = input.begin(); it != input.end(); ++it) {
        QJsonValue value = it.value();
        if (value.isObject() && m_document.value(it.key()).isObject()) {
            auto object = m_document.value(it.key()).toObject();
            const auto additions = value.toObject();
            for (auto field = additions.begin(); field != additions.end(); ++field)
                if (object.value(field.key()) != field.value())
                    object.insert(field.key(), field.value());
            value = object;
        }
        if (m_document.value(it.key()) != value) {
            m_document.insert(it.key(), value);
            modified = true;
        }
    }
    if (!modified)
        return true;
    ++m_revision;
    m_dirty = true;
    reportError({ });
    emit changed();
    schedule();
    return true;
}
void PreferencesStore::setAudio(const QJsonObject &audio)
{
    QVariantMap patch;
    for (auto key : { "volume", "bgm", "bgmVolume", "sound", "soundVolume" })
        if (audio.contains(key))
            patch.insert(key, audio.value(key).toVariant());
    update(patch);
}
QString PreferencesStore::error() const
{
    return m_error;
}
void PreferencesStore::setNavigation(bool remember, bool realistic)
{
    update({ { "rememberColumnPosition", remember },
             { "arrayMode", realistic ? "realistic" : "filled" } });
}
bool PreferencesStore::importJson(const QUrl &url)
{
    if (!m_writable || m_import.isRunning() || !url.isLocalFile()
        || !QDir::isAbsolutePath(url.toLocalFile()) || url.hasQuery() || url.hasFragment()) {
        emit saveError(QStringLiteral("偏好导入需要可写原生数据和一个本地绝对 JSON 路径。"));
        return false;
    }
    const auto revision = ++m_revision;
    disconnect(&m_import, nullptr, this, nullptr);
    connect(&m_import, &QFutureWatcher<QPair<QJsonObject, QString>>::finished, this,
            [this, revision] {
                if (revision != m_revision)
                    return;
                const auto result = m_import.result();
                if (!result.second.isEmpty()) {
                    reportError(result.second);
                    return;
                }
                if (update(result.first.toVariantMap()))
                    emit imported();
            });
    m_import.setFuture(QtConcurrent::run(&m_worker, [path = url.toLocalFile()] {
        using Result = QPair<QJsonObject, QString>;
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024)
            return Result{ { }, QStringLiteral("偏好 JSON 无法读取或超过 1MiB。") };
        const auto bytes = file.read(1024 * 1024 + 1);
        QJsonParseError parse;
        const auto document = QJsonDocument::fromJson(bytes, &parse);
        if (bytes.size() > 1024 * 1024 || file.error() != QFileDevice::NoError
            || parse.error != QJsonParseError::NoError || !document.isObject())
            return Result{ { }, QStringLiteral("偏好 JSON 无效，原文件已保留。") };
        auto object = document.object();
        const auto error = validate(object);
        if (!error.isEmpty())
            return Result{ { }, error };
        return Result{ object, { } };
    }));
    return true;
}
QString PreferencesStore::write(const QString &path, const QJsonObject &document)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return QStringLiteral("无法创建偏好目录。");
    if (QFileInfo(path).isSymLink())
        return QStringLiteral("原生偏好缓存不能通过符号链接覆盖其他文件。");
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    const auto bytes = QJsonDocument(document).toJson();
    if (bytes.size() > 1024 * 1024)
        return QStringLiteral("偏好超过 1MiB，原文件已保留。");
    if (!file.open(QIODevice::WriteOnly)
        || !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)
        || file.write(bytes) != bytes.size() || !file.commit())
        return QStringLiteral("无法保存偏好：") + file.errorString();
    return { };
}
void PreferencesStore::schedule()
{
    if (!m_dirty || m_saving || m_scheduled || !m_writable)
        return;
    m_scheduled = true;
    QTimer::singleShot(0, this, [this] {
        m_scheduled = false;
        if (!m_dirty || m_saving || !m_writable)
            return;
        m_dirty = false;
        m_saving = true;
        m_save.setFuture(
                QtConcurrent::run(&m_worker, &PreferencesStore::write, m_path, m_document));
    });
}
}
