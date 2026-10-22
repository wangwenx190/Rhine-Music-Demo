#include "AppIdentity.h"
#include "AssetCatalog.h"
#include "BackendCapabilities.h"
#include "GraphicsBackend.h"
#include "Localization.h"
#include "OffscreenSurface.h"
#include "RulerSourceProbe.h"
#include "TitleSourceProbe.h"
#include "VerificationWindow.h"
#include <QGuiApplication>
#include <QScreen>
#include <cstdio>

using namespace Qt::StringLiterals;

int main(int argc, char **argv)
{
    yalahoo::verification::prepareApplication();
    yalahoo::initializeIdentity();
    if (!yalahoo::selectGraphicsBackend())
        return 2;
    QGuiApplication app(argc, argv);
    const yalahoo::Localization localization(QLocale(QLocale::Chinese, QLocale::China));
    if (!yalahoo::probeOptionalGraphicsBackend())
        return 22;
    if (qEnvironmentVariableIntValue("YALAHOO_VERIFY_VISIBLE") == 1)
        OffscreenSurface::Enabled = false;
    const auto arguments = app.arguments();
    if (arguments.size() == 2 && arguments[1] == "--native-dpr"_L1) {
        if (!app.primaryScreen())
            return 3;
        std::printf("%.9f\n", app.primaryScreen()->devicePixelRatio());
        return 0;
    }
    if (!yalahoo::initializeAssetFonts())
        return 3;
    if (arguments.size() == 4 && arguments[1] == "--titles"_L1)
        return probeTitleSource(app, arguments[2], arguments[3]);
    if (arguments.size() == 4 && arguments[1] == "--ruler"_L1)
        return probeRulerSource(app, arguments[2], arguments[3]);
    return 2;
}
