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
    m_document.insert("rememberColumnPosition", remember);
    m_document.insert("arrayMode", realistic ? "realistic" : "filled");
    m_dirty = true;
    schedule();
}
QString PreferencesStore::write(const QString &path, const QJsonObject &document)
{
    if (!QDir().mkpath(QFileInfo(path).absolutePath()))
        return QStringLiteral("无法创建偏好目录。");
    QSaveFile file(path);
    const auto bytes = QJsonDocument(document).toJson();
    if (!file.open(QIODevice::WriteOnly) || file.write(bytes) != bytes.size() || !file.commit())
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
