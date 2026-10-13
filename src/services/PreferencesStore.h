#pragma once
#include <QObject>
#include <QJsonObject>
#include <QFutureWatcher>
#include <QThreadPool>

namespace yalahoo {
class PreferencesStore : public QObject
{
    Q_OBJECT
public:
    explicit PreferencesStore(QString dataRoot, QObject *parent = nullptr);
    ~PreferencesStore() override;
    bool rememberColumnPosition() const;
    bool realistic() const;
    QString error() const;
    void setNavigation(bool remember, bool realistic);
signals:
    void saveError(const QString &message);
    void saved();

private:
    static QString write(const QString &path, const QJsonObject &document);
    void schedule();
    QString m_path, m_error;
    QJsonObject m_document;
    QThreadPool m_worker;
    QFutureWatcher<QString> m_save;
    bool m_writable = true, m_dirty = false, m_saving = false;
};
}
