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

using namespace yalahoo;
namespace {
QQuickItem *find(QQuickItem *item, const QString &name)
{
    if (item->objectName() == name)
        return item;
    for (auto *child : item->childItems())
        if (auto *found = find(child, name))
            return found;
    return nullptr;
}
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
    const QStringList names{ "晚风与留声机", "下伸 gyp (Live)", "一段非常长的收藏列名 · Archive",
                             "声音与记忆",   "夜色唱片",        "旧日旋律",
                             "静静聆听",     "收藏里的微光",    "匿名歌手" };
    for (int lane = 0; lane < names.size(); ++lane) {
        LibraryColumn column{ QString::number(lane), names[lane], { } };
        for (int row = 0; row < lane * 3 + 1; ++row)
            column.albumIds.append(QString("a%1-%2").arg(lane).arg(row));
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
    QQmlComponent component(&engine, QUrl("qrc:/qt/qml/Yalahoo/Music/OverviewVisual.qml"));
    std::unique_ptr<QObject> object(component.createWithInitialProperties(
            { { "overview", QVariant::fromValue(&overview) },
              { "inputRouter", QVariant::fromValue(&input) } }));
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
               && timer.elapsed() < 5000)
            QTest::qWait(10);
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
        if (!tile.save(output + (night ? "/tile-night.png" : "/tile-day.png")))
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
                                      night ? "tile-night.png" : "tile-day.png")));
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
                    anchors.append(QJsonObject{ { "lane", c.lane },
                                                { "name", c.name },
                                                { "count", c.count },
                                                { "x", c.x },
                                                { "y", c.y },
                                                { "extent", c.extent },
                                                { "selected", c.selected } });
                for (int row = 0; row < overview.labels()->rowCount(); ++row) {
                    const auto index = overview.labels()->index(row);
                    if (!overview.labels()->data(index, OverviewLabelsModel::Interactive).toBool())
                        continue;
                    const int lane =
                            overview.labels()->data(index, OverviewLabelsModel::Lane).toInt();
                    auto *label = find(root, "overviewLabel-" + QString::number(lane));
                    if (!label)
                        return 4;
                    const QString suffix = QString::number(lane);
                    auto *anchor = find(root, "overviewAnchor-" + suffix);
                    auto *card = find(root, "overviewCard-" + suffix);
                    auto *name = find(root, "overviewName-" + suffix);
                    auto *count = find(root, "overviewCount-" + suffix);
                    auto *entry = find(root, "overviewEnter-" + suffix);
                    auto *backdrop = find(root, "overviewBackdrop-" + suffix);
                    if (!anchor || !card || !name || !count || !entry || !backdrop)
                        return 4;
                    const auto rectangle = [](QQuickItem *item, QQuickItem *relative) {
                        const auto rect = item->mapRectToItem(relative, item->boundingRect());
                        return QJsonObject{ { "x", rect.x() },
                                            { "y", rect.y() },
                                            { "width", rect.width() },
                                            { "height", rect.height() } };
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
                        auto *texture = find(backdrop, textureName);
                        if (!texture)
                            return 4;
                        const auto target = texture->property("textureSize").toSize();
                        structurePassed =
                                target == QSize(int(labelWidth + 148), 129) && structurePassed;
                        textures.append(QJsonObject{ { "width", target.width() },
                                                     { "height", target.height() } });
                    }
                    passed = structurePassed && passed;
                    structures.append(QJsonObject{ { "lane", lane },
                                                   { "expanded", entryWidth > 0 },
                                                   { "passed", structurePassed },
                                                   { "card", rectangle(card, anchor) },
                                                   { "label", rectangle(label, card) },
                                                   { "name", rectangle(name, card) },
                                                   { "count", rectangle(count, card) },
                                                   { "entry", rectangle(entry, card) },
                                                   { "blurTextures", textures } });
                    if (expanded < 0)
                        expanded = lane;
                    const auto box = label->mapRectToScene(label->boundingRect());
                    rois.append(QJsonObject{
                            { "lane", lane },
                            { "x", box.x() },
                            { "y", box.y() },
                            { "width", box.width() + (variant && expanded == lane ? 76 : 0) },
                            { "height", box.height() } });
                }
                const auto file = QString("overview-%1.png").arg(captures.size());
                const auto image = surface.grab();
                passed = !image.isNull() && image.save(output + '/' + file) && passed;
                captures.append(QJsonObject{ { "width", size.width() },
                                             { "height", size.height() },
                                             { "night", night },
                                             { "dpr", window.effectiveDevicePixelRatio() },
                                             { "file", file },
                                             { "expanded", variant ? expanded : -1 },
                                             { "columns", anchors },
                                             { "structures", structures },
                                             { "rois", rois } });
                if (expanded >= 0) {
                    const auto before = selection.albumId();
                    auto *button = find(root,
                                        (variant ? "overviewEnter-" : "overviewLabel-")
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
                { { "overview", QVariant::fromValue(&overview) },
                  { "inputRouter", QVariant::fromValue(&input) },
                  { "night", true },
                  { "backgroundUrl",
                    QUrl::fromLocalFile(QDir(output).absoluteFilePath("tile-night.png")) } }));
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
    QJsonObject report{ { "passed", passed },
                        { "configuration", "Release" },
                        { "qtVersion", qVersion() },
                        { "backend", qEnvironmentVariable("QSG_RHI_BACKEND") },
                        { "offscreen", OffscreenSurface::Enabled },
                        { "qmlWarnings", warnings.size() },
                        { "captures", captures },
                        { "expansionClicks", interactions },
                        { "entryClicks", entries },
                        { "lifecycle", lifecycle },
                        { "requestedLifecycleCycles", requestedCycles },
                        { "retired", retired },
                        { "resourcePlan", surface.diagnostics() },
                        { "windowPolicy", verification::windowPolicy(window) } };
    QSaveFile file(output + "/report.json");
    if (!file.open(QIODevice::WriteOnly))
        return 4;
    file.write(QJsonDocument(report).toJson());
    return file.commit() && passed ? 0 : 6;
}
