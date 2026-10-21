// Loads and atomically saves validated product preferences. Unknown fields survive partial
// updates and source music paths are never rewritten.

#pragma once
#include <memory>
#include <QObject>
#include <QUrl>
#include <QJsonObject>
#include <QPair>
#include <QVariantMap>

namespace yalahoo {
class PreferencesStore final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PreferencesStore)
public:
    explicit PreferencesStore(const QString &dataRoot, QObject *parent = nullptr);
    ~PreferencesStore() override;
    Q_PROPERTY(QVariantMap values READ values NOTIFY changed FINAL)
    Q_PROPERTY(QString error READ error NOTIFY errorChanged FINAL)
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
    void load();
    static QPair<QJsonObject, QString> readPreferenceImport(const QString &path);

    static QString validate(const QJsonObject &document);
    static QString write(const QString &path, const QJsonObject &document);
    void schedule();
    void reportError(const QString &error);
    struct Private;
    std::unique_ptr<Private> d;
};
}
