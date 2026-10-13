#include "SceneItem.h"
#include <QQuickWindow>
#include <QScreen>
#include <QMutex>
#include <QMutexLocker>
#include <memory>
#include <limits>

namespace yalahoo::rhi {
SceneItem::SceneItem(QQuickItem *parent)
    : RhiItem(parent), m_cover(":/assets/demo-covers/square.png")
{
    // R11 supplies asynchronous library covers. This one embedded bootstrap image is decoded
    // before show, and subsequently shared; the frame and render paths perform no decoding.
    connect(this, &QQuickItem::widthChanged, this, &SceneItem::schedule);
    connect(this, &QQuickItem::heightChanged, this, &SceneItem::schedule);
    connect(this, &QQuickItem::visibleChanged, this, &SceneItem::schedule);
    connect(this, &RhiItem::renderFailed, this, [this](const QString &message) {
        m_errorText = message;
        emit errorTextChanged();
    });
    connect(this, &QQuickItem::windowChanged, this, [this](QQuickWindow *window) {
        if (window) {
            connect(window, &QWindow::screenChanged, this, &SceneItem::schedule,
                    Qt::UniqueConnection);
            connect(window, &QWindow::visibleChanged, this, &SceneItem::schedule,
                    Qt::UniqueConnection);
        }
        schedule();
    });
}
void SceneItem::setAdapter(SceneAdapter *adapter)
{
    if (m_adapter == adapter)
        return;
    if (m_adapter) {
        disconnect(m_adapter, nullptr, this, nullptr);
        disconnect(m_adapter->lighting(), nullptr, this, nullptr);
        disconnect(m_adapter->quality(), nullptr, this, nullptr);
    }
    m_adapter = adapter;
    if (adapter) {
        connect(adapter, &SceneAdapter::frameChanged, this, &SceneItem::schedule);
        connect(adapter->lighting(), &SelectionLighting::changed, this, &SceneItem::schedule);
        connect(adapter->quality(), &RenderQuality::changed, this, &SceneItem::schedule);
    }
    schedule();
    emit adapterChanged();
}
void SceneItem::setGeometryOnly(bool enabled)
{
    if (m_geometryOnly == enabled)
        return;
    m_geometryOnly = enabled;
    schedule();
    emit geometryOnlyChanged();
}
void SceneItem::setTelemetryEnabled(bool enabled)
{
    if (m_telemetryEnabled == enabled)
        return;
    m_telemetryEnabled = enabled;
    profiled = { };
    if (enabled) {
        struct Mailbox
        {
            QMutex mutex;
            QVector<QJsonObject> samples;
            bool queued = false;
            quint64 coalesced = 0;
        };
        auto mailbox = std::make_shared<Mailbox>();
        QPointer<SceneItem> guard(this);
        profiled = [mailbox, guard](QJsonObject sample) {
            const QMutexLocker lock(&mailbox->mutex);
            if (mailbox->samples.size() == 128) {
                mailbox->samples.removeFirst();
                ++mailbox->coalesced;
            }
            mailbox->samples.append(std::move(sample));
            if (mailbox->queued)
                return;
            mailbox->queued = true;
            if (guard)
                QMetaObject::invokeMethod(
                        guard,
                        [mailbox, guard] {
                            QVector<QJsonObject> batch;
                            quint64 dropped = 0;
                            {
                                const QMutexLocker lock(&mailbox->mutex);
                                batch.swap(mailbox->samples);
                                dropped = mailbox->coalesced;
                                mailbox->queued = false;
                            }
                            if (guard && guard->telemetryEnabled())
                                for (auto &sample : batch) {
                                    sample["coalescedSamples"] = qint64(dropped);
                                    emit guard->frameSubmitted(std::move(sample));
                                }
                        },
                        Qt::QueuedConnection);
        };
    }
    schedule();
    emit telemetryEnabledChanged();
}
QVector3D SceneItem::qtProjection(QVector3D world) const
{
    const auto &scene = input().scene;
    const auto clip = scene.projection * scene.view * QVector4D(world, 1);
    if (clip.w() <= 0)
        return { std::numeric_limits<float>::quiet_NaN(), std::numeric_limits<float>::quiet_NaN(),
                 -1 };
    const auto ndc = clip.toVector3DAffine();
    return { float((ndc.x() + 1) * .5 * scene.width), float((1 - ndc.y()) * .5 * scene.height),
             ndc.z() };
}
void SceneItem::schedule()
{
    // Qt coalesces polish: camera, lighting and quality notifications publish once per frame.
    polish();
}
bool SceneItem::event(QEvent *event)
{
    if (event->type() == QEvent::DevicePixelRatioChange)
        schedule();
    return RhiItem::event(event);
}
void SceneItem::updatePolish()
{
    if (!m_adapter || !window() || !isVisible() || width() <= 0 || height() <= 0)
        return;
    auto &adapter = *m_adapter;
    auto &quality = *adapter.quality();
    quality.resize(width(), height(), m_geometryOnly ? 1 : qMin(width() / 1920, height() / 1080),
                   window()->effectiveDevicePixelRatio(), std::numeric_limits<int>::max());
    const auto &l = *adapter.lighting();
    const auto settings = quality.values();
    Input in;
    in.scene = adapter.frame();
    in.cover = m_cover;
    in.geometryDiagnostic = m_geometryOnly;
    in.shells = !m_geometryOnly;
    in.showPaper = !m_geometryOnly;
    in.captureSamples = 4;
    in.profile = m_telemetryEnabled;
    in.shadows = !m_geometryOnly && quality.shadows() > 0;
    in.shadowSize = qMax(1, quality.shadows());
    in.postprocess = !m_geometryOnly;
    in.aoSamples = m_geometryOnly ? 0 : quality.aoSamples();
    in.aoResolution = float(quality.aoResolution());
    in.bokeh = !m_geometryOnly && settings.value("depthOfField").toDouble() > 0;
    in.smaa = !m_geometryOnly && settings.value("antialias") == "smaa";
    in.focus = float(adapter.focalDepth(adapter.casePosition()));
    in.aperture = float((.0003 + .0005 * adapter.detail())
                        * settings.value("depthOfField").toDouble() / 100);
    in.clarity = float(adapter.detail());
    in.background = l.backgroundLinear();
    in.keyColor = l.keyColor();
    in.skyColor = l.skyColor();
    in.groundColor = l.groundColor();
    in.lightIntensities = l.lightIntensities();
    in.spotPosition = l.spotPosition();
    in.spotTarget = l.spotTarget();
    in.spotColor = l.spotColor();
    in.spotIntensity = float(l.spotIntensity());
    in.environmentIntensity = float(l.environmentIntensity());
    in.rowSettings = l.rowSettings();
    in.rowColumn = l.rowColumn();
    in.rowDetail = float(l.rowDetail());
    in.rowFill = float(l.rowFill());
    in.lightColumn = l.lightColumn();
    in.printAmbient = float(l.printAmbient());
    in.scatterColor = l.scatterColor();
    in.scatterStrength = float(l.scatterStrength());
    in.areaPosition = l.areaPosition();
    in.areaTarget = l.areaTarget();
    in.areaIntensity = float(l.areaIntensity());
    in.warmth = float(l.warmth());
    in.exposure = float(l.exposure());
    const auto target = m_geometryOnly
            ? QSize(qRound(width() * window()->effectiveDevicePixelRatio()),
                    qRound(height() * window()->effectiveDevicePixelRatio()))
            : quality.targetSize();
    const auto &old = input();
    const bool unchanged = in.scene.revision == old.scene.revision
            && in.scene.inputRevision == old.scene.inputRevision
            && in.cover.cacheKey() == old.cover.cacheKey()
            && target == QSize(fixedColorBufferWidth(), fixedColorBufferHeight())
            && in.geometryDiagnostic == old.geometryDiagnostic && in.shells == old.shells
            && in.profile == old.profile && in.shadows == old.shadows
            && in.shadowSize == old.shadowSize && in.captureSamples == old.captureSamples
            && in.aoSamples == old.aoSamples && in.aoResolution == old.aoResolution
            && in.postprocess == old.postprocess && in.bokeh == old.bokeh && in.smaa == old.smaa
            && in.focus == old.focus && in.aperture == old.aperture && in.clarity == old.clarity
            && in.background == old.background && in.keyColor == old.keyColor
            && in.skyColor == old.skyColor && in.groundColor == old.groundColor
            && in.lightIntensities == old.lightIntensities && in.spotPosition == old.spotPosition
            && in.spotTarget == old.spotTarget && in.spotColor == old.spotColor
            && in.spotIntensity == old.spotIntensity
            && in.environmentIntensity == old.environmentIntensity
            && in.rowSettings == old.rowSettings && in.rowColumn == old.rowColumn
            && in.rowDetail == old.rowDetail && in.rowFill == old.rowFill
            && in.lightColumn == old.lightColumn && in.printAmbient == old.printAmbient
            && in.scatterColor == old.scatterColor && in.scatterStrength == old.scatterStrength
            && in.areaPosition == old.areaPosition && in.areaTarget == old.areaTarget
            && in.areaIntensity == old.areaIntensity && in.warmth == old.warmth
            && in.exposure == old.exposure;
    if (unchanged)
        return;
    in.entities = { in.scene.caseTransform };
    in.entityIds = { 1 };
    submit(std::move(in), target);
}
}
