#pragma once
#include <QString>
using Qt::StringLiterals::operator""_s;
#include "SceneAdapter.h"
#include "PreferencesStore.h"
#include "LibrarySession.h"

namespace yalahoo {
// Field-specific routing; visual reload never owns media or persistent controllers.
class SettingsController final : public QObject
{
    Q_OBJECT
    Q_PROPERTY(QVariantMap values READ values NOTIFY changed)
public:
    SettingsController(PreferencesStore *preferences, LibrarySession *library,
                       MotionController *motion, SceneAdapter *scene,
                       SelectionController *selection, QObject *parent = nullptr);
    QVariantMap values() const;
    Q_INVOKABLE bool update(const QString &field, const QVariant &value);
    Q_INVOKABLE bool updateQuality(const QString &field, const QVariant &value);
    Q_INVOKABLE void reload();
Q_SIGNALS:
    void changed();
    void reloadRequested();

private:
    void apply();
    void persist(const QString &field, const QVariant &value);
    PreferencesStore *m_preferences;
    LibrarySession *m_library;
    MotionController *m_motion;
    SceneAdapter *m_scene;
    SelectionController *m_selection;
    QString m_sortMode = u"genre"_s;
    bool m_applying = false;
};
}
