#pragma once
#include <functional>
#include <memory>

class QQuickWindow;

namespace yalahoo {
// The system menu is independent of the replaceable QML visual tree.
class ApplicationMenu final
{
public:
    ApplicationMenu(QQuickWindow &window, std::function<void()> settings,
                    std::function<bool()> settingsEnabled);
    ~ApplicationMenu();
    ApplicationMenu(const ApplicationMenu &) = delete;
    ApplicationMenu &operator=(const ApplicationMenu &) = delete;

private:
    struct State;
    std::unique_ptr<State> m_state;
};
}
