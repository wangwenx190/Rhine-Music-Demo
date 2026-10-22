#include "ReviewSupport.h"
#include "Localization.h"
#include "CoverCache.h"
#include "OpeningChecks.h"
#include "StartupChecks.h"
#include "ExperimentChecks.h"
#include "ControlsChecks.h"
#include "LibraryChecks.h"
#include "OffscreenSurface.h"
#include "AssetCatalog.h"
#include "AppIdentity.h"
#include "PresentationChecks.h"
#include "OverviewChecks.h"
#include "GraphicsBackend.h"
#include "VerificationWindow.h"
#include <QGuiApplication>
#include <QScopeGuard>
#include <QScreen>
#include <QElapsedTimer>
#include <cstdio>

using namespace Qt::StringLiterals;
namespace {
constexpr auto NanosecondsPerMillisecond = 1e6;
void message(QtMsgType, const QMessageLogContext &, const QString &text)
{
    std::fprintf(stderr, "%s\n", text.toUtf8().constData());
}
}

int main(int argc, char **argv)
{
    QElapsedTimer bootstrapTimer;
    bootstrapTimer.start();
    yalahoo::verification::prepareApplication();
    if (!yalahoo::selectGraphicsBackend())
        return 2;
    yalahoo::initializeIdentity();
    QGuiApplication app(argc, argv);
    const yalahoo::Localization localization(QLocale(QLocale::Chinese, QLocale::China));
    if (!yalahoo::probeOptionalGraphicsBackend())
        return 22;
    const auto coverShutdown = qScopeGuard([] {
        yalahoo::CoverCache::finishDecoders();
    });
    const auto applicationNs = bootstrapTimer.nsecsElapsed();
    if (qEnvironmentVariableIntValue("YALAHOO_VERIFY_VISIBLE") == 1)
        OffscreenSurface::Enabled = false;
    if (argc == 2 && QString::fromLocal8Bit(argv[1]) == "--native-dpr"_L1) {
        std::printf("%.9f\n", app.primaryScreen()->devicePixelRatio());
        return 0;
    }
    if (argc < 2 || !yalahoo::initializeAssetFonts())
        return 2;
    const auto fontsNs = bootstrapTimer.nsecsElapsed();
    qputenv("QCPAINTER_DEBUG_COLLECT", "0"_ba);
    const auto previousMessageHandler = qInstallMessageHandler(message);
    // Restore before application teardown or function-static diagnostic storage destruction.
    // Every early return after installation follows this same ownership boundary.
    const auto restoreMessageHandler = qScopeGuard([previousMessageHandler] {
        qInstallMessageHandler(previousMessageHandler);
    });
    if (app.arguments().contains(u"--controls"_s))
        return argc == 3 ? reviewControls(QString::fromLocal8Bit(argv[2])) : 2;
    if (app.arguments().contains(u"--experiments"_s))
        return argc == 3 ? reviewExperiments(QString::fromLocal8Bit(argv[2])) : 2;
    if (app.arguments().contains(u"--settings"_s)) {
        const bool representative =
                argc == 4 && QString::fromLocal8Bit(argv[3]) == "--representative"_L1;
        return argc == 3 || representative
                ? reviewLibrary(app, QString::fromLocal8Bit(argv[2]), true, true, representative)
                : 2;
    }
    if (app.arguments().contains(u"--playback"_s))
        return argc == 3 ? reviewLibrary(app, QString::fromLocal8Bit(argv[2]), true) : 2;
    if (app.arguments().contains(u"--library"_s)) {
        const bool representative =
                argc == 4 && QString::fromLocal8Bit(argv[3]) == "--representative"_L1;
        return argc == 3 || representative
                ? reviewLibrary(app, QString::fromLocal8Bit(argv[2]), false, false, representative)
                : 2;
    }
    if (app.arguments().contains(u"--gui"_s)) {
        const bool narrow = argc == 4 && QString::fromLocal8Bit(argv[3]) == "narrow"_L1;
        const bool representative =
                argc == 4 && QString::fromLocal8Bit(argv[3]) == "--representative"_L1;
        return argc == 3 || narrow || representative
                ? reviewGui(QString::fromLocal8Bit(argv[2]), narrow, representative)
                : 2;
    }
    if (app.arguments().contains(u"--window-lifecycle"_s))
        return argc == 3 ? reviewWindowLifecycle(QString::fromLocal8Bit(argv[2])) : 2;
    if (app.arguments().contains(u"--startup"_s)) {
        bool validSeconds = true;
        const double seconds =
                argc >= 4 ? QString::fromLocal8Bit(argv[3]).toDouble(&validSeconds) : 0;
        const bool observeOnly = argc == 6 && QString::fromLocal8Bit(argv[5]) == "observe"_L1;
        return (argc == 3 || argc == 4 || argc == 5 || observeOnly) && validSeconds
                ? reviewStartup(
                          app, QString::fromLocal8Bit(argv[2]), seconds,
                          { { u"applicationSetupMs"_s, applicationNs / NanosecondsPerMillisecond },
                            { u"fontAndArgumentSetupMs"_s,
                              (fontsNs - applicationNs) / NanosecondsPerMillisecond },
                            { u"scope"_s,
                              u"Review main entry through QGuiApplication and embedded font/argument setup; excludes pre-main loader, anonymous fixture generation, model/cover/render preparation and native presentation."_s } },
                          argc >= 5 ? QString::fromLocal8Bit(argv[4]) : u"matrix"_s, observeOnly)
                : 2;
    }
    if (app.arguments().contains(u"--opening"_s)) {
        const bool representative =
                argc == 4 && QString::fromLocal8Bit(argv[3]) == "--representative"_L1;
        return argc == 3 || representative
                ? reviewOpening(app, QString::fromLocal8Bit(argv[2]), representative)
                : 2;
    }
    if (app.arguments().contains(u"--overview"_s))
        return argc == 3 ? reviewOverviewVisual(app, QString::fromLocal8Bit(argv[2])) : 2;
    if (app.arguments().contains(u"--presentation"_s))
        return argc == 3 ? reviewPresentation(app, QString::fromLocal8Bit(argv[2])) : 2;
    return 2;
}
