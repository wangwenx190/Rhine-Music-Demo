#include "DisplayState.h"
#include <QtGui/qtguiglobal.h>
#if QT_CONFIG(wayland)
#  include "color-management-v1-client.h"
#  include <QGuiApplication>
#  include <QScreen>
#  include <QWindow>
#  include <QtGui/qguiapplication_platform.h>
#  include <QtGui/qscreen_platform.h>
#  include <wayland-client.h>
#  include <algorithm>
#  include <cstring>
#  include <unistd.h>

namespace yalahoo {
// Observe the compositor's output description on Qt's existing connection. Qt
// retains sole ownership of the window's color-management surface and dispatch.
class WaylandDisplayMonitor final : public DisplayMonitor
{
public:
    ~WaylandDisplayMonitor() override
    {
        resetOutput();
        if (m_manager)
            wp_color_manager_v1_destroy(m_manager);
        if (m_registry)
            wl_registry_destroy(m_registry);
    }
    DisplayState query(QWindow &window) override
    {
        auto *native = qGuiApp->nativeInterface<QNativeInterface::QWaylandApplication>();
        if (!native)
            return { };
        if (!m_registry) {
            m_registry = wl_display_get_registry(native->display());
            static const wl_registry_listener listener{ registryGlobal, registryRemove };
            wl_registry_add_listener(m_registry, &listener, this);
        }
        auto *screen = window.screen();
        auto *output =
                screen ? screen->nativeInterface<QNativeInterface::QWaylandScreen>() : nullptr;
        auto *handle = output ? output->output() : nullptr;
        if (m_manager && (handle != m_output || !m_colorOutput)) {
            resetOutput();
            m_output = handle;
            if (handle) {
                m_colorOutput = wp_color_manager_v1_get_output(m_manager, handle);
                static const wp_color_management_output_v1_listener listener{ outputChanged };
                wp_color_management_output_v1_add_listener(m_colorOutput, &listener, this);
                requestDescription();
            }
        }
        return m_state;
    }

private:
    void notify()
    {
        if (m_notifyPending)
            return;
        m_notifyPending = true;
        QMetaObject::invokeMethod(
                this,
                [this] {
                    m_notifyPending = false;
                    if (changed)
                        changed();
                },
                Qt::QueuedConnection);
    }
    void resetDescription()
    {
        if (m_info) {
            wp_image_description_info_v1_destroy(m_info);
            m_info = nullptr;
        }
        if (m_description) {
            wp_image_description_v1_destroy(m_description);
            m_description = nullptr;
        }
    }
    void resetOutput()
    {
        resetDescription();
        if (m_colorOutput)
            wp_color_management_output_v1_destroy(m_colorOutput);
        m_colorOutput = nullptr;
        m_output = nullptr;
        m_state = { };
    }
    void requestDescription()
    {
        resetDescription();
        m_description = wp_color_management_output_v1_get_image_description(m_colorOutput);
        static const wp_image_description_v1_listener listener{ descriptionFailed,
                                                                descriptionReady };
        wp_image_description_v1_add_listener(m_description, &listener, this);
    }
    static void registryGlobal(void *data, wl_registry *registry, uint32_t name,
                               const char *interface, uint32_t)
    {
        auto &self = *static_cast<WaylandDisplayMonitor *>(data);
        if (std::strcmp(interface, wp_color_manager_v1_interface.name) != 0 || self.m_manager)
            return;
        self.m_managerId = name;
        self.m_manager = static_cast<wp_color_manager_v1 *>(
                wl_registry_bind(registry, name, &wp_color_manager_v1_interface, 1));
        static const wp_color_manager_v1_listener listener{
            [](void *, wp_color_manager_v1 *, uint32_t) {
            },
            [](void *d, wp_color_manager_v1 *, uint32_t value) {
                auto &s = *static_cast<WaylandDisplayMonitor *>(d);
                if (value == WP_COLOR_MANAGER_V1_FEATURE_PARAMETRIC)
                    s.m_parametric = true;
            },
            [](void *d, wp_color_manager_v1 *, uint32_t value) {
                auto &s = *static_cast<WaylandDisplayMonitor *>(d);
                s.m_linear |= value == WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_LINEAR;
                s.m_pq |= value == WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ;
            },
            [](void *d, wp_color_manager_v1 *, uint32_t value) {
                auto &s = *static_cast<WaylandDisplayMonitor *>(d);
                s.m_srgb |= value == WP_COLOR_MANAGER_V1_PRIMARIES_SRGB;
                s.m_bt2020 |= value == WP_COLOR_MANAGER_V1_PRIMARIES_BT2020;
            },
            [](void *d, wp_color_manager_v1 *) {
                static_cast<WaylandDisplayMonitor *>(d)->notify();
            }
        };
        wp_color_manager_v1_add_listener(self.m_manager, &listener, &self);
    }
    static void registryRemove(void *data, wl_registry *, uint32_t name)
    {
        auto &self = *static_cast<WaylandDisplayMonitor *>(data);
        if (name != self.m_managerId)
            return;
        self.resetOutput();
        wp_color_manager_v1_destroy(self.m_manager);
        self.m_manager = nullptr;
        self.m_parametric = self.m_linear = self.m_pq = self.m_srgb = self.m_bt2020 = false;
        self.notify();
    }
    static void outputChanged(void *data, wp_color_management_output_v1 *)
    {
        static_cast<WaylandDisplayMonitor *>(data)->requestDescription();
    }
    static void descriptionFailed(void *data, wp_image_description_v1 *, uint32_t, const char *)
    {
        auto &self = *static_cast<WaylandDisplayMonitor *>(data);
        self.m_state = { };
        self.notify();
    }
    static void descriptionReady(void *data, wp_image_description_v1 *description, uint32_t)
    {
        auto &self = *static_cast<WaylandDisplayMonitor *>(data);
        self.m_reference = self.m_peak = 0;
        self.m_transfer = 0;
        self.m_info = wp_image_description_v1_get_information(description);
        static const wp_image_description_info_v1_listener listener{
            infoDone,
            [](void *, wp_image_description_info_v1 *, int32_t fd, uint32_t) {
                close(fd);
            },
            [](void *, wp_image_description_info_v1 *, int32_t, int32_t, int32_t, int32_t, int32_t,
               int32_t, int32_t, int32_t) {
            },
            [](void *, wp_image_description_info_v1 *, uint32_t) {
            },
            [](void *, wp_image_description_info_v1 *, uint32_t) {
            },
            [](void *d, wp_image_description_info_v1 *, uint32_t tf) {
                static_cast<WaylandDisplayMonitor *>(d)->m_transfer = tf;
            },
            [](void *d, wp_image_description_info_v1 *, uint32_t, uint32_t, uint32_t reference) {
                static_cast<WaylandDisplayMonitor *>(d)->m_reference = float(reference);
            },
            [](void *, wp_image_description_info_v1 *, int32_t, int32_t, int32_t, int32_t, int32_t,
               int32_t, int32_t, int32_t) {
            },
            [](void *d, wp_image_description_info_v1 *, uint32_t, uint32_t peak) {
                static_cast<WaylandDisplayMonitor *>(d)->m_peak = float(peak);
            },
            [](void *, wp_image_description_info_v1 *, uint32_t) {
            },
            [](void *, wp_image_description_info_v1 *, uint32_t) {
            }
        };
        wp_image_description_info_v1_add_listener(self.m_info, &listener, &self);
    }
    static void infoDone(void *data, wp_image_description_info_v1 *info)
    {
        auto &self = *static_cast<WaylandDisplayMonitor *>(data);
        // The server destroys the info resource with this event; release the
        // local proxy without sending another request to the destroyed object.
        wl_proxy_destroy(reinterpret_cast<wl_proxy *>(info));
        self.m_info = nullptr;
        const bool hdrTransfer = self.m_transfer == WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_ST2084_PQ
                || self.m_transfer == WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_HLG
                || self.m_transfer == WP_COLOR_MANAGER_V1_TRANSFER_FUNCTION_EXT_LINEAR;
        const bool pq = self.m_pq && self.m_bt2020;
        const bool linear = self.m_linear && self.m_srgb;
        if (self.m_parametric && (pq || linear) && hdrTransfer && self.m_reference > 0
            && self.m_peak > self.m_reference) {
            // Prefer PQ/Rec.2020 on Linux HDR compositors. The protocol anchors
            // its standard 203-nit PQ white (80 for linear) to the output white.
            self.m_state = { true, pq, pq ? 203.0f : 1.0f, self.m_peak / self.m_reference };
            self.m_state.allowLinearFallback = linear;
        } else {
            self.m_state = { };
        }
        self.notify();
    }
    wl_registry *m_registry = nullptr;
    wp_color_manager_v1 *m_manager = nullptr;
    wp_color_management_output_v1 *m_colorOutput = nullptr;
    wp_image_description_v1 *m_description = nullptr;
    wp_image_description_info_v1 *m_info = nullptr;
    wl_output *m_output = nullptr;
    uint32_t m_managerId = 0, m_transfer = 0;
    bool m_parametric = false, m_linear = false, m_pq = false, m_srgb = false, m_bt2020 = false;
    float m_reference = 0, m_peak = 0;
    bool m_notifyPending = false;
    DisplayState m_state;
};
std::unique_ptr<DisplayMonitor> makeDisplayMonitor()
{
    return std::make_unique<WaylandDisplayMonitor>();
}
}
#else
namespace yalahoo {
class SdrDisplayMonitor final : public DisplayMonitor
{
public:
    DisplayState query(QWindow &) override
    {
        return { };
    }
};
std::unique_ptr<DisplayMonitor> makeDisplayMonitor()
{
    return std::make_unique<SdrDisplayMonitor>();
}
}
#endif
