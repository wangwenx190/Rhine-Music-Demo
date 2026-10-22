// Internal implementation detail. Include only from the owner, its collaborating peer or tests.
#pragma once
#include "InterfaceSoundController.h"
#include <QTimer>
#include "ProductDefaults.h"
#include <QMediaDevices>
#include <QAudioDevice>
#include <QSoundEffect>
#include <QHash>
#include <array>
#include <memory>
namespace yalahoo {
struct InterfaceSoundController::Private final
{
    Q_DISABLE_COPY_MOVE(Private)
    explicit Private(InterfaceSoundController *owner)
        : q(owner), m_devices(owner), m_outputRefresh(owner)
    {
    }
    InterfaceSoundController *const q;

    void refreshOutput();
    QMediaDevices m_devices;
    QTimer m_outputRefresh;
    QHash<QString, QUrl> m_sources;
    std::array<std::unique_ptr<QSoundEffect>, feedbackAudio::VoiceCount> m_pool;
    double m_volume = yalahoo::productDefaults::DefaultFeedbackVolume;
    bool m_enabled = true;
};
}
