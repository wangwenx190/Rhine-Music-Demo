#pragma once
#include <QObject>
#include <QUrl>
#include <QJsonObject>
#include <QFutureWatcher>
#include <QThreadPool>
#include <QVariantMap>

namespace yalahoo {
class PreferencesStore : public QObject
{
    Q_OBJECT
public:
    explicit PreferencesStore(QString dataRoot, QObject *parent = nullptr);
    ~PreferencesStore() override;
    Q_PROPERTY(QVariantMap values READ values NOTIFY changed)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged)
    QVariantMap values() const;
    Q_INVOKABLE bool update(const QVariantMap &patch);
    bool rememberColumnPosition() const;
    bool realistic() const;
    QString error() const;
    QJsonObject audio() const;
    void setAudio(const QJsonObject &audio);
    bool importJson(const QUrl &file);
    void setNavigation(bool remember, bool realistic);
Q_SIGNALS:
    void changed();
    void errorChanged();
    void saveError(const QString &message);
    void saved();
    void imported();

private:
    static QString validate(const QJsonObject &document);
    static QString write(const QString &path, const QJsonObject &document);
    void schedule();
    void reportError(const QString &error);
    QString m_path, m_error;
    QJsonObject m_document;
    QThreadPool m_worker;
    QFutureWatcher<QString> m_save;
    QFutureWatcher<QPair<QJsonObject, QString>> m_import;
    quint64 m_revision = 0;
    bool m_writable = true, m_dirty = false, m_saving = false, m_scheduled = false;
};
}
