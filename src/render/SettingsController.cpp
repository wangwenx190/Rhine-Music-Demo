#include "SelectionLighting.h"
#include "RenderQuality.h"
#include "FrameClock.h"
#include "MotionController.h"
#include "SelectionController.h"
#include "LibrarySession.h"
#include "PreferencesStore.h"
#include "SceneAdapter.h"
#include "SettingsController.h"
#include "NumericValue.h"
#include <array>
#include <algorithm>
#include <iterator>

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
    return { { u"singleInstance"_s, m_singleInstance },
             { u"theme"_s, m_scene->lighting()->night() ? u"night"_s : u"day"_s },
             { u"sortMode"_s, m_sortMode },
             { u"arrayMode"_s, m_selection->realistic() ? u"realistic"_s : u"filled"_s },
             { u"rememberColumnPosition"_s, m_selection->rememberColumnPosition() },
             { u"reduced"_s, m_motion->reducedMotion() },
             { u"motionSpeed"_s, m_motion->clock()->speed() },
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
    m_singleInstance = m_preferences->values().value(u"singleInstance"_s).toBool();
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

    m_applying = false;
    Q_EMIT changed();
}
bool SettingsController::update(const QString &field, const QVariant &value)
{
    struct Route final
    {
        QLatin1StringView field;
        bool (*apply)(SettingsController &, const QVariant &);
    };
    static constexpr Route routes[] = {
        { "theme"_L1,
          [](SettingsController &self, const QVariant &input) {
              if (input != u"day"_s && input != u"night"_s)
                  return false;
              self.m_scene->lighting()->setNight(input == u"night"_s);
              return true;
          } },
        { "reduced"_L1,
          [](SettingsController &self, const QVariant &input) {
              if (input.metaType().id() != QMetaType::Bool)
                  return false;
              self.m_motion->setReducedMotion(input.toBool());
              return true;
          } },
        { "motionSpeed"_L1,
          [](SettingsController &self, const QVariant &input) {
              const auto number = finiteNumber(input);
              if (!number)
                  return false;
              self.m_motion->clock()->setSpeed(*number);
              return true;
          } },
        { "sortMode"_L1,
          [](SettingsController &self, const QVariant &input) {
              if (input != u"genre"_s && input != u"artist"_s && input != u"album"_s)
                  return false;
              self.m_sortMode = input.toString();
              self.m_library->sort(self.m_sortMode);
              self.persist(u"sortMode"_s, input);
              return true;
          } },
        { "arrayMode"_L1,
          [](SettingsController &self, const QVariant &input) {
              if (input != u"filled"_s && input != u"realistic"_s)
                  return false;
              self.m_selection->setRealistic(input == u"realistic"_s);
              return true;
          } },
        { "rememberColumnPosition"_L1,
          [](SettingsController &self, const QVariant &input) {
              if (input.metaType().id() != QMetaType::Bool)
                  return false;
              self.m_selection->setRememberColumnPosition(input.toBool());
              return true;
          } },
        { "renderQuality"_L1,
          [](SettingsController &self, const QVariant &input) {
              if (!input.canConvert<QVariantMap>())
                  return false;
              return self.m_scene->quality()->configure(input.toMap());
          } },
        { "singleInstance"_L1,
          [](SettingsController &self, const QVariant &input) {
              if (input.metaType().id() != QMetaType::Bool)
                  return false;
              if (self.m_preferences
                  && !self.m_preferences->update({ { u"singleInstance"_s, input } }))
                  return false;
              self.m_singleInstance = input.toBool();
              Q_EMIT self.changed();
              return true;
          } }
    };
    const auto route =
            std::find_if(std::begin(routes), std::end(routes), [&](const Route &candidate) {
                return candidate.field == field;
            });
    return route != std::end(routes) && route->apply(*this, value);
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
