#pragma once
#include "TextLayoutAdapter.h"
#include "FrameClock.h"
#include <QPointer>
#include <QQmlParserStatus>
#include <QVariantMap>
#include <QAbstractListModel>
#include <array>

namespace yalahoo {
struct ReelTrack
{
    double start = 0, initialVelocity = 0, target = 0, elapsed = .460;
    double duration = .460;
    bool sampled = false;
    std::array<double, 49> points{ };
    struct Sample
    {
        double value = 0, velocity = 0;
    };
    Sample sample() const;
    double display() const;
    void move(double to, bool instant, Sample from);
    void move(double to, bool instant = false);
    void advance(double dt);
    bool moving() const;
};
class ReelSlot : public QObject
{
    Q_OBJECT
    Q_PROPERTY(double positionX READ positionX NOTIFY frameChanged)
    Q_PROPERTY(double positionY READ positionY NOTIFY frameChanged)
    Q_PROPERTY(double viewportHeight READ viewportHeight NOTIFY frameChanged)
    Q_PROPERTY(double padding READ padding NOTIFY contentChanged)
    Q_PROPERTY(double capacityWidth MEMBER capacityWidth NOTIFY contentChanged)
    Q_PROPERTY(double capacityHeight MEMBER capacityHeight NOTIFY contentChanged)
    Q_PROPERTY(bool shown READ shown NOTIFY frameChanged)
    Q_PROPERTY(bool translation READ translation NOTIFY contentChanged)
    Q_PROPERTY(double pixelSize READ pixelSize NOTIFY contentChanged)
    Q_PROPERTY(double rollPosition READ rollPosition NOTIFY frameChanged)
    Q_PROPERTY(QVariantList faceValues READ faceValues NOTIFY contentChanged)
    Q_PROPERTY(double opacity READ opacity NOTIFY frameChanged)
    Q_PROPERTY(double blurOpacity READ blurOpacity NOTIFY frameChanged)
    Q_PROPERTY(bool nativeInk READ nativeInk NOTIFY contentChanged)
    Q_PROPERTY(int faceCapacity READ faceCapacity NOTIFY contentChanged)
public:
    using QObject::QObject;
    struct Face
    {
        TitleGlyph glyph;
        double offset = 0;
    };
    QString key;
    TitleGlyph glyph;
    QList<Face> faces;
    ReelTrack roll, x, y, viewport, alpha, entry;
    bool transport = false;
    bool blurOnEntry = false;
    std::array<double, 49> blurPoints{ };
    double capacityWidth = 0, capacityHeight = 0;
    quint64 revision = 0;
    bool active = false;
    double positionX() const;
    double positionY() const;
    double viewportHeight() const;
    double padding() const;
    bool shown() const;
    bool translation() const;
    double pixelSize() const;
    double rollPosition() const;
    QVariantList faceValues() const;
    double opacity() const;
    double blurOpacity() const;
    bool nativeInk() const;
    int faceCapacity() const;
    void writeFaces(const QList<TitleGlyph> &values);
    void rollTo(const TitleGlyph &next, bool instant);
    void advance(double dt);
    bool moving() const;
signals:
    void contentChanged();
    void frameChanged();

private:
    QVariantList m_values;
    int m_faceCapacity = 3;
};
class TitleReel : public QAbstractListModel, public QQmlParserStatus
{
    Q_OBJECT
    Q_INTERFACES(QQmlParserStatus)
    Q_PROPERTY(yalahoo::FrameClock *clock READ clock WRITE setClock NOTIFY clockChanged)
    Q_PROPERTY(QString text READ text WRITE setText NOTIFY inputChanged)
    Q_PROPERTY(QFont font READ font WRITE setFont NOTIFY inputChanged)
    Q_PROPERTY(QFont subtitleFont READ subtitleFont WRITE setSubtitleFont NOTIFY inputChanged)
    Q_PROPERTY(double pixelSize READ pixelSize WRITE setPixelSize NOTIFY inputChanged)
    Q_PROPERTY(double availableWidth READ availableWidth WRITE setAvailableWidth NOTIFY inputChanged)
    Q_PROPERTY(double budget READ budget WRITE setBudget NOTIFY inputChanged)
    Q_PROPERTY(double dpr READ dpr WRITE setDpr NOTIFY inputChanged)
    Q_PROPERTY(bool narrow READ narrow WRITE setNarrow NOTIFY inputChanged)
    Q_PROPERTY(bool transport READ transport WRITE setTransport NOTIFY inputChanged)
    Q_PROPERTY(bool reducedMotion READ reducedMotion WRITE setReducedMotion NOTIFY inputChanged)
    Q_PROPERTY(bool active READ active WRITE setActive NOTIFY inputChanged)
    Q_PROPERTY(int slotCount READ slotCount NOTIFY slotsChanged)
    Q_PROPERTY(double layoutHeight READ layoutHeight NOTIFY changed)
    Q_PROPERTY(double naturalWidth READ naturalWidth NOTIFY changed)
    Q_PROPERTY(double fittedSize READ fittedSize NOTIFY changed)
    Q_PROPERTY(QString fittedText READ fittedText NOTIFY changed)
    Q_PROPERTY(bool moving READ moving NOTIFY changed)
public:
    explicit TitleReel(QObject *parent = nullptr);
    int rowCount(const QModelIndex &parent = { }) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;
    void classBegin() override;
    void componentComplete() override;
    FrameClock *clock() const;
    void setClock(FrameClock *clock);
    QString text() const;
    void setText(const QString &text);
    QFont font() const;
    void setFont(const QFont &font);
    QFont subtitleFont() const;
    void setSubtitleFont(const QFont &font);
    double pixelSize() const;
    void setPixelSize(double value);
    double availableWidth() const;
    void setAvailableWidth(double value);
    double budget() const;
    void setBudget(double value);
    double dpr() const;
    void setDpr(double value);
    bool narrow() const;
    void setNarrow(bool value);
    bool transport() const;
    void setTransport(bool value);
    bool reducedMotion() const;
    void setReducedMotion(bool value);
    bool active() const;
    void setActive(bool value);
    int slotCount() const;
    double layoutHeight() const;
    double naturalWidth() const;
    double fittedSize() const;
    QString fittedText() const;
    bool moving() const;
    Q_INVOKABLE yalahoo::ReelSlot *slotAt(int index) const;
    Q_INVOKABLE void reconcile();
    Q_INVOKABLE void finish();
    Q_INVOKABLE QVariantMap diagnostics() const;
    QList<QSize> resourceSizes() const;
    void advance(double dt);
signals:
    void inputChanged();
    void clockChanged();
    void slotsChanged();
    void changed();
    void resourcesChanged();

private:
    void schedule(bool instant);
    void prune();
    void publishResources();
    TextLayoutAdapter m_layout;
    TitleLayout m_target;
    QList<ReelSlot *> m_slots;
    QList<ReelSlot *> m_animatedSlots;
    QHash<QString, ReelSlot *> m_index;
    QPointer<FrameClock> m_clock;
    QMetaObject::Connection m_connection;
    QString m_text, m_displayed;
    QFont m_font, m_subtitle;
    double m_size = 28, m_width = 300, m_budget = 300, m_dpr = 1;
    bool m_narrow = false, m_transport = false, m_reduced = false, m_active = true;
    bool m_complete = false, m_scheduled = false, m_instant = true;
    bool m_dirty = true;
    ReelTrack m_height;
    QList<QSize> m_resourceSizes;
};
}
