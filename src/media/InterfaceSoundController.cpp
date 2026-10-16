#include "InterfaceSoundController.h"
#include <QFile>
#include <QJsonDocument>
#include <QJsonArray>
#include <QJsonObject>
#include <QTimer>
#include <algorithm>
#include <cmath>

namespace yalahoo {
InterfaceSoundController::InterfaceSoundController(QObject *parent)
    : QObject(parent), m_devices(this)
{
    // One small, trusted embedded manifest is read at construction, never on an event.
    QFile manifest(QStringLiteral(":/assets/audio/interface-events.json"));
    if (manifest.open(QIODevice::ReadOnly)) {
        const auto events =
                QJsonDocument::fromJson(manifest.readAll()).object().value("events").toArray();
        for (const auto &value : events) {
            const auto entry = value.toObject();
            const QUrl source(entry.value("source").toString());
            if (entry.value("enabled").toBool() && source.scheme() == QLatin1String("qrc")
                && source.path().startsWith(QLatin1String("/assets/audio/")) && !source.hasQuery()
                && !source.hasFragment() && QFile::exists(":" + source.path()))
                m_sources.insert(entry.value("event").toString(), source);
        }
    }
    if (available()) {
        for (auto &effect : m_pool) {
            effect = std::make_unique<QSoundEffect>();
            effect->setLoopCount(1);
            effect->setVolume(float(m_volume));
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
    emit settingsChanged();
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
    emit settingsChanged();
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
    trigger(QStringLiteral("confirm"));
}
}
