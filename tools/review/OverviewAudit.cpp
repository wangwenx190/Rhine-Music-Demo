#include "ReviewSupport.h"
#include "MotionController.h"
#include "SelectionController.h"
#include <QQuickWindow>
#include "OverviewAudit.h"
#include "OverviewController.h"
#include "DetailPorts.h"
#include "InputRouter.h"
#include "SceneAdapter.h"
#include "PresentationController.h"
#include "LibrarySession.h"
#include "OfflineMetadataProvider.h"
#include "OffscreenSurface.h"
#include "VerificationWindow.h"
#include <QDir>
#include <QQmlComponent>
#include <QQmlEngine>
#include <QQuickItem>
#include <QSignalSpy>
#include <QTest>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonArray>
#include <QSaveFile>
#include <QMouseEvent>

namespace {
// Event-driven probe waits are bounded independently of injected fixture values.
constexpr int FrameDeadlineMilliseconds = 5000;
}

namespace {
// Bounds for asynchronous completion and event delivery; fixture values remain independent.
constexpr int EventPollMilliseconds = 10;
}

using namespace Qt::StringLiterals;

using namespace yalahoo;
namespace {
}
int reviewOverviewVisual(QGuiApplication &, const QString &output)
{
    QDir().mkpath(output);
    MotionController motion;
    OverviewController overview(&motion);
    SelectionController selection(&motion);
    SceneAdapter scene(motion.clock());
    InputRouter input(&selection);
    OfflineMetadataProvider provider;
    LibrarySession library(&provider);
    PresentationController presentation(motion.clock(), detailPorts(scene, selection, library));
    QList<LibraryColumn> columns;
    const QStringList names{
        u"\u665a\u98ce\u4e0e\u7559\u58f0\u673a"_s,
        u"\u4e0b\u4f38 gyp (Live)"_s,
        u"\u4e00\u6bb5\u975e\u5e38\u957f\u7684\u6536\u85cf\u5217\u540d \u00b7 Archive"_s,
        u"\u58f0\u97f3\u4e0e\u8bb0\u5fc6"_s,
        u"\u591c\u8272\u5531\u7247"_s,
        u"\u65e7\u65e5\u65cb\u5f8b"_s,
        u"\u9759\u9759\u8046\u542c"_s,
        u"\u6536\u85cf\u91cc\u7684\u5fae\u5149"_s,
        u"\u533f\u540d\u6b4c\u624b"_s
    };
    for (int lane = 0; lane < names.size(); ++lane) {
        LibraryColumn column{ QString::number(lane), names[lane], { } };
        for (int row = 0; row < lane * 3 + 1; ++row)
            column.albumIds.append(u"a%1-%2"_s.arg(lane).arg(row));
        columns.append(column);
    }
    selection.setColumns(columns);
    selection.selectCell({ 4, 12 }, true);
    motion.finish();
    selection.advance(10);
    scene.setSelection(&selection);
    overview.bind(&scene, &selection, &presentation);
    motion.setReducedMotion(true);
    scene.setReducedMotion(true);
    presentation.setReducedMotion(true);
    QQmlEngine engine;
    QSignalSpy warnings(&engine, &QQmlEngine::warnings);
    QQmlComponent component(&engine, QUrl(u"qrc:/qt/qml/Yalahoo/Music/OverviewVisual.qml"_s));
    std::unique_ptr<QObject> object(component.createWithInitialProperties(
            { { u"overview"_s, QVariant::fromValue(&overview) },
              { u"inputRouter"_s, QVariant::fromValue(&input) } }));
    auto *root = qobject_cast<QQuickItem *>(object.get());
    if (!root) {
        qWarning() << component.errors();
        return 3;
    }
    QQuickRenderControl control;
    QQuickWindow window(OffscreenSurface::Enabled ? &control : nullptr);
    verification::prepareWindow(window, true);
    root->setParentItem(window.contentItem());
    OffscreenSurface surface(window, control);
    QSignalSpy frames(&surface, &OffscreenSurface::frameRendered);
    QSignalSpy swaps(&window, &QQuickWindow::frameSwapped);
    const auto render = [&] {
        const int before = OffscreenSurface::Enabled ? frames.size() : swaps.size();
        surface.requestFrame();
        QElapsedTimer timer;
        timer.start();
        while ((OffscreenSurface::Enabled ? frames.size() : swaps.size()) <= before
               && timer.elapsed() < FrameDeadlineMilliseconds)
            QTest::qWait(EventPollMilliseconds);
        return (OffscreenSurface::Enabled ? frames.size() : swaps.size()) > before;
    };
    for (bool night : { false, true }) {
        QImage tile(256, 256, QImage::Format_RGBA8888);
        for (int y = 0; y < 256; ++y)
            for (int x = 0; x < 256; ++x) {
                const int stripe = ((x / 13 + y / 17) % 2) * 90;
                const int r = night ? 12 + stripe : 130 + stripe;
                tile.setPixelColor(x, y,
                                   QColor(r, night ? 24 + y / 3 : 145 + y / 3,
                                          night ? 38 + x / 3 : 160 + x / 3));
            }
        if (!tile.save(output + (night ? "/tile-night.png"_L1 : "/tile-day.png"_L1)))
            return 4;
    }
    QJsonArray captures, interactions, entries;
    bool passed = true;
    const QList<QSize> sizes{ { 1920, 1080 }, { 1280, 720 },  { 995, 858 },
                              { 960, 900 },   { 930, 900 },   { 390, 844 },
                              { 320, 720 },   { 2560, 1080 }, { 3440, 1440 } };
    surface.show();
    for (const auto size : sizes) {
        window.resize(size);
        root->setSize(size);
        scene.layout()->setViewport(size.width(), size.height());
        for (bool night : { false, true }) {
            root->setProperty("night", night);
            root->setProperty("backgroundUrl",
                              QUrl::fromLocalFile(QDir(output).absoluteFilePath(
                                      night ? u"tile-night.png"_s : u"tile-day.png"_s)));
            overview.setActive(true);
            selection.advance(0);
            scene.advance(0);
            overview.synchronize(0, true);
            int expanded = -1;
            for (int variant = 0; variant < 2; ++variant) {
                for (int i = 0; i < 5; ++i)
                    passed = render() && passed;
                QJsonArray anchors, rois, structures;
                for (const auto &c : overview.projectColumns())
                    anchors.append(QJsonObject{ { u"lane"_s, c.lane },
                                                { u"name"_s, c.name },
                                                { u"count"_s, c.count },
                                                { u"x"_s, c.x },
                                                { u"y"_s, c.y },
                                                { u"extent"_s, c.extent },
                                                { u"selected"_s, c.selected } });
                for (int row = 0; row < overview.labels()->rowCount(); ++row) {
                    const auto index = overview.labels()->index(row);
                    if (!overview.labels()->data(index, OverviewLabelsModel::Interactive).toBool())
                        continue;
                    const int lane =
                            overview.labels()->data(index, OverviewLabelsModel::Lane).toInt();
                    auto *label = yalahoo::review::findVisual(
                            root, "overviewLabel-"_L1 + QString::number(lane));
                    if (!label)
                        return 4;
                    const QString suffix = QString::number(lane);
                    auto *anchor = yalahoo::review::findVisual(root, "overviewAnchor-"_L1 + suffix);
                    auto *card = yalahoo::review::findVisual(root, "overviewCard-"_L1 + suffix);
                    auto *name = yalahoo::review::findVisual(root, "overviewName-"_L1 + suffix);
                    auto *count = yalahoo::review::findVisual(root, "overviewCount-"_L1 + suffix);
                    auto *entry = yalahoo::review::findVisual(root, "overviewEnter-"_L1 + suffix);
                    auto *backdrop =
                            yalahoo::review::findVisual(root, "overviewBackdrop-"_L1 + suffix);
                    if (!anchor || !card || !name || !count || !entry || !backdrop)
                        return 4;
                    const auto rectangle = [](QQuickItem *item, QQuickItem *relative) {
                        const auto rect = item->mapRectToItem(relative, item->boundingRect());
                        return QJsonObject{ { u"x"_s, rect.x() },
                                            { u"y"_s, rect.y() },
                                            { u"width"_s, rect.width() },
                                            { u"height"_s, rect.height() } };
                    };
                    const double labelWidth = size.width() <= 600 ? 116 : 160;
                    const double entryWidth = variant && expanded == lane ? 76 : 0;
                    bool structurePassed = std::abs(anchor->x() + labelWidth / 2
                                                    - overview.labels()
                                                              ->data(index, OverviewLabelsModel::X)
                                                              .toDouble())
                                    <= 1
                            && std::abs(anchor->y() + card->height()
                                        - overview.labels()
                                                  ->data(index, OverviewLabelsModel::Y)
                                                  .toDouble())
                                    <= 1
                            && std::abs(label->width() - labelWidth) <= 1
                            && std::abs(card->width() - labelWidth - entryWidth) <= 1
                            && label->isEnabled() && entry->isEnabled() == (entryWidth > 0);
                    QJsonArray textures;
                    for (const auto &textureName :
                         { "overviewCrop", "overviewHorizontalTexture" }) {
                        auto *texture = yalahoo::review::findVisual(backdrop,
                                                                    QString::fromUtf8(textureName));
                        if (!texture)
                            return 4;
                        const auto target = texture->property("textureSize").toSize();
                        structurePassed = target
                                        == QSize(int(labelWidth + 148), 129)
                                                * window.effectiveDevicePixelRatio()
                                && structurePassed;
                        textures.append(QJsonObject{ { u"width"_s, target.width() },
                                                     { u"height"_s, target.height() } });
                    }
                    passed = structurePassed && passed;
                    structures.append(QJsonObject{ { u"lane"_s, lane },
                                                   { u"expanded"_s, entryWidth > 0 },
                                                   { u"passed"_s, structurePassed },
                                                   { u"card"_s, rectangle(card, anchor) },
                                                   { u"label"_s, rectangle(label, card) },
                                                   { u"name"_s, rectangle(name, card) },
                                                   { u"count"_s, rectangle(count, card) },
                                                   { u"entry"_s, rectangle(entry, card) },
                                                   { u"blurTextures"_s, textures } });
                    if (expanded < 0)
                        expanded = lane;
                    const auto box = label->mapRectToScene(label->boundingRect());
                    rois.append(QJsonObject{
                            { u"lane"_s, lane },
                            { u"x"_s, box.x() },
                            { u"y"_s, box.y() },
                            { u"width"_s, box.width() + (variant && expanded == lane ? 76 : 0) },
                            { u"height"_s, box.height() } });
                }
                const auto file = u"overview-%1.png"_s.arg(captures.size());
                const auto image = surface.grab();
                passed = !image.isNull() && image.save(output + '/'_L1 + file) && passed;
                captures.append(QJsonObject{ { u"width"_s, size.width() },
                                             { u"height"_s, size.height() },
                                             { u"night"_s, night },
                                             { u"dpr"_s, window.effectiveDevicePixelRatio() },
                                             { u"file"_s, file },
                                             { u"expanded"_s, variant ? expanded : -1 },
                                             { u"columns"_s, anchors },
                                             { u"structures"_s, structures },
                                             { u"rois"_s, rois } });
                if (expanded >= 0) {
                    const auto before = selection.albumId();
                    auto *button = yalahoo::review::findVisual(
                            root,
                            (variant ? "overviewEnter-"_L1 : "overviewLabel-"_L1)
                                    + QString::number(expanded));
                    if (!button)
                        return 4;
                    const auto point =
                            button->mapToScene({ button->width() / 2, button->height() / 2 });
                    QMouseEvent press(QEvent::MouseButtonPress, point, point, Qt::LeftButton,
                                      Qt::LeftButton, Qt::NoModifier);
                    QMouseEvent release(QEvent::MouseButtonRelease, point, point, Qt::LeftButton,
                                        Qt::NoButton, Qt::NoModifier);
                    QCoreApplication::sendEvent(&window, &press);
                    QCoreApplication::sendEvent(&window, &release);
                    const bool result = variant ? !overview.active() && !presentation.wantsDetail()
                                                : overview.labels()->canEnter(expanded)
                                    && selection.albumId() == before;
                    if (variant)
                        entries.append(result);
                    else
                        interactions.append(result);
                    passed = result && passed;
                    overview.synchronize(0, true);
                }
            }
            overview.setActive(false);
        }
    }
    QJsonArray lifecycle;
    const int requestedCycles =
            qEnvironmentVariableIntValue("YALAHOO_VERIFY_LIFECYCLE") == 1 ? 10 : 0;
    for (int cycle = 0; cycle < requestedCycles; ++cycle) {
        surface.hide();
        object.reset();
        overview.reset();
        window.resize(1280, 720);
        scene.layout()->setViewport(1280, 720);
        object.reset(component.createWithInitialProperties(
                { { u"overview"_s, QVariant::fromValue(&overview) },
                  { u"inputRouter"_s, QVariant::fromValue(&input) },
                  { u"night"_s, true },
                  { u"backgroundUrl"_s,
                    QUrl::fromLocalFile(QDir(output).absoluteFilePath(u"tile-night.png"_s)) } }));
        root = qobject_cast<QQuickItem *>(object.get());
        if (!root)
            return 4;
        root->setParentItem(window.contentItem());
        root->setSize({ 1280, 720 });
        overview.setActive(true);
        scene.advance(0);
        overview.synchronize(0, true);
        surface.show();
        for (int i = 0; i < 5; ++i)
            passed = render() && passed;
        const bool alive =
                overview.labels()->liveCount() > 0 && overview.labels()->liveCount() <= 18;
        lifecycle.append(alive);
        passed = alive && passed;
    }
    overview.setActive(false);
    passed = render() && passed;
    const bool retired = overview.labels()->liveCount() == 0;
    passed = retired && warnings.isEmpty() && verification::windowPolicySatisfied(window) && passed;
    surface.hide();
    object.reset();
    QJsonObject report{ { u"passed"_s, passed },
                        { u"configuration"_s, u"Release"_s },
                        { u"qtVersion"_s, QString::fromUtf8(qVersion()) },
                        { u"backend"_s, qEnvironmentVariable("QSG_RHI_BACKEND") },
                        { u"offscreen"_s, OffscreenSurface::Enabled },
                        { u"qmlWarnings"_s, warnings.size() },
                        { u"captures"_s, captures },
                        { u"expansionClicks"_s, interactions },
                        { u"entryClicks"_s, entries },
                        { u"lifecycle"_s, lifecycle },
                        { u"requestedLifecycleCycles"_s, requestedCycles },
                        { u"retired"_s, retired },
                        { u"resourcePlan"_s, surface.diagnostics() },
                        { u"windowPolicy"_s, verification::windowPolicy(window) } };
    QSaveFile file(output + "/report.json"_L1);
    if (!file.open(QIODevice::WriteOnly))
        return 4;
    file.write(QJsonDocument(report).toJson());
    return file.commit() && passed ? 0 : 6;
}
