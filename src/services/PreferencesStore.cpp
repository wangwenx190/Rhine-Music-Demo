#include "PreferencesStore.h"
#include <QtConcurrentRun>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QSaveFile>

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
        } else
            m_document = document.object();
    }
    connect(&m_save, &QFutureWatcher<QString>::finished, this, [this] {
        m_saving = false;
        m_error = m_save.result();
        if (!m_error.isEmpty())
            emit saveError(m_error);
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
QString PreferencesStore::error() const
{
    return m_error;
}
void PreferencesStore::setNavigation(bool remember, bool realistic)
{
    if (!m_writable) {
        emit saveError(m_error);
        return;
    }
    ++m_revision;
    m_document.insert("rememberColumnPosition", remember);
    m_document.insert("arrayMode", realistic ? "realistic" : "filled");
    m_dirty = true;
    schedule();
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
                    emit saveError(result.second);
                    return;
                }
                for (auto it = result.first.begin(); it != result.first.end(); ++it)
                    m_document.insert(it.key(), it.value());
                m_dirty = true;
                schedule();
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
        const auto object = document.object();
        if ((object.contains("rememberColumnPosition")
             && !object.value("rememberColumnPosition").isBool())
            || (object.contains("arrayMode") && object.value("arrayMode") != "realistic"
                && object.value("arrayMode") != "filled"))
            return Result{ { }, QStringLiteral("偏好导航字段类型无效。") };
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
    if (!file.open(QIODevice::WriteOnly)
        || !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)
        || file.write(bytes) != bytes.size() || !file.commit())
        return QStringLiteral("无法保存偏好：") + file.errorString();
    return { };
}
void PreferencesStore::schedule()
{
    if (!m_dirty || m_saving || !m_writable)
        return;
    m_dirty = false;
    m_saving = true;
    m_save.setFuture(QtConcurrent::run(&m_worker, &PreferencesStore::write, m_path, m_document));
}
}
