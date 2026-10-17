#include "SettingsController.h"
#include <cmath>

using namespace Qt::StringLiterals;

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
        persist(u"theme"_s, m_scene->lighting()->night() ? u"night"_s : u"day"_s);
    });
    connect(scene->lighting(), &SelectionLighting::settingsChanged, this, [this] {
        persist(u"lighting"_s, m_scene->lighting()->settings());
    });
    connect(scene->quality(), &RenderQuality::changed, this, [this] {
        persist(u"renderQuality"_s, m_scene->quality()->values());
    });
    connect(motion, &MotionController::reducedMotionChanged, this, [this] {
        persist(u"reduced"_s, m_motion->reducedMotion());
    });
    connect(motion->clock(), &FrameClock::speedChanged, this, [this] {
        persist(u"motionSpeed"_s, m_motion->clock()->speed());
    });
    connect(selection, &SelectionController::memoryChanged, this, [this] {
        persist(u"rememberColumnPosition"_s, m_selection->rememberColumnPosition());
    });
    connect(selection, &SelectionController::arrayModeChanged, this, [this] {
        persist(u"arrayMode"_s, m_selection->realistic() ? u"realistic"_s : u"filled"_s);
    });
}
QVariantMap SettingsController::values() const
{
    return { { u"theme"_s, m_scene->lighting()->night() ? u"night"_s : u"day"_s },
             { u"sortMode"_s, m_sortMode },
             { u"arrayMode"_s, m_selection->realistic() ? u"realistic"_s : u"filled"_s },
             { u"rememberColumnPosition"_s, m_selection->rememberColumnPosition() },
             { u"reduced"_s, m_motion->reducedMotion() },
             { u"motionSpeed"_s, m_motion->clock()->speed() },
             { u"quality"_s, m_scene->quality()->preset() },
             { u"renderQuality"_s, m_scene->quality()->values() } };
}
void SettingsController::persist(const QString &field, const QVariant &value)
{
    if (m_applying)
        return;
    if (m_preferences)
        m_preferences->update({ { field, value } });
    Q_EMIT changed();
}
void SettingsController::apply()
{
    m_applying = true;
    const auto values = m_preferences->values();
    m_scene->lighting()->setNight(values.value(u"theme"_s).toString() == "night"_L1);
    // Unknown nested extensions stay in PreferencesStore, outside the runtime parameter set.
    auto lighting = values.value(u"lighting"_s).toMap();
    const auto fields = m_scene->lighting()->settings();
    for (auto it = lighting.begin(); it != lighting.end();) {
        if (!fields.contains(it.key()))
            it = lighting.erase(it);
        else
            ++it;
    }
    m_scene->lighting()->resetSettings();
    m_scene->lighting()->configure(lighting);
    m_motion->setReducedMotion(values.value(u"reduced"_s).toBool());
    m_motion->clock()->setSpeed(values.value(u"motionSpeed"_s).toDouble());
    m_sortMode = values.value(u"sortMode"_s).toString();
    m_library->sort(m_sortMode);
    m_selection->setRememberColumnPosition(values.value(u"rememberColumnPosition"_s).toBool());
    m_selection->setRealistic(values.value(u"arrayMode"_s).toString() == "realistic"_L1);
    if (values.contains(u"renderQuality"_s))
        m_scene->quality()->configure(values.value(u"renderQuality"_s).toMap());
    else
        m_scene->quality()->setPreset(values.value(u"quality"_s, u"original"_s).toString());
    m_applying = false;
    Q_EMIT changed();
}
bool SettingsController::update(const QString &field, const QVariant &value)
{
    if (field == "theme"_L1 && (value == u"day"_s || value == u"night"_s))
        m_scene->lighting()->setNight(value == u"night"_s);
    else if (field == "reduced"_L1 && value.metaType().id() == QMetaType::Bool)
        m_motion->setReducedMotion(value.toBool());
    else if (field == "motionSpeed"_L1 && value.metaType().id() != QMetaType::QString
             && value.metaType().id() != QMetaType::Bool && value.canConvert<double>()
             && std::isfinite(value.toDouble()))
        m_motion->clock()->setSpeed(value.toDouble());
    else if (field == "sortMode"_L1
             && (value == u"genre"_s || value == u"artist"_s || value == u"album"_s)) {
        m_sortMode = value.toString();
        m_library->sort(m_sortMode);
        persist(field, value);
    } else if (field == "arrayMode"_L1 && (value == u"filled"_s || value == u"realistic"_s))
        m_selection->setRealistic(value == u"realistic"_s);
    else if (field == "rememberColumnPosition"_L1 && value.metaType().id() == QMetaType::Bool)
        m_selection->setRememberColumnPosition(value.toBool());
    else if (field == "quality"_L1
             && (value == u"performance"_s || value == u"original"_s || value == u"high"_s
                 || value == u"ultra"_s))
        m_scene->quality()->setPreset(value.toString());
    else if (field == "renderQuality"_L1 && value.canConvert<QVariantMap>())
        m_scene->quality()->configure(value.toMap());
    else
        return false;
    return true;
}
bool SettingsController::updateQuality(const QString &field, const QVariant &value)
{
    auto values = m_scene->quality()->values();
    if (!values.contains(field))
        return false;
    values.insert(field, value);
    return update(u"renderQuality"_s, values);
}
void SettingsController::reload()
{
    m_library->cancelAllMetadata();
    m_motion->clock()->setActive(false);
    Q_EMIT reloadRequested();
}
}
