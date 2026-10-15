#pragma once
#include "RowLightController.h"
#include "FrameClock.h"
#include <QColor>
#include <QStringView>
#include <QVector4D>
namespace yalahoo {
struct LightingPalette
{
    double warmth = 1, environment = .25, key = .45, hemisphere = .32, fill = .045;
    double spot = 180 * 64, area = 34, scatter = 1, printAmbient = .5, exposure = 1.05;
    SceneVector keyColor, skyColor, groundColor, spotColor, scatterColor, background;
    SceneVector floor;
    static SceneVector floorColor(QStringView theme);
    static LightingPalette theme(bool night);
};
class SelectionLighting : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double warmth READ warmth NOTIFY changed)
    Q_PROPERTY(double environmentIntensity READ environmentIntensity NOTIFY changed)
    Q_PROPERTY(double exposure READ exposure NOTIFY changed)
    Q_PROPERTY(double printAmbient READ printAmbient NOTIFY changed)
    Q_PROPERTY(double scatterStrength READ scatterStrength NOTIFY changed)
    Q_PROPERTY(double spotIntensity READ spotIntensity NOTIFY changed)
    Q_PROPERTY(double areaIntensity READ areaIntensity NOTIFY changed)
    Q_PROPERTY(double rowDetail READ rowDetail NOTIFY changed)
    Q_PROPERTY(double rowFill READ rowFill NOTIFY changed)
    Q_PROPERTY(QVector4D rowSettings READ rowSettings NOTIFY changed)
    Q_PROPERTY(QVector3D rowColumn READ rowColumn NOTIFY changed)
    Q_PROPERTY(QVector3D lightColumn READ lightColumn NOTIFY changed)
    Q_PROPERTY(QVector3D spotPosition READ spotPosition NOTIFY changed)
    Q_PROPERTY(QVector3D spotTarget READ spotTarget NOTIFY changed)
    Q_PROPERTY(QVector3D areaPosition READ areaPosition NOTIFY changed)
    Q_PROPERTY(QVector3D areaTarget READ areaTarget NOTIFY changed)
    Q_PROPERTY(QVector3D scatterColor READ scatterColor NOTIFY changed)
    Q_PROPERTY(QVector3D spotColor READ spotColor NOTIFY changed)
    Q_PROPERTY(QVector3D keyColor READ keyColor NOTIFY changed)
    Q_PROPERTY(QVector3D skyColor READ skyColor NOTIFY changed)
    Q_PROPERTY(QVector3D groundColor READ groundColor NOTIFY changed)
    Q_PROPERTY(QVector3D lightIntensities READ lightIntensities NOTIFY changed)
    Q_PROPERTY(QVector3D backgroundLinear READ backgroundLinear NOTIFY changed)
    Q_PROPERTY(QColor backgroundColor READ backgroundColor NOTIFY changed)
    Q_PROPERTY(bool night READ night WRITE setNight NOTIFY nightChanged)
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
signals:
    void changed();
    void nightChanged();

private:
    LightingPalette m_palette, m_from, m_to;
    double m_elapsed = .65, m_detail = 0;
    SceneVector m_anchor, m_column, m_offset, m_anchorVelocity, m_columnVelocity;
    bool m_night = false, m_reduced = false, m_initialized = false, m_visible = true;
};
}
