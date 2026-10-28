// Hosts the product self-check commands while the product runtime owns all application services.
#include "ApplicationRuntime.h"
#include "VerificationOptions.h"
#include "AppIdentity.h"
#include "Localization.h"
#include "Logging.h"
#include "VerificationWindow.h"
#include "DeploymentProbe.h"
#include "OverviewReview.h"
#include "DetailReview.h"
#include "NavigationReview.h"
#include "PlaybackController.h"
#ifdef YALAHOO_REVIEW
#  include "MaterialReview.h"
#endif
#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QTimer>
#include <cstdio>
#include <utility>
using namespace Qt::StringLiterals;
namespace {
QQuickItem *findVisualItem(QQuickItem *item, const QString &name)
{
    if (item->objectName() == name)
        return item;
    for (auto *child : item->childItems()) {
        if (auto *found = findVisualItem(child, name))
            return found;
    }
    return nullptr;
}

}
int main(int argc, char **argv)
{
    yalahoo::initializeIdentity();
#ifdef YALAHOO_REVIEW
    constexpr bool ReviewEnabled = true;
#else
    constexpr bool ReviewEnabled = false;
#endif
    // Inspect product arguments without constructing a GUI platform. A Core application
    // is created only for commands that exit here, so normal startup owns one application.
    QStringList rawArguments;
    bool helpRequested = false;
    for (int i = 0; i < argc; ++i) {
        const auto argument = QString::fromLocal8Bit(argv[i]);
        rawArguments.append(argument);
        helpRequested = helpRequested || argument == "--help"_L1 || argument == "-h"_L1
                || argument == "--help-all"_L1;
    }
    if (helpRequested
        || !yalahoo::parseVerificationOptions(rawArguments, ReviewEnabled).error.isEmpty()) {
        QCoreApplication commandLineApp(argc, argv);
        const yalahoo::Localization localization;
        const auto parsed =
                yalahoo::parseVerificationOptions(commandLineApp.arguments(), ReviewEnabled);
        if (!parsed.error.isEmpty()) {
            const auto error = parsed.error.toUtf8();
            std::fwrite(error.constData(), 1, size_t(error.size()), stderr);
            std::fputc('\n', stderr);
            return parsed.errorCode;
        }
        const auto help = parsed.help.toUtf8();
        std::fwrite(help.constData(), 1, size_t(help.size()), stdout);
        return 0;
    }
    const auto parsed = yalahoo::parseVerificationOptions(rawArguments, ReviewEnabled);
    const auto &options = *parsed.options;
    yalahoo::ApplicationLaunch launch;
    const bool verificationRun = options.verificationRun();
    const bool appVerification = options.appVerification();
    launch.persistent = options.persistent();
    launch.automaticPresentation = !verificationRun;
    launch.bindLibrary = !verificationRun || appVerification;
    launch.bindOpening = !verificationRun || options.verification == yalahoo::VerificationMode::App;
    launch.anonymousLibrary = verificationRun || options.product.demonstration();
    QStringList deploymentWarnings;
    if (verificationRun) {
        yalahoo::verification::prepareApplication();
        launch.initialProperties.insert(u"flags"_s,
                                        int(yalahoo::verification::windowFlags(appVerification)));
        launch.observeEngine = [&](QQmlApplicationEngine &engine) {
            QObject::connect(&engine, &QQmlEngine::warnings, &engine,
                             [&](const QList<QQmlError> &warnings) {
                                 for (const auto &warning : warnings)
                                     deploymentWarnings.append(warning.toString());
                             });
        };
        launch.beforeLoad = [&](yalahoo::ApplicationContext &context) -> std::optional<int> {
#ifdef YALAHOO_REVIEW
            if (options.verification == yalahoo::VerificationMode::Materials)
                return yalahoo::reviewMaterials(context.app, context.scene,
                                                options.verificationInput, options.materialReport);
#else
            Q_UNUSED(context);
#endif
            return std::nullopt;
        };
        launch.afterLoad = [&](yalahoo::ApplicationContext &context) -> std::optional<int> {
            auto &app = context.app;
            auto &engine = context.engine;
            auto &scene = context.scene;
            auto &overview = context.overview;
            auto &presentation = context.presentation;
            auto &library = context.library;
            auto &motion = context.motion;
            auto &inputRouter = context.input;
            auto &rulerController = context.ruler;
            yalahoo::verification::prepareWindow(*context.window, appVerification);
            if (context.window->isVisible()
                || !yalahoo::verification::windowPolicySatisfied(*context.window))
                return 7;
            if (options.verification == yalahoo::VerificationMode::App) {
                QTimer::singleShot(0, &engine, [&context, &deploymentWarnings] {
                    auto *surface =
                            findVisualItem(context.window->contentItem(), u"displaySurface"_s);
                    auto *overlay =
                            findVisualItem(context.window->contentItem(), u"displayOverlay"_s);
                    auto *sceneItem =
                            findVisualItem(context.window->contentItem(), u"nativeScene"_s);
                    const bool passed = deploymentWarnings.isEmpty() && surface && overlay
                            && sceneItem && overlay->parentItem() == surface
                            && surface->isAncestorOf(sceneItem)
                            && surface->size() == context.window->size()
                            && !context.window->isVisible()
                            && yalahoo::verification::windowPolicySatisfied(*context.window)
                            && context.playback.source().isEmpty() && !context.playback.requested();
                    if (!passed)
                        qCCritical(yalahoo::logApp)
                                << "Hidden product QML creation or composition failed"
                                << deploymentWarnings;
                    context.app.exit(passed ? 0 : 25);
                });
                return app.exec();
            }
            if (options.verification == yalahoo::VerificationMode::Deployment)
                return yalahoo::verifyDeployment(app, *context.window, options.verificationInput,
                                                 deploymentWarnings);
            if (options.verification == yalahoo::VerificationMode::Overview)
                return yalahoo::reviewOverview(app, *context.window, scene, overview, presentation,
                                               library, motion, options.verificationInput);
            if (options.verification == yalahoo::VerificationMode::Detail)
                return yalahoo::reviewDetail(app, *context.window, scene, library, presentation,
                                             motion, options.verificationInput);
            if (options.verification == yalahoo::VerificationMode::Navigation)
                return yalahoo::reviewNavigation(app, *context.window, scene, library, inputRouter,
                                                 rulerController, options.verificationInput);
            return std::nullopt;
        };
    }
    return yalahoo::runApplication(argc, argv, options.product, launch);
}
