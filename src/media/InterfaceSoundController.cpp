#include "InterfaceSoundController.h"
#include "ProductDefaults.h"
#include <QMediaDevices>
#include <QAudioDevice>
#include <QSoundEffect>
#include <QHash>
#include <array>
#include <memory>
#include "InterfaceSoundController_p.h"
#include <utility>
#include "Logging.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;

namespace yalahoo {

InterfaceSoundController::InterfaceSoundController(QObject *parent)
    : QObject(parent), d(std::make_unique<Private>(this))
{
    // One small, trusted embedded manifest is read at construction, never on an event.
    QFile manifest(u":/assets/audio/interface-events.json"_s);
    if (manifest.open(QIODevice::ReadOnly)) {
        const auto events =
                QJsonDocument::fromJson(manifest.readAll()).object().value(u"events"_s).toArray();
        for (const auto &value : events) {
            const auto entry = value.toObject();
            const QUrl source(entry.value(u"source"_s).toString());
            if (entry.value(u"enabled"_s).toBool() && source.scheme() == "qrc"_L1
                && source.path().startsWith("/assets/audio/"_L1) && !source.hasQuery()
                && !source.hasFragment() && QFile::exists(":"_L1 + source.path()))
                d->m_sources.insert(entry.value(u"event"_s).toString(), source);
        }
    }
    if (available()) {
        for (auto &effect : d->m_pool) {
            effect = std::make_unique<QSoundEffect>();
            effect->setLoopCount(1);
            effect->setVolume(float(d->m_volume));
            connect(effect.get(), &QSoundEffect::statusChanged, this, [sound = effect.get()] {
                if (sound->status() == QSoundEffect::Error)
                    qCWarning(logMedia) << "Interface sound error" << "source=" << sound->source()
                                        << "device=" << sound->audioDevice().description();
            });
        }
    }
    d->m_outputRefresh.setSingleShot(true);
    connect(&d->m_devices, &QMediaDevices::audioOutputsChanged, &d->m_outputRefresh,
            qOverload<>(&QTimer::start));
    connect(&d->m_outputRefresh, &QTimer::timeout, this, [this] {
        d->refreshOutput();
    });
    d->refreshOutput();
}

InterfaceSoundController::~InterfaceSoundController()
{
    stop();
}
void InterfaceSoundController::stop()
{
    for (auto &effect : d->m_pool)
        if (effect)
            effect->stop();
}
void InterfaceSoundController::Private::refreshOutput()
{
    const auto device = QMediaDevices::defaultAudioOutput();
    q->stop();
    for (auto &effect : m_pool)
        if (effect)
            effect->setAudioDevice(device);
}
void InterfaceSoundController::setEnabled(bool enabled)
{
    if (d->m_enabled == enabled)
        return;
    d->m_enabled = enabled;
    if (!enabled)
        stop();
    Q_EMIT settingsChanged();
}
void InterfaceSoundController::setVolume(double volume)
{
    if (!std::isfinite(volume))
        return;
    volume = std::clamp(volume, 0., 1.);
    if (d->m_volume == volume)
        return;
    d->m_volume = volume;
    for (auto &effect : d->m_pool)
        if (effect)
            effect->setVolume(float(volume));
    Q_EMIT settingsChanged();
}
bool InterfaceSoundController::trigger(const QString &event)
{
    const auto source = d->m_sources.constFind(event);
    if (!d->m_enabled || d->m_volume == 0 || source == d->m_sources.cend())
        return false;
    for (auto &effect : d->m_pool) {
        if (effect->isPlaying() || effect->status() == QSoundEffect::Loading
            || effect->audioDevice().isNull())
            continue;
        if (effect->source() != *source)
            effect->setSource(*source);
        effect->play();
        return true;
    }
    // Saturation drops the event: no pending queue or new media instances.
    return false;
}
void InterfaceSoundController::preview()
{
    trigger(u"confirm"_s);
}
bool InterfaceSoundController::enabled() const
{
    return d->m_enabled;
}
double InterfaceSoundController::volume() const
{
    return d->m_volume;
}
bool InterfaceSoundController::available() const
{
    return !d->m_sources.isEmpty();
}

}
