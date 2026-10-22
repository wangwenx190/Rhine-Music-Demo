// Coordinates browse, detail and modal presentation around stable album identity. Revisions
// prevent interrupted readiness work from revealing stale content.

#pragma once
#include <memory>
#include <QString>
using Qt::StringLiterals::operator""_s;
using Qt::StringLiterals::operator""_L1;
#include "SurfaceTransition.h"
#include <optional>

namespace yalahoo {
namespace presentationMotion {
// Surface and tab tracks use independent source-authored time windows.
inline constexpr auto BrowseEnterMilliseconds = 720;
inline constexpr auto BrowseExitMilliseconds = 140;
inline constexpr auto DetailEnterMilliseconds = 360;
inline constexpr auto DetailExitMilliseconds = 240;
inline constexpr auto TabSlideSeconds = .18;
inline constexpr auto TabInitialOpacity = .35;
inline constexpr auto TabFadeSeconds = .15;
}

struct PresentationSelection final
{
    QString albumId, trackId;
    bool archiveRoute = false;
    bool operator==(const PresentationSelection &) const = default;
};
struct PresentationPorts final
{
    std::function<bool()> presentationReady, archiveReady, archiveInteractive;
    std::function<void()> enterCamera, returnCamera;
    std::function<void(const PresentationSelection &)> select, switchDetail;
    std::function<bool(const PresentationSelection &)> canSelect;
    std::function<QString(const QString &, int)> adjacentAlbum;
    std::function<bool(const QString &, int)> navigationEnabled;
};
// GUI-thread owner. Readiness is supplied by the scene; elapsed time never substitutes for it.
class PresentationController final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(PresentationController)
    Q_PROPERTY(double tabOpacity READ tabOpacity NOTIFY tabIndicatorChanged FINAL)
    Q_PROPERTY(double tabX READ tabX NOTIFY tabIndicatorChanged FINAL)
    Q_PROPERTY(double tabWidth READ tabWidth NOTIFY tabIndicatorChanged FINAL)
    Q_PROPERTY(QString phase READ phase NOTIFY changed FINAL)
    Q_PROPERTY(bool wantsDetail READ wantsDetail NOTIFY changed FINAL)
    Q_PROPERTY(bool archiveEnabled READ archiveEnabled NOTIFY changed FINAL)
    Q_PROPERTY(bool detailEnabled READ detailEnabled NOTIFY changed FINAL)
    Q_PROPERTY(bool canMovePrevious READ canMovePrevious NOTIFY changed FINAL)
    Q_PROPERTY(bool canMoveNext READ canMoveNext NOTIFY changed FINAL)
    Q_PROPERTY(quint64 layoutRevision READ layoutRevision NOTIFY prepared FINAL)
    Q_PROPERTY(yalahoo::SurfaceTransition *browseSurface READ browseSurface CONSTANT FINAL)
    Q_PROPERTY(yalahoo::SurfaceTransition *detailSurface READ detailSurface CONSTANT FINAL)
public:
    explicit PresentationController(FrameClock *clock, PresentationPorts ports,
                                    QObject *parent = nullptr);
    ~PresentationController() override;
    QString phase() const;
    bool wantsDetail() const;
    bool archiveEnabled() const;
    bool detailEnabled() const;
    quint64 layoutRevision() const;
    SurfaceTransition *browseSurface();
    SurfaceTransition *detailSurface();
    std::optional<PresentationSelection> pendingSelection() const;
    Q_INVOKABLE void open();
    Q_INVOKABLE void back();
    Q_INVOKABLE void select(const QString &albumId, bool openAfter, bool archiveRoute = false,
                            const QString &trackId = { });
    bool canMovePrevious() const;
    bool canMoveNext() const;
    Q_INVOKABLE void stepAlbum(int direction);
    Q_INVOKABLE bool canStepAlbum(int direction) const;
    Q_INVOKABLE void layoutReady(quint64 revision);
    Q_INVOKABLE void reset();
    void update();
    double tabOpacity() const;
    Q_INVOKABLE void revealTabContent();
    double tabX() const;
    double tabWidth() const;
    Q_INVOKABLE void setTabIndicator(double x, double width, bool animate);

    void setReducedMotion(bool reduced);
Q_SIGNALS:
    void changed();
    void tabIndicatorChanged();
    void prepared();
    void focusRequested(bool detail);

private:
    struct Private;
    std::unique_ptr<Private> d;
};
}
