#include "SettingsController.h"
#include <cmath>

namespace yalahoo {
SettingsController::SettingsController(PreferencesStore *preferences, LibrarySession *library,
                                       MotionController *motion, SceneAdapter *scene,
                                       SelectionController *selection, QObject *parent)
    : QObject(parent),
      m_preferences(preferences),
      m_library(library),
      m_motion(motion),
      m_scene(scene),
      m_selection(selection)
{
    if (m_preferences) {
        apply();
        connect(m_preferences, &PreferencesStore::imported, this, &SettingsController::apply);
    }
    connect(scene->lighting(), &SelectionLighting::nightChanged, this, [this] {
        persist("theme", m_scene->lighting()->night() ? "night" : "day");
    });
    connect(scene->quality(), &RenderQuality::changed, this, [this] {
        persist("renderQuality", m_scene->quality()->values());
    });
    connect(motion, &MotionController::reducedMotionChanged, this, [this] {
        persist("reduced", m_motion->reducedMotion());
    });
    connect(motion->clock(), &FrameClock::speedChanged, this, [this] {
        persist("motionSpeed", m_motion->clock()->speed());
    });
    connect(selection, &SelectionController::memoryChanged, this, [this] {
        persist("rememberColumnPosition", m_selection->rememberColumnPosition());
    });
    connect(selection, &SelectionController::arrayModeChanged, this, [this] {
        persist("arrayMode", m_selection->realistic() ? "realistic" : "filled");
    });
}
QVariantMap SettingsController::values() const
{
    return { { "theme", m_scene->lighting()->night() ? "night" : "day" },
             { "sortMode", m_sortMode },
             { "arrayMode", m_selection->realistic() ? "realistic" : "filled" },
             { "rememberColumnPosition", m_selection->rememberColumnPosition() },
             { "reduced", m_motion->reducedMotion() },
             { "motionSpeed", m_motion->clock()->speed() },
             { "quality", m_scene->quality()->preset() },
             { "renderQuality", m_scene->quality()->values() } };
}
void SettingsController::persist(const QString &field, const QVariant &value)
{
    if (m_applying)
        return;
    if (m_preferences)
        m_preferences->update({ { field, value } });
    emit changed();
}
void SettingsController::apply()
{
    m_applying = true;
    const auto values = m_preferences->values();
    m_scene->lighting()->setNight(values.value("theme").toString() == "night");
    m_motion->setReducedMotion(values.value("reduced").toBool());
    m_motion->clock()->setSpeed(values.value("motionSpeed").toDouble());
    m_sortMode = values.value("sortMode").toString();
    m_library->sort(m_sortMode);
    m_selection->setRememberColumnPosition(values.value("rememberColumnPosition").toBool());
    m_selection->setRealistic(values.value("arrayMode").toString() == "realistic");
    if (values.contains("renderQuality"))
        m_scene->quality()->configure(values.value("renderQuality").toMap());
    else
        m_scene->quality()->setPreset(values.value("quality", "original").toString());
    m_applying = false;
    emit changed();
}
bool SettingsController::update(const QString &field, const QVariant &value)
{
    if (field == "theme" && (value == "day" || value == "night"))
        m_scene->lighting()->setNight(value == "night");
    else if (field == "reduced" && value.metaType().id() == QMetaType::Bool)
        m_motion->setReducedMotion(value.toBool());
    else if (field == "motionSpeed" && value.metaType().id() != QMetaType::QString
             && value.metaType().id() != QMetaType::Bool && value.canConvert<double>()
             && std::isfinite(value.toDouble()))
        m_motion->clock()->setSpeed(value.toDouble());
    else if (field == "sortMode" && (value == "genre" || value == "artist" || value == "album")) {
        m_sortMode = value.toString();
        m_library->sort(m_sortMode);
        persist(field, value);
    } else if (field == "arrayMode" && (value == "filled" || value == "realistic"))
        m_selection->setRealistic(value == "realistic");
    else if (field == "rememberColumnPosition" && value.metaType().id() == QMetaType::Bool)
        m_selection->setRememberColumnPosition(value.toBool());
    else if (field == "quality"
             && (value == "performance" || value == "original" || value == "high"
                 || value == "ultra"))
        m_scene->quality()->setPreset(value.toString());
    else if (field == "renderQuality" && value.canConvert<QVariantMap>())
        m_scene->quality()->configure(value.toMap());
    else
        return false;
    return true;
}
void SettingsController::reload()
{
    m_library->cancelAllMetadata();
    m_motion->clock()->setActive(false);
    emit reloadRequested();
}
}
