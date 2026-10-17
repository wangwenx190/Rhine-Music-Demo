#include "InterfaceSoundController.h"
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
    : QObject(parent), m_devices(this)
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
                m_sources.insert(entry.value(u"event"_s).toString(), source);
        }
    }
    if (available()) {
        for (auto &effect : m_pool) {
            effect = std::make_unique<QSoundEffect>();
            effect->setLoopCount(1);
            effect->setVolume(float(m_volume));
            connect(effect.get(), &QSoundEffect::statusChanged, this, [sound = effect.get()] {
                if (sound->status() == QSoundEffect::Error)
                    qCWarning(logMedia) << "Interface sound error" << "source=" << sound->source()
                                        << "device=" << sound->audioDevice().description();
            });
        }
    }
    connect(&m_devices, &QMediaDevices::audioOutputsChanged, this, [this] {
        if (m_refreshPending)
            return;
        m_refreshPending = true;
        QTimer::singleShot(0, this, [this] {
            m_refreshPending = false;
            refreshOutput();
        });
    });
    refreshOutput();
}
InterfaceSoundController::~InterfaceSoundController()
{
    stop();
}
void InterfaceSoundController::stop()
{
    for (auto &effect : m_pool)
        if (effect)
            effect->stop();
}
void InterfaceSoundController::refreshOutput()
{
    const auto device = QMediaDevices::defaultAudioOutput();
    stop();
    for (auto &effect : m_pool)
        if (effect)
            effect->setAudioDevice(device);
}
void InterfaceSoundController::setEnabled(bool enabled)
{
    if (m_enabled == enabled)
        return;
    m_enabled = enabled;
    if (!enabled)
        stop();
    Q_EMIT settingsChanged();
}
void InterfaceSoundController::setVolume(double volume)
{
    if (!std::isfinite(volume))
        return;
    volume = std::clamp(volume, 0., 1.);
    if (m_volume == volume)
        return;
    m_volume = volume;
    for (auto &effect : m_pool)
        if (effect)
            effect->setVolume(float(volume));
    Q_EMIT settingsChanged();
}
bool InterfaceSoundController::trigger(const QString &event)
{
    const auto source = m_sources.constFind(event);
    if (!m_enabled || m_volume == 0 || source == m_sources.cend())
        return false;
    for (auto &effect : m_pool) {
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
}
