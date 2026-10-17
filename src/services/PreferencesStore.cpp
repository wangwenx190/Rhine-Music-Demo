#include <QCoreApplication>
#include "PreferencesStore.h"
#include "Logging.h"
#include <QtConcurrentRun>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTimer>
#include <cmath>

using namespace Qt::StringLiterals;

namespace yalahoo {
PreferencesStore::PreferencesStore(QString dataRoot, QObject *parent)
    : QObject(parent), m_path(QDir(dataRoot).filePath(u"preferences.json"_s))
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
            m_error = QCoreApplication::translate(
                    "Yalahoo",
                    "Cannot read preferences, or the file is damaged. The original file was preserved.");
        } else {
            m_document = document.object();
            m_error = validate(m_document);
            m_writable = m_error.isEmpty();
            if (!m_writable)
                m_document = { };
        }
    }
    if (!m_error.isEmpty())
        qCWarning(logSettings) << "Preferences load failed" << "path=" << m_path
                               << "error=" << m_error;
    else
        qCInfo(logSettings) << "Preferences loaded" << "path=" << m_path;
    m_document.insert(u"schemaVersion"_s, 1);
    connect(&m_save, &QFutureWatcher<QString>::finished, this, [this] {
        m_saving = false;
        const auto error = m_save.result();
        reportError(error);
        if (!error.isEmpty())
            m_dirty = false;
        else {
            qCInfo(logSettings) << "Preferences saved" << "path=" << m_path;
            Q_EMIT saved();
        }
        schedule();
    });
}
PreferencesStore::~PreferencesStore()
{
    // Teardown only: finish the one in-flight save and commit the latest coalesced value.
    m_worker.waitForDone();
    if (m_dirty && m_writable) {
        const auto error = write(m_path, m_document);
        if (!error.isEmpty())
            qCWarning(logSettings)
                    << "Preferences shutdown save failed" << "path=" << m_path << "error=" << error;
    }
}
bool PreferencesStore::rememberColumnPosition() const
{
    return m_document.value(u"rememberColumnPosition"_s).toBool(true);
}
bool PreferencesStore::realistic() const
{
    return m_document.value(u"arrayMode"_s).toString() == "realistic"_L1;
}
QJsonObject PreferencesStore::audio() const
{
    QJsonObject result;
    for (const auto &key : { u"volume"_s, u"bgm"_s, u"bgmVolume"_s, u"sound"_s, u"soundVolume"_s })
        if (m_document.contains(key))
            result.insert(key, m_document.value(key));
    return result;
}
QVariantMap PreferencesStore::values() const
{
    QJsonObject values{ { u"theme"_s, u"day"_s },
                        { u"sortMode"_s, u"genre"_s },
                        { u"arrayMode"_s, u"filled"_s },
                        { u"rememberColumnPosition"_s, true },
                        { u"reduced"_s, false },
                        { u"motionSpeed"_s, 1.0 },
                        { u"volume"_s, .65 },
                        { u"bgm"_s, true },
                        { u"bgmVolume"_s, .18 },
                        { u"sound"_s, true },
                        { u"soundVolume"_s, .22 } };
    for (auto it = m_document.begin(); it != m_document.end(); ++it)
        values.insert(it.key(), it.value());
    return values.toVariantMap();
}
QString PreferencesStore::validate(const QJsonObject &document)
{
    if (document.contains(u"schemaVersion"_s) && document.value(u"schemaVersion"_s) != 1)
        return QCoreApplication::translate(
                "Yalahoo",
                "Unsupported preferences schemaVersion. The original file was preserved.");
    for (auto key : { u"rememberColumnPosition"_s, u"reduced"_s, u"bgm"_s, u"sound"_s })
        if (document.contains(key) && !document.value(key).isBool())
            return QCoreApplication::translate("Yalahoo", "Invalid preference switch type: ") + key;
    const QList<QPair<QString, QStringList>> choices{
        { u"theme"_s, { u"day"_s, u"night"_s } },
        { u"quality"_s, { u"performance"_s, u"original"_s, u"high"_s, u"ultra"_s, u"custom"_s } },
        { u"sortMode"_s, { u"genre"_s, u"artist"_s, u"album"_s } },
        { u"arrayMode"_s, { u"filled"_s, u"realistic"_s } }
    };
    for (const auto &choice : choices)
        if (document.contains(choice.first)
            && (!document.value(choice.first).isString()
                || !choice.second.contains(document.value(choice.first).toString())))
            return QCoreApplication::translate("Yalahoo", "Invalid preference option: ")
                    + choice.first;
    for (auto key : { u"volume"_s, u"bgmVolume"_s, u"soundVolume"_s, u"motionSpeed"_s }) {
        const auto value = document.value(key);
        if (!value.isUndefined()
            && (!value.isDouble() || !std::isfinite(value.toDouble())
                || value.toDouble() < (QString(key) == "motionSpeed"_L1 ? .25 : 0)
                || value.toDouble() > (QString(key) == "motionSpeed"_L1 ? 3 : 1)))
            return QCoreApplication::translate("Yalahoo", "Invalid preference number: ") + key;
    }
    if (document.contains(u"lighting"_s)) {
        if (!document.value(u"lighting"_s).isObject())
            return QCoreApplication::translate("Yalahoo",
                                               "Lighting preferences must be an object.");
        const auto lighting = document.value(u"lighting"_s).toObject();
        const QStringList modes{ u"baseline"_s, u"area"_s, u"guided"_s, u"hybrid"_s };
        if (lighting.contains(u"mode"_s) && !modes.contains(lighting.value(u"mode"_s).toString()))
            return QCoreApplication::translate("Yalahoo", "Invalid lighting mode.");
        for (auto key :
             { u"strength"_s, u"width"_s, u"offset"_s, u"fill"_s, u"transitionDuration"_s })
            if (lighting.contains(key)
                && (!lighting.value(key).isDouble()
                    || !std::isfinite(lighting.value(key).toDouble())))
                return QCoreApplication::translate("Yalahoo", "Invalid lighting value type: ")
                        + key;
    }
    if (document.contains(u"renderQuality"_s)) {
        if (!document.value(u"renderQuality"_s).isObject())
            return QCoreApplication::translate("Yalahoo", "Quality preferences must be an object.");
        const auto quality = document.value(u"renderQuality"_s).toObject();
        for (auto key : { u"scale"_s, u"depthOfField"_s, u"pixelRatio"_s, u"shadows"_s,
                          u"aoSamples"_s, u"aoResolution"_s, u"transmission"_s, u"anisotropy"_s })
            if (quality.contains(key)
                && (!quality.value(key).isDouble()
                    || !std::isfinite(quality.value(key).toDouble())))
                return QCoreApplication::translate("Yalahoo", "Invalid quality value type: ") + key;
        if (quality.contains(u"antialias"_s) && quality.value(u"antialias"_s) != u"off"_s
            && quality.value(u"antialias"_s) != u"smaa"_s)
            return QCoreApplication::translate("Yalahoo", "Invalid antialiasing option.");
    }
    return { };
}
void PreferencesStore::reportError(const QString &error)
{
    if (m_error != error) {
        m_error = error;
        Q_EMIT errorChanged();
    }
    if (!error.isEmpty()) {
        qCWarning(logSettings) << "Preferences operation failed" << "path=" << m_path
                               << "error=" << error;
        Q_EMIT saveError(error);
    }
}
bool PreferencesStore::update(const QVariantMap &patch)
{
    if (!m_writable) {
        Q_EMIT saveError(m_error);
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
    Q_EMIT changed();
    schedule();
    return true;
}
void PreferencesStore::setAudio(const QJsonObject &audio)
{
    QVariantMap patch;
    for (auto key : { u"volume"_s, u"bgm"_s, u"bgmVolume"_s, u"sound"_s, u"soundVolume"_s })
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
    update({ { u"rememberColumnPosition"_s, remember },
             { u"arrayMode"_s, realistic ? u"realistic"_s : u"filled"_s } });
}
bool PreferencesStore::importJson(const QUrl &url)
{
    if (!m_writable || m_import.isRunning() || !url.isLocalFile()
        || !QDir::isAbsolutePath(url.toLocalFile()) || url.hasQuery() || url.hasFragment()) {
        Q_EMIT saveError(QCoreApplication::translate(
                "Yalahoo",
                "Preference import requires writable native data and a local absolute JSON path."));
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
                    Q_EMIT imported();
            });
    m_import.setFuture(QtConcurrent::run(&m_worker, [path = url.toLocalFile()] {
        using Result = QPair<QJsonObject, QString>;
        QFile file(path);
        if (!file.open(QIODevice::ReadOnly) || file.size() > 1024 * 1024)
            return Result{ { },
                           QCoreApplication::translate(
                                   "Yalahoo",
                                   "Cannot read preference JSON, or it exceeds 1 MiB.") };
        const auto bytes = file.read(1024 * 1024 + 1);
        QJsonParseError parse;
        const auto document = QJsonDocument::fromJson(bytes, &parse);
        if (bytes.size() > 1024 * 1024 || file.error() != QFileDevice::NoError
            || parse.error != QJsonParseError::NoError || !document.isObject())
            return Result{ { },
                           QCoreApplication::translate(
                                   "Yalahoo",
                                   "Invalid preference JSON. The original file was preserved.") };
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
        return QCoreApplication::translate("Yalahoo", "Cannot create the preferences directory.");
    if (QFileInfo(path).isSymLink())
        return QCoreApplication::translate(
                "Yalahoo",
                "The native preferences cache cannot overwrite other files through symbolic links.");
    QSaveFile file(path);
    file.setDirectWriteFallback(false);
    const auto bytes = QJsonDocument(document).toJson();
    if (bytes.size() > 1024 * 1024)
        return QCoreApplication::translate(
                "Yalahoo", "Preferences exceed 1 MiB. The original file was preserved.");
    if (!file.open(QIODevice::WriteOnly)
        || !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)
        || file.write(bytes) != bytes.size() || !file.commit())
        return QCoreApplication::translate("Yalahoo", "Cannot save preferences: ")
                + file.errorString();
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
