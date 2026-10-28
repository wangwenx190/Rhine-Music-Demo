// Installs native macOS commands and routes editing to the focused Qt editor. Menu ownership and
// callbacks remain tied to the application window lifetime.

#pragma once
#include <QtClassHelperMacros>
#include <functional>
#include <memory>

class QQuickWindow;

namespace yalahoo {
// The system menu is independent of the replaceable QML visual tree.
class ApplicationMenu final
{
    Q_DISABLE_COPY_MOVE(ApplicationMenu)
public:
    ApplicationMenu(QQuickWindow &window, std::function<void()> settings,
                    std::function<bool()> settingsEnabled);
    ~ApplicationMenu();

private:
    struct Private;
    std::unique_ptr<Private> d;
};
}
