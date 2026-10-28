// Evaluates source-derived theme lighting and selection fields. Pure parameters are converted
// into render inputs without per-frame resource construction.

#pragma once
#include "RowLightController.h"
#include "FrameClock.h"
#include <QColor>
#include <QStringView>
#include <QVector4D>
#include <QVariantMap>
#include <optional>
namespace yalahoo {
namespace lightingDefaults {
// Default day palette uses source light units; theme interpolation takes seconds.
inline constexpr auto Environment = .25;
inline constexpr auto Key = .45;
inline constexpr auto Hemisphere = .32;
inline constexpr auto Fill = .045;
inline constexpr auto SpotIntensity = 180;
inline constexpr auto SpotUnitScale = 64;
inline constexpr auto AreaIntensity = 34;
inline constexpr auto PrintAmbient = .5;
inline constexpr auto Exposure = 1.05;
inline constexpr auto TransitionSeconds = .65;
}

struct LightingPalette final
{
    double warmth = 1, environment = lightingDefaults::Environment, key = lightingDefaults::Key,
           hemisphere = lightingDefaults::Hemisphere, fill = lightingDefaults::Fill;
    double spot = lightingDefaults::SpotIntensity * lightingDefaults::SpotUnitScale,
           area = lightingDefaults::AreaIntensity, scatter = 1,
           printAmbient = lightingDefaults::PrintAmbient, exposure = lightingDefaults::Exposure;
    SceneVector keyColor, skyColor, groundColor, spotColor, scatterColor, background;
    SceneVector floor;
    static SceneVector floorColor(QStringView theme);
    static LightingPalette theme(bool night);
    bool operator==(const LightingPalette &) const = default;
};
class SelectionLighting : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SelectionLighting)
    Q_PROPERTY(QVariantMap settings READ settings NOTIFY settingsChanged FINAL)
    Q_PROPERTY(bool comparing READ comparing WRITE setComparing NOTIFY comparingChanged FINAL)
    Q_PROPERTY(double warmth READ warmth NOTIFY changed FINAL)
    Q_PROPERTY(double environmentIntensity READ environmentIntensity NOTIFY changed FINAL)
    Q_PROPERTY(double exposure READ exposure NOTIFY changed FINAL)
    Q_PROPERTY(double printAmbient READ printAmbient NOTIFY changed FINAL)
    Q_PROPERTY(double scatterStrength READ scatterStrength NOTIFY changed FINAL)
    Q_PROPERTY(double spotIntensity READ spotIntensity NOTIFY changed FINAL)
    Q_PROPERTY(double areaIntensity READ areaIntensity NOTIFY changed FINAL)
    Q_PROPERTY(double rowDetail READ rowDetail NOTIFY changed FINAL)
    Q_PROPERTY(double rowFill READ rowFill NOTIFY changed FINAL)
    Q_PROPERTY(QVector4D rowSettings READ rowSettings NOTIFY changed FINAL)
    Q_PROPERTY(QVector3D rowColumn READ rowColumn NOTIFY changed FINAL)
    Q_PROPERTY(QVector3D lightColumn READ lightColumn NOTIFY changed FINAL)
    Q_PROPERTY(QVector3D spotPosition READ spotPosition NOTIFY changed FINAL)
    Q_PROPERTY(QVector3D spotTarget READ spotTarget NOTIFY changed FINAL)
    Q_PROPERTY(QVector3D areaPosition READ areaPosition NOTIFY changed FINAL)
    Q_PROPERTY(QVector3D areaTarget READ areaTarget NOTIFY changed FINAL)
    Q_PROPERTY(QVector3D scatterColor READ scatterColor NOTIFY changed FINAL)
    Q_PROPERTY(QVector3D spotColor READ spotColor NOTIFY changed FINAL)
    Q_PROPERTY(QVector3D keyColor READ keyColor NOTIFY changed FINAL)
    Q_PROPERTY(QVector3D skyColor READ skyColor NOTIFY changed FINAL)
    Q_PROPERTY(QVector3D groundColor READ groundColor NOTIFY changed FINAL)
    Q_PROPERTY(QVector3D lightIntensities READ lightIntensities NOTIFY changed FINAL)
    Q_PROPERTY(QVector3D backgroundLinear READ backgroundLinear NOTIFY changed FINAL)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor NOTIFY changed FINAL)
    Q_PROPERTY(bool night READ night WRITE setNight NOTIFY nightChanged FINAL)
public:
    explicit SelectionLighting(QObject *parent = nullptr);
    RowLightController ribbon;
    const LightingPalette &palette() const
    {
        return m_palette;
    }
    void setNight(bool night);
    void setReducedMotion(bool reduced);
    void setSettings(RowLightingSettings settings);
    QVariantMap settings() const;
    Q_INVOKABLE bool configure(const QVariantMap &patch);
    Q_INVOKABLE void resetSettings();
    bool comparing() const
    {
        return m_comparing;
    }
    void setComparing(bool comparing);
    // Receives motionDt, already scaled once by FrameClock. No private timer.
    void update(const RowLightingInput &input, SceneVector aim, SceneVector cameraOffset,
                double detail, double motionDt);
    void advanceTheme(double motionDt);
    static double themeProgress(double t);
    double warmth() const
    {
        return m_palette.warmth;
    }
    double environmentIntensity() const
    {
        return m_palette.environment;
    }
    double exposure() const
    {
        return m_palette.exposure;
    }
    double printAmbient() const
    {
        return m_palette.printAmbient;
    }
    double scatterStrength() const
    {
        return m_palette.scatter;
    }
    double spotIntensity() const
    {
        return m_visible ? m_palette.spot : 0;
    }
    double areaIntensity() const;
    double rowDetail() const
    {
        return m_detail;
    }
    double rowFill() const
    {
        return ribbon.settings.fill;
    }
    QVector4D rowSettings() const;
    QVector3D rowColumn() const
    {
        return ribbon.column.qt();
    }
    QVector3D lightColumn() const
    {
        return m_column.qt();
    }
    QVector3D spotPosition() const
    {
        return (m_anchor + m_offset).qt();
    }
    QVector3D spotTarget() const
    {
        return m_anchor.qt();
    }
    QVector3D areaPosition() const;
    QVector3D areaTarget() const;
    QVector3D scatterColor() const
    {
        return m_palette.scatterColor.qt();
    }
    QVector3D spotColor() const
    {
        return m_palette.spotColor.qt();
    }
    QVector3D keyColor() const
    {
        return m_palette.keyColor.qt();
    }
    QVector3D skyColor() const
    {
        return m_palette.skyColor.qt();
    }
    QVector3D groundColor() const
    {
        return m_palette.groundColor.qt();
    }
    QVector3D lightIntensities() const
    {
        return { float(m_palette.key), float(m_palette.hemisphere), float(m_palette.fill) };
    }
    QVector3D backgroundLinear() const
    {
        return m_palette.background.qt();
    }
    QColor backgroundColor() const;
    bool night() const
    {
        return m_night;
    }
    SceneVector anchorValue() const
    {
        return m_anchor;
    }
    SceneVector columnValue() const
    {
        return m_column;
    }
Q_SIGNALS:
    void changed();
    void nightChanged();
    void settingsChanged();
    void comparingChanged();

private:
    struct PublishedLighting final
    {
        LightingPalette palette;
        RowLightingSettings settings;
        QVector3D rowColumn, lightColumn, spotPosition, spotTarget, areaPosition, areaTarget;
        double detail = 0;
        bool visible = true;
        bool operator==(const PublishedLighting &) const = default;
    };
    void notifyChanged();
    std::optional<PublishedLighting> m_published;
    RowLightingSettings m_settings;
    bool m_comparing = false;
    LightingPalette m_palette, m_from, m_to;
    double m_elapsed = lightingDefaults::TransitionSeconds, m_detail = 0;
    SceneVector m_anchor, m_column, m_offset, m_anchorVelocity, m_columnVelocity;
    bool m_night = false, m_reduced = false, m_initialized = false, m_visible = true;
};
}
