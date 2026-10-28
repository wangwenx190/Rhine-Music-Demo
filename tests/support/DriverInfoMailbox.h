// One driver value crosses from the render callback to GUI diagnostics. No QRhi or
// window resource escapes its owning thread, including while the surface is destroyed.
#pragma once
#include <QMutex>
#include <QMutexLocker>
#include <rhi/qrhi.h>
#include <optional>
#include <utility>

class DriverInfoMailbox final
{
    Q_DISABLE_COPY_MOVE(DriverInfoMailbox)
public:
    DriverInfoMailbox() = default;
    void publish(QRhiDriverInfo value)
    {
        QMutexLocker lock(&m_mutex);
        m_value = std::move(value);
    }
    std::optional<QRhiDriverInfo> value() const
    {
        QMutexLocker lock(&m_mutex);
        return m_value;
    }

private:
    mutable QMutex m_mutex;
    std::optional<QRhiDriverInfo> m_value;
};
