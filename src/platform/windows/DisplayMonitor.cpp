#include "DisplayState.h"
#include <QAbstractNativeEventFilter>
#include <QGuiApplication>
#include <QScreen>
#include <QWindow>
#include <QtGui/qscreen_platform.h>
#include <windows.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <algorithm>
#include <vector>

namespace yalahoo {
class WindowsDisplayMonitor final : public DisplayMonitor, public QAbstractNativeEventFilter
{
    Q_DISABLE_COPY_MOVE(WindowsDisplayMonitor)
public:
    WindowsDisplayMonitor()
    {
        qGuiApp->installNativeEventFilter(this);
    }
    ~WindowsDisplayMonitor() override
    {
        qGuiApp->removeNativeEventFilter(this);
    }
    bool nativeEventFilter(const QByteArray &, void *message, qintptr *) override
    {
        const auto *event = static_cast<MSG *>(message);
        if (!m_pending
            && (event->message == WM_DISPLAYCHANGE || event->message == WM_SETTINGCHANGE
                || event->message == WM_DEVICECHANGE)) {
            // Never rebuild a native window inside the Win32 message dispatch.
            m_pending = true;
            QMetaObject::invokeMethod(
                    this,
                    [this] {
                        m_pending = false;
                        if (changed)
                            changed();
                    },
                    Qt::QueuedConnection);
        }
        return false;
    }
    DisplayState query(QWindow &window) override
    {
        auto *screen = window.screen();
        auto *native =
                screen ? screen->nativeInterface<QNativeInterface::QWindowsScreen>() : nullptr;
        if (!native)
            return { };
        const HMONITOR outputMonitor = window.handle()
                ? MonitorFromWindow(reinterpret_cast<HWND>(window.winId()),
                                    MONITOR_DEFAULTTONEAREST)
                : native->handle();
        MONITORINFOEXW monitor{ };
        monitor.cbSize = sizeof(monitor);
        if (!GetMonitorInfoW(outputMonitor, reinterpret_cast<MONITORINFO *>(&monitor)))
            return { };
        UINT32 pathCount = 0, modeCount = 0;
        std::vector<DISPLAYCONFIG_PATH_INFO> paths;
        std::vector<DISPLAYCONFIG_MODE_INFO> modes;
        LONG status = ERROR_INSUFFICIENT_BUFFER;
        for (int retry = 0; retry < 3 && status == ERROR_INSUFFICIENT_BUFFER; ++retry) {
            if (GetDisplayConfigBufferSizes(QDC_ONLY_ACTIVE_PATHS, &pathCount, &modeCount)
                != ERROR_SUCCESS)
                return { };
            paths.resize(pathCount);
            modes.resize(modeCount);
            status = QueryDisplayConfig(QDC_ONLY_ACTIVE_PATHS, &pathCount, paths.data(), &modeCount,
                                        modes.data(), nullptr);
        }
        if (status != ERROR_SUCCESS)
            return { };
        float white = 0;
        for (UINT32 i = 0; i < pathCount; ++i) {
            const auto &path = paths[i];
            DISPLAYCONFIG_SOURCE_DEVICE_NAME source{ };
            source.header = { DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME, sizeof(source),
                              path.sourceInfo.adapterId, path.sourceInfo.id };
            if (DisplayConfigGetDeviceInfo(&source.header) != ERROR_SUCCESS
                || wcscmp(source.viewGdiDeviceName, monitor.szDevice) != 0)
                continue;
            // GET_ADVANCED_COLOR_INFO alone includes WCG SDR. DXGI below proves
            // that this specific output is currently in HDR (PQ/Rec.2020).
            DISPLAYCONFIG_SDR_WHITE_LEVEL level{ };
            level.header = { DISPLAYCONFIG_DEVICE_INFO_GET_SDR_WHITE_LEVEL, sizeof(level),
                             path.targetInfo.adapterId, path.targetInfo.id };
            if (DisplayConfigGetDeviceInfo(&level.header) == ERROR_SUCCESS)
                white = float(level.SDRWhiteLevel) * 80 / 1000;
            break;
        }
        if (white <= 0)
            return { };
        using Microsoft::WRL::ComPtr;
        ComPtr<IDXGIFactory1> factory;
        if (FAILED(CreateDXGIFactory1(IID_PPV_ARGS(&factory))))
            return { };
        for (UINT index = 0;; ++index) {
            ComPtr<IDXGIAdapter1> adapter;
            if (factory->EnumAdapters1(index, &adapter) != S_OK)
                break;
            for (UINT outputIndex = 0;; ++outputIndex) {
                ComPtr<IDXGIOutput> output;
                if (adapter->EnumOutputs(outputIndex, &output) != S_OK)
                    break;
                ComPtr<IDXGIOutput6> hdrOutput;
                DXGI_OUTPUT_DESC1 description{ };
                if (FAILED(output.As(&hdrOutput)) || FAILED(hdrOutput->GetDesc1(&description))
                    || description.Monitor != outputMonitor)
                    continue;
                if (description.ColorSpace != DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020
                    || description.MaxLuminance <= 0)
                    return { };
                return { true, false, white / 80,
                         std::max(1.0f, description.MaxLuminance / white) };
            }
        }
        return { };
    }

private:
    bool m_pending = false;
};
std::unique_ptr<DisplayMonitor> makeDisplayMonitor()
{
    return std::make_unique<WindowsDisplayMonitor>();
}
}
