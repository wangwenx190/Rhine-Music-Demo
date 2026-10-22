#include "ProductDefaults.h"
#include "Units.h"
#include <QCoreApplication>
#include "PreferencesStore.h"
#include <QThreadPool>
#include <QFutureWatcher>
#include "Logging.h"
#include <QtConcurrentRun>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>
#include <QTimer>
#include <cmath>
#include <optional>

using namespace Qt::StringLiterals;

namespace yalahoo {
struct PreferencesStore::Private final
{
    Q_DISABLE_COPY_MOVE(Private)
    Private() = default;
    QString m_path, m_error;
    QJsonObject m_document;
    mutable std::optional<QVariantMap> m_values;
    QThreadPool m_worker;
    QFutureWatcher<QString> m_save;
    QFutureWatcher<ImportResult> m_import;
    quint64 m_revision = 0;
    bool m_writable = true, m_dirty = false, m_saving = false, m_scheduled = false;
};
void PreferencesStore::load()
{
    if (!QFileInfo::exists(d->m_path))
        return;
    const auto result = readPreferenceImport(d->m_path);
    d->m_error = result.error;
    d->m_writable = d->m_error.isEmpty();
    if (d->m_writable)
        d->m_document = result.document;
}
PreferencesStore::PreferencesStore(const QString &dataRoot, QObject *parent)
    : QObject(parent), d(std::make_unique<Private>())
{
    d->m_path = QDir(dataRoot).filePath(u"preferences.json"_s);
    d->m_worker.setMaxThreadCount(1);
    load();

    if (!d->m_error.isEmpty())
        qCWarning(logSettings) << "Preferences load failed" << "path=" << d->m_path
                               << "error=" << d->m_error;
    else
        qCInfo(logSettings) << "Preferences loaded" << "path=" << d->m_path;
    d->m_document.insert(u"schemaVersion"_s, 1);
    connect(&d->m_save, &QFutureWatcher<QString>::finished, this, [this] {
        d->m_saving = false;
        const auto error = d->m_save.result();
        reportError(error);
        if (!error.isEmpty())
            d->m_dirty = false;
        else {
            qCInfo(logSettings) << "Preferences saved" << "path=" << d->m_path;
            Q_EMIT saved();
        }
        schedule();
    });
}
PreferencesStore::~PreferencesStore()
{
    // Teardown only: finish the one in-flight save and commit the latest coalesced value.
    d->m_worker.waitForDone();
    if (d->m_dirty && d->m_writable) {
        const auto error = write(d->m_path, d->m_document);
        if (!error.isEmpty())
            qCWarning(logSettings) << "Preferences shutdown save failed" << "path=" << d->m_path
                                   << "error=" << error;
    }
}
bool PreferencesStore::rememberColumnPosition() const
{
    return d->m_document.value(u"rememberColumnPosition"_s).toBool(true);
}
bool PreferencesStore::realistic() const
{
    return d->m_document.value(u"arrayMode"_s).toString() == "realistic"_L1;
}
QJsonObject PreferencesStore::audio() const
{
    QJsonObject result;
    for (const auto &key : { u"volume"_s, u"bgm"_s, u"bgmVolume"_s, u"sound"_s, u"soundVolume"_s })
        if (d->m_document.contains(key))
            result.insert(key, d->m_document.value(key));
    return result;
}
QVariantMap PreferencesStore::values() const
{
    if (d->m_values)
        return *d->m_values;
    static const QJsonObject defaults{
        { u"theme"_s, u"day"_s },
        { u"sortMode"_s, u"genre"_s },
        { u"arrayMode"_s, u"filled"_s },
        { u"rememberColumnPosition"_s, true },
        { u"reduced"_s, false },
        { u"singleInstance"_s, true },
        { u"motionSpeed"_s, 1.0 },
        { u"volume"_s, yalahoo::productDefaults::DefaultSongVolume },
        { u"bgm"_s, true },
        { u"bgmVolume"_s, yalahoo::productDefaults::DefaultBackgroundVolume },
        { u"sound"_s, true },
        { u"soundVolume"_s, yalahoo::productDefaults::DefaultFeedbackVolume }
    };
    auto values = defaults;
    for (auto it = d->m_document.constBegin(); it != d->m_document.constEnd(); ++it)
        values.insert(it.key(), it.value());
    d->m_values = values.toVariantMap();
    return *d->m_values;
}
QString PreferencesStore::validate(const QJsonObject &document)
{
    if (document.contains(u"schemaVersion"_s) && document.value(u"schemaVersion"_s) != 1)
        return QCoreApplication::translate(
                "Yalahoo",
                "Unsupported preferences schemaVersion. The original file was preserved.");
    for (const auto &key :
         { u"rememberColumnPosition"_s, u"reduced"_s, u"singleInstance"_s, u"bgm"_s, u"sound"_s })
        if (document.contains(key) && !document.value(key).isBool())
            return QCoreApplication::translate("Yalahoo", "Invalid preference switch type: ") + key;
    struct PreferenceChoice final
    {
        QString key;
        QStringList options;
    };
    static const QList<PreferenceChoice> choices{
        { u"theme"_s, { u"day"_s, u"night"_s } },
        { u"sortMode"_s, { u"genre"_s, u"artist"_s, u"album"_s } },
        { u"arrayMode"_s, { u"filled"_s, u"realistic"_s } }
    };
    for (const auto &choice : choices)
        if (document.contains(choice.key)
            && (!document.value(choice.key).isString()
                || !choice.options.contains(document.value(choice.key).toString())))
            return QCoreApplication::translate("Yalahoo", "Invalid preference option: ")
                    + choice.key;
    for (const auto &key : { u"volume"_s, u"bgmVolume"_s, u"soundVolume"_s, u"motionSpeed"_s }) {
        const auto value = document.value(key);
        if (!value.isUndefined()
            && (!value.isDouble() || !std::isfinite(value.toDouble())
                || value.toDouble() < (key == "motionSpeed"_L1
                                               ? yalahoo::productDefaults::MinimumMotionSpeed
                                               : 0)
                || value.toDouble() > (key == "motionSpeed"_L1
                                               ? yalahoo::productDefaults::MaximumMotionSpeed
                                               : 1)))
            return QCoreApplication::translate("Yalahoo", "Invalid preference number: ") + key;
    }
    if (document.contains(u"lighting"_s)) {
        if (!document.value(u"lighting"_s).isObject())
            return QCoreApplication::translate("Yalahoo",
                                               "Lighting preferences must be an object.");
        const auto lighting = document.value(u"lighting"_s).toObject();
        const auto &modes = productDefaults::lightingModes();
        if (lighting.contains(u"mode"_s) && !modes.contains(lighting.value(u"mode"_s).toString()))
            return QCoreApplication::translate("Yalahoo", "Invalid lighting mode.");
        for (const auto &key :
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
        const auto amount = quality.value(u"depthOfField"_s);
        if (!amount.isUndefined()
            && (!amount.isDouble() || !std::isfinite(amount.toDouble()) || amount.toDouble() < 0))
            return QCoreApplication::translate("Yalahoo", "Invalid depth of field strength.");
    }
    return { };
}
void PreferencesStore::reportError(const QString &error)
{
    if (d->m_error != error) {
        d->m_error = error;
        Q_EMIT errorChanged();
    }
    if (!error.isEmpty()) {
        qCWarning(logSettings) << "Preferences operation failed" << "path=" << d->m_path
                               << "error=" << error;
        Q_EMIT saveError(error);
    }
}
bool PreferencesStore::update(const QVariantMap &patch)
{
    if (!d->m_writable) {
        Q_EMIT saveError(d->m_error);
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
        if (value.isObject() && d->m_document.value(it.key()).isObject()) {
            auto object = d->m_document.value(it.key()).toObject();
            const auto additions = value.toObject();
            for (auto field = additions.begin(); field != additions.end(); ++field)
                if (object.value(field.key()) != field.value())
                    object.insert(field.key(), field.value());
            value = object;
        }
        if (d->m_document.value(it.key()) != value) {
            d->m_document.insert(it.key(), value);
            modified = true;
        }
    }
    if (!modified)
        return true;
    ++d->m_revision;
    d->m_values.reset();
    d->m_dirty = true;
    reportError({ });
    Q_EMIT changed();
    schedule();
    return true;
}
void PreferencesStore::setAudio(const QJsonObject &audio)
{
    QVariantMap patch;
    for (const auto &key : { u"volume"_s, u"bgm"_s, u"bgmVolume"_s, u"sound"_s, u"soundVolume"_s })
        if (audio.contains(key))
            patch.insert(key, audio.value(key).toVariant());
    update(patch);
}
QString PreferencesStore::error() const
{
    return d->m_error;
}
void PreferencesStore::setNavigation(bool remember, bool realistic)
{
    update({ { u"rememberColumnPosition"_s, remember },
             { u"arrayMode"_s, realistic ? u"realistic"_s : u"filled"_s } });
}
PreferencesStore::ImportResult PreferencesStore::readPreferenceImport(const QString &path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)
        || file.size() > yalahoo::units::BytesPerKiB * yalahoo::units::BytesPerKiB)
        return ImportResult{ { },
                             QCoreApplication::translate(
                                     "Yalahoo",
                                     "Cannot read preference JSON, or it exceeds 1 MiB.") };
    const auto bytes = file.read(yalahoo::units::BytesPerKiB * yalahoo::units::BytesPerKiB + 1);
    QJsonParseError parse;
    const auto document = QJsonDocument::fromJson(bytes, &parse);
    if (bytes.size() > yalahoo::units::BytesPerKiB * yalahoo::units::BytesPerKiB
        || file.error() != QFileDevice::NoError || parse.error != QJsonParseError::NoError
        || !document.isObject())
        return ImportResult{ { },
                             QCoreApplication::translate(
                                     "Yalahoo",
                                     "Invalid preference JSON. The original file was preserved.") };
    auto object = document.object();
    const auto error = validate(object);
    if (!error.isEmpty())
        return ImportResult{ { }, error };
    return ImportResult{ object, { } };
}
bool PreferencesStore::importJson(const QUrl &url)
{
    if (!d->m_writable || d->m_import.isRunning() || !url.isLocalFile()
        || !QDir::isAbsolutePath(url.toLocalFile()) || url.hasQuery() || url.hasFragment()) {
        Q_EMIT saveError(QCoreApplication::translate(
                "Yalahoo",
                "Preference import requires writable native data and a local absolute JSON path."));
        return false;
    }
    const auto revision = ++d->m_revision;
    disconnect(&d->m_import, nullptr, this, nullptr);
    connect(&d->m_import, &QFutureWatcher<ImportResult>::finished, this, [this, revision] {
        if (revision != d->m_revision)
            return;
        const auto result = d->m_import.result();
        if (!result.error.isEmpty()) {
            reportError(result.error);
            return;
        }
        if (update(result.document.toVariantMap()))
            Q_EMIT imported();
    });
    d->m_import.setFuture(QtConcurrent::run(&d->m_worker, &PreferencesStore::readPreferenceImport,
                                            url.toLocalFile()));
    return true;
}
// Write a complete validated candidate with QSaveFile. Failure leaves the previous file intact;
// this helper returns the actual persistence error rather than reporting a successful preference update.
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
    if (bytes.size() > yalahoo::units::BytesPerKiB * yalahoo::units::BytesPerKiB)
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
    if (!d->m_dirty || d->m_saving || d->m_scheduled || !d->m_writable)
        return;
    d->m_scheduled = true;
    QTimer::singleShot(0, this, [this] {
        d->m_scheduled = false;
        if (!d->m_dirty || d->m_saving || !d->m_writable)
            return;
        d->m_dirty = false;
        d->m_saving = true;
        d->m_save.setFuture(QtConcurrent::run(&d->m_worker, &PreferencesStore::write, d->m_path,
                                              d->m_document));
    });
}
}
