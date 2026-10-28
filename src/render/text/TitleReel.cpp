#include "TitleReel.h"
#include <QScopedValueRollback>
#include "FrameConnection.h"
#include "Units.h"
#include "FrameClock.h"
#include <QSet>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <algorithm>
#include <cmath>
#include <utility>

namespace {
// Reel curves preserve the source sampled trajectory, velocity clamp and blur response.
constexpr auto GlyphPaddingEm = .3;
constexpr auto SlotPaddingPixels = .3;
constexpr auto EmptyFadeDurationRatio = .65;
constexpr auto EntryDelayFraction = .14;
constexpr auto EntryActiveFraction = .86;
constexpr auto EntryBlurVelocityRange = 5;
constexpr auto RollbackVelocityScale = 10000;
constexpr auto RollbackReferenceMilliseconds = 460;
constexpr auto RollBlurSpeed = 24;
constexpr auto BlurSpeedDivisor = 6;
constexpr auto MaximumIdleSlots = 64;
}

using namespace Qt::StringLiterals;

namespace yalahoo {
ReelTrack::Sample ReelTrack::sample() const
{
    if (!moving())
        return { target, 0 };
    if (sampled)
        return sampledReel::interpolate(
                points, std::max(0.0, elapsed / duration) * sampledReel::Intervals, duration);
    return sampledReel::analytic(start, target, initialVelocity, duration,
                                 std::max(0.0, elapsed / duration));
}
double ReelTrack::display() const
{
    if (!moving())
        return target;
    return sampledReel::interpolate(
                   points, std::max(0.0, elapsed / duration) * sampledReel::Intervals, duration)
            .value;
}
void ReelTrack::move(double to, bool instant)
{
    if (to == target && !instant)
        return;
    move(to, instant, sample());
}
void ReelTrack::move(double to, bool instant, Sample from)
{
    start = from.value;
    initialVelocity = sampledReel::limitedVelocity(start - to, from.velocity, duration);
    target = to;
    elapsed = instant
                    || (std::abs(to - start) < sampledReel::AlignmentTolerance
                        && std::abs(initialVelocity) < sampledReel::AlignmentTolerance)
            ? duration
            : 0;
    if (!moving()) {
        points.fill(target);
        return;
    }
    sampledReel::fill(points, start, target, initialVelocity, duration);
}
void ReelTrack::advance(double dt)
{
    elapsed = std::min(duration, elapsed + std::max(0.0, dt));
    if (elapsed >= std::nextafter(duration, 0.0))
        elapsed = duration;
}
bool ReelTrack::moving() const
{
    return elapsed < duration;
}
double ReelSlot::positionX() const
{
    return x.display();
}
double ReelSlot::positionY() const
{
    return y.display();
}
double ReelSlot::viewportHeight() const
{
    return viewport.display();
}
double ReelSlot::padding() const
{
    return glyph.size * sampledReel::GlyphSidePaddingEm;
}
bool ReelSlot::shown() const
{
    return active || (transport ? alpha.moving() : roll.moving());
}
bool ReelSlot::translation() const
{
    return glyph.translation;
}
double ReelSlot::pixelSize() const
{
    return glyph.size;
}
double ReelSlot::rollPosition() const
{
    return roll.display() - (transport ? entry.display() : 0);
}
QVariantList ReelSlot::faceValues() const
{
    return m_values;
}
double ReelSlot::opacity() const
{
    return transport ? std::clamp(alpha.display(), 0.0, 1.0) : 1;
}
double ReelSlot::blurOpacity() const
{
    const auto &track = blurOnEntry ? entry : roll;
    if (!transport || !track.moving())
        return 0;
    return sampledReel::interpolate(blurPoints,
                                    track.elapsed / track.duration * sampledReel::Intervals,
                                    track.duration)
            .value;
}
bool ReelSlot::nativeInk() const
{
    for (const auto &face : faces)
        if (!face.glyph.text.isEmpty() && !face.glyph.colorGlyph)
            return true;
    return false;
}
int ReelSlot::faceCapacity() const
{
    return m_faceCapacity;
}
void ReelSlot::writeFaces(const QList<TitleGlyph> &values)
{
    // Finishing or reconciling an already-settled slot must not rebuild identical QML
    // descriptors and invalidate native glyph nodes. Compare the normalized full descriptor.
    const bool unchanged = m_contentPixelSize == glyph.size
            && m_contentTranslation == glyph.translation
            && std::equal(faces.cbegin(), faces.cend(), values.cbegin(), values.cend(),
                          [this](const Face &face, TitleGlyph value) {
                              if (value.height == 0)
                                  value.height = glyph.height;
                              return face.glyph == value
                                      && capacityWidth >= value.width + glyph.size * GlyphPaddingEm
                                              + SlotPaddingPixels
                                      && capacityHeight >= value.height;
                          });
    if (unchanged)
        return;
    m_contentPixelSize = glyph.size;
    m_contentTranslation = glyph.translation;
    faces.clear();
    m_values.clear();
    faces.reserve(values.size());
    m_values.reserve(values.size());
    m_faceCapacity = std::max(m_faceCapacity, int(values.size()));
    double offset = 0;
    for (auto face : values) {
        if (face.height == 0)
            face.height = glyph.height;
        capacityWidth = std::max(capacityWidth,
                                 face.width + glyph.size * GlyphPaddingEm + SlotPaddingPixels);
        capacityHeight = std::max(capacityHeight, face.height);
        faces.append({ face, offset });
        const QFontMetricsF metrics(face.font);
        const double ascent = metrics.ascent() * face.scale;
        const double descent = metrics.descent() * face.scale;
        const double baseline =
                (face.height - std::round(ascent) - std::round(descent)) / 2 + std::round(ascent);
        m_values.append(QVariantMap{ { u"text"_s, face.text },
                                     { u"font"_s, face.font },
                                     { u"scale"_s, face.scale },
                                     { u"offset"_s, offset },
                                     { u"baseline"_s, baseline },
                                     { u"height"_s, face.height },
                                     { u"colored"_s, face.colorGlyph } });
        offset += face.height;
    }
    ++revision;
    Q_EMIT contentChanged();
}
void ReelSlot::rollTo(const TitleGlyph &next, bool instant)
{
    const bool entering = transport && !active && !shown();
    const double previousBlur = blurOpacity();
    const double alphaTarget = next.text.isEmpty() ? 0 : 1;
    if (transport && alphaTarget != alpha.target) {
        const auto from = alpha.sample();
        alpha.duration = next.text.isEmpty()
                ? sampledReel::TransitionSeconds * EmptyFadeDurationRatio
                : sampledReel::TransitionSeconds;
        alpha.sampled = transport;
        alpha.move(alphaTarget, instant, from);
    } else if (transport && instant) {
        alpha.move(alphaTarget, true);
    }
    if (transport && next.text.isEmpty()) {
        if (instant) {
            roll.move(0, true);
            writeFaces({ next });
        }
        return;
    }
    if (instant) {
        writeFaces({ next });
        roll.move(0, true);
        entry.move(0, true);
        return;
    }
    if (entering) {
        writeFaces({ next });
        roll.move(0, true);
        entry.sampled = true;
        entry.move(0, false, { glyph.height, 0 });
        for (int i = 0; i < sampledReel::SampleCount; ++i) {
            const double t = std::clamp((double(i) / sampledReel::Intervals - EntryDelayFraction)
                                                / EntryActiveFraction,
                                        0.0, 1.0);
            entry.points[i] = i == sampledReel::Intervals ? 0
                                                          : glyph.height
                            * (1 + sampledReel::Decay * t) * std::exp(-sampledReel::Decay * t);
        }
        blurOnEntry = true;
        blurPoints[0] = 0;
        blurPoints[sampledReel::Intervals] = 0;
        for (int i = 1; i < sampledReel::Intervals; ++i) {
            const double speed =
                    std::abs((entry.points[i + 1] - entry.points[i - 1]) * sampledReel::Intervals
                             / (2 * entry.duration * glyph.height));
            blurPoints[i] = std::clamp((speed - 1) / EntryBlurVelocityRange, 0.0, 1.0);
        }
        return;
    }
    const auto sample = roll.sample();
    const double position = std::max(0.0, sample.value);
    int first = 0;
    while (first + 1 < faces.size() && faces[first + 1].offset <= position)
        ++first;
    const double offset = faces.isEmpty() ? 0 : std::max(0.0, position - faces[first].offset);
    QList<TitleGlyph> values{ faces.isEmpty() ? TitleGlyph() : faces[first].glyph };
    for (int i = first + 1; i < faces.size(); ++i) {
        if (faces[i].offset >= position + viewport.sample().value - sampledReel::AlignmentTolerance)
            break;
        values.append(faces[i].glyph);
    }
    const auto &last = values.last();
    if (!sameGlyphInk(last, next))
        values.append(next);
    writeFaces(values);
    const double target = faces.last().offset;
    const double from = std::min(offset, target);
    const double velocity = std::max(
            0.0,
            std::min(sample.velocity,
                     (target - from) * RollbackVelocityScale / RollbackReferenceMilliseconds));
    roll.move(target, false, { from, velocity });
    if (transport) {
        blurOnEntry = false;
        const double speed = RollBlurSpeed;
        blurPoints[0] = previousBlur;
        blurPoints[sampledReel::Intervals] = 0;
        for (int i = 1; i < sampledReel::Intervals; ++i) {
            const double blurVelocity =
                    std::abs((roll.points[i + 1] - roll.points[i - 1]) * sampledReel::Intervals
                             / (2 * roll.duration * std::max(1.0, glyph.height)));
            blurPoints[i] = std::clamp((blurVelocity - speed / BlurSpeedDivisor)
                                               / (speed - speed / BlurSpeedDivisor),
                                       0.0, 1.0);
        }
    }
    if (!roll.moving()) {
        writeFaces({ next });
        roll.move(0, true);
    }
}
void ReelSlot::advance(double dt)
{
    const bool rolling = roll.moving();
    const bool moving = rolling || x.moving() || y.moving() || viewport.moving()
            || (transport && (alpha.moving() || entry.moving()));
    if (!moving)
        return;
    roll.advance(dt);
    x.advance(dt);
    y.advance(dt);
    viewport.advance(dt);
    if (transport) {
        alpha.advance(dt);
        entry.advance(dt);
    }
    if (rolling && !roll.moving()) {
        writeFaces({ active || transport ? glyph : TitleGlyph() });
        roll.move(0, true);
    }
    Q_EMIT frameChanged();
}
bool ReelSlot::moving() const
{
    return roll.moving() || x.moving() || y.moving() || viewport.moving()
            || (transport && (alpha.moving() || entry.moving()));
}
struct TitleReel::Private final
{
    Q_DISABLE_COPY_MOVE(Private)
    explicit Private(TitleReel *owner) : q(owner)
    {
    }
    TitleReel *const q;

    void schedule(bool instant);
    void prune();
    void finishMotion();
    void publishResources();
    TextLayoutAdapter m_layout;
    TitleLayout m_target;
    QList<ReelSlot *> m_slots;
    QList<ReelSlot *> m_animatedSlots;
    QHash<QString, ReelSlot *> m_index;
    QPointer<FrameClock> m_clock;
    QMetaObject::Connection m_connection;
    QString m_text, m_displayed;
    QFont m_font, m_subtitle;
    double m_size = reelMotion::DefaultFontPixels, m_width = reelMotion::DefaultLayoutBudgetPixels,
           m_budget = reelMotion::DefaultLayoutBudgetPixels, m_dpr = 1;
    bool m_narrow = false, m_transport = false, m_reduced = false, m_active = true;
    bool m_complete = false, m_scheduled = false, m_instant = true;
    bool m_finishPending = false;
    bool m_reconciling = false, m_deferredFinish = false;
    bool m_dirty = true;
    ReelTrack m_height;
    QList<QSize> m_resourceSizes;
};
TitleReel::TitleReel(QObject *parent)
    : QAbstractListModel(parent), d(std::make_unique<Private>(this))
{
    if (qGuiApp)
        connect(qGuiApp, &QGuiApplication::fontDatabaseChanged, this, [this] {
            d->m_layout.clear();
            d->schedule(true);
        });
}
int TitleReel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : d->m_slots.size();
}
QVariant TitleReel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.model() != this || index.column() != 0 || role != Qt::UserRole
        || index.row() < 0 || index.row() >= d->m_slots.size())
        return { };
    return QVariant::fromValue(d->m_slots[index.row()]);
}
QHash<int, QByteArray> TitleReel::roleNames() const
{
    static const QHash<int, QByteArray> roles = { { Qt::UserRole, "slotObject"_ba } };
    return roles;
}
void TitleReel::classBegin()
{
}
void TitleReel::componentComplete()
{
    d->m_complete = true;
    reconcile();
}
FrameClock *TitleReel::clock() const
{
    return d->m_clock;
}
void TitleReel::setClock(FrameClock *clock)
{
    if (bindFrameClock(this, d->m_clock, d->m_connection, clock))
        Q_EMIT clockChanged();
}
QString TitleReel::text() const
{
    return d->m_text;
}
// Retarget from displayed slot state so rapid changes do not restart from a hidden endpoint.
// The layout cache owns shaped glyphs; slots retain only the faces needed for current transitions.
void TitleReel::setText(const QString &text)
{
    if (d->m_text == text)
        return;
    d->m_text = text;
    d->schedule(false);
}
QFont TitleReel::font() const
{
    return d->m_font;
}
void TitleReel::setFont(const QFont &font)
{
    if (d->m_font == font)
        return;
    d->m_font = font;
    d->schedule(true);
}
QFont TitleReel::subtitleFont() const
{
    return d->m_subtitle;
}
void TitleReel::setSubtitleFont(const QFont &font)
{
    if (d->m_subtitle == font)
        return;
    d->m_subtitle = font;
    d->schedule(true);
}
double TitleReel::pixelSize() const
{
    return d->m_size;
}
void TitleReel::setPixelSize(double value)
{
    if (!std::isfinite(value) || value <= 0 || d->m_size == value)
        return;
    d->m_size = value;
    d->schedule(true);
}
double TitleReel::availableWidth() const
{
    return d->m_width;
}
void TitleReel::setAvailableWidth(double value)
{
    if (!std::isfinite(value) || value < 0 || d->m_width == value)
        return;
    d->m_width = value;
    d->schedule(true);
}
double TitleReel::budget() const
{
    return d->m_budget;
}
void TitleReel::setBudget(double value)
{
    if (!std::isfinite(value) || value < 0 || d->m_budget == value)
        return;
    d->m_budget = value;
    d->schedule(false);
}
double TitleReel::dpr() const
{
    return d->m_dpr;
}
void TitleReel::setDpr(double value)
{
    if (!std::isfinite(value) || value <= 0 || d->m_dpr == value)
        return;
    d->m_dpr = value;
    d->schedule(true);
}
bool TitleReel::narrow() const
{
    return d->m_narrow;
}
void TitleReel::setNarrow(bool value)
{
    if (d->m_narrow == value)
        return;
    d->m_narrow = value;
    d->schedule(true);
}
bool TitleReel::transport() const
{
    return d->m_transport;
}
void TitleReel::setTransport(bool value)
{
    if (d->m_transport == value)
        return;
    d->m_transport = value;
    d->schedule(true);
}
bool TitleReel::reducedMotion() const
{
    return d->m_reduced;
}
void TitleReel::setReducedMotion(bool value)
{
    if (d->m_reduced == value)
        return;
    d->m_reduced = value;
    if (value)
        finish();
    Q_EMIT inputChanged();
}
bool TitleReel::active() const
{
    return d->m_active;
}
void TitleReel::setActive(bool value)
{
    if (d->m_active == value)
        return;
    d->m_active = value;
    if (!value && !d->m_finishPending) {
        // Visibility is evaluated by QML bindings. Publishing finished motion here
        // can synchronously reevaluate the same binding through the transport.
        d->m_finishPending = true;
        QMetaObject::invokeMethod(
                this,
                [this] {
                    d->m_finishPending = false;
                    if (!d->m_active)
                        finish();
                },
                Qt::QueuedConnection);
    }
    Q_EMIT inputChanged();
}
int TitleReel::slotCount() const
{
    return d->m_slots.size();
}
double TitleReel::layoutHeight() const
{
    return d->m_height.display();
}
double TitleReel::naturalWidth() const
{
    return d->m_target.naturalWidth;
}
double TitleReel::fittedSize() const
{
    return d->m_target.size;
}
QString TitleReel::fittedText() const
{
    return d->m_target.fitted;
}
bool TitleReel::moving() const
{
    return d->m_height.moving() || !d->m_animatedSlots.isEmpty();
}
ReelSlot *TitleReel::slotAt(int index) const
{
    return index >= 0 && index < d->m_slots.size() ? d->m_slots[index] : nullptr;
}
void TitleReel::Private::schedule(bool instant)
{
    m_instant |= instant;
    m_dirty = true;
    Q_EMIT q->inputChanged();
    if (!m_complete || m_scheduled)
        return;
    m_scheduled = true;
    QMetaObject::invokeMethod(
            q,
            [this] {
                if (m_scheduled)
                    q->reconcile();
            },
            Qt::QueuedConnection);
}
void TitleReel::reconcile()
{
    if (d->m_reconciling)
        return;
    const QScopedValueRollback guard(d->m_reconciling, true);
    d->m_scheduled = false;
    if (!d->m_dirty)
        return;
    if (d->m_width <= 0)
        return;
    d->m_dirty = false;
    const auto text = d->m_text;
    const bool instant = d->m_instant || d->m_reduced || !d->m_active || d->m_displayed.isEmpty()
            || (d->m_displayed == d->m_text && !moving())
            || (d->m_clock && !d->m_clock->applicationVisible());
    d->m_instant = false;
    const double previousNaturalWidth = d->m_target.naturalWidth;
    d->m_target = d->m_layout.measure(text, d->m_font, d->m_subtitle, d->m_size, d->m_width,
                                      d->m_budget, d->m_narrow, d->m_dpr, d->m_transport);
    QSet<QString> keys;
    keys.reserve(d->m_target.glyphs.size());
    bool added = false;
    for (const auto &glyph : std::as_const(d->m_target.glyphs)) {
        keys.insert(glyph.key);
        auto *slot = d->m_index.value(glyph.key);
        if (!slot) {
            slot = new ReelSlot(this);
            slot->key = glyph.key;
            slot->glyph = glyph;
            slot->writeFaces({ TitleGlyph() });
            slot->x.move(glyph.x, true);
            slot->y.move(glyph.y, true);
            slot->viewport.move(glyph.height, true);
            beginInsertRows({ }, d->m_slots.size(), d->m_slots.size());
            d->m_slots.append(slot);
            endInsertRows();
            d->m_index.insert(glyph.key, slot);
            added = true;
        }
        const auto old = slot->glyph;
        slot->transport = d->m_transport;
        slot->roll.sampled = d->m_transport;
        slot->x.sampled = d->m_transport;
        const bool changed = !slot->active || !sameGlyphInk(old, glyph);
        slot->glyph = glyph;
        slot->x.move(glyph.x, instant);
        slot->y.move(glyph.y, instant);
        slot->viewport.move(glyph.height, instant);
        if (changed || instant)
            slot->rollTo(glyph, instant);
        slot->active = true;
        Q_EMIT slot->frameChanged();
    }
    for (auto *slot : d->m_slots) {
        if (!slot->active || keys.contains(slot->key))
            continue;
        slot->active = false;
        slot->rollTo({ }, instant);
        Q_EMIT slot->frameChanged();
    }
    d->m_height.move(d->m_target.height, instant);
    d->m_animatedSlots.clear();
    for (auto *slot : d->m_slots)
        if (slot->moving())
            d->m_animatedSlots.append(slot);
    d->m_displayed = text;
    d->publishResources();
    if (added)
        Q_EMIT slotsChanged();
    if (instant)
        d->finishMotion();
    else if (!moving())
        d->prune();
    if (previousNaturalWidth != d->m_target.naturalWidth)
        Q_EMIT naturalWidthChanged();
    Q_EMIT changed();
}
void TitleReel::Private::prune()
{
    const QScopedValueRollback guard(m_reconciling, true);
    int idle = 0;
    for (auto *slot : m_slots)
        idle += !slot->active;
    bool removed = false;
    for (int i = 0; i < m_slots.size() && idle > MaximumIdleSlots;) {
        if (m_slots[i]->active) {
            ++i;
            continue;
        }
        int end = i;
        const int maximum = idle - MaximumIdleSlots;
        while (end < m_slots.size() && end - i < maximum && !m_slots[end]->active)
            ++end;
        // A long title can retire a whole suffix. Remove each contiguous run once,
        // avoiding a shift of the remaining pointer array for every retired slot.
        q->beginRemoveRows({ }, i, end - 1);
        for (int row = i; row < end; ++row) {
            auto *slot = m_slots[row];
            m_index.remove(slot->key);
            slot->deleteLater();
        }
        m_slots.remove(i, end - i);
        q->endRemoveRows();
        idle -= end - i;
        removed = true;
    }
    if (removed)
        Q_EMIT q->slotsChanged();
}
void TitleReel::finish()
{
    if (d->m_reconciling) {
        if (!std::exchange(d->m_deferredFinish, true))
            QMetaObject::invokeMethod(
                    this,
                    [this] {
                        d->m_deferredFinish = false;
                        finish();
                    },
                    Qt::QueuedConnection);
        return;
    }
    if (d->m_scheduled) {
        reconcile();
        if (!moving())
            return;
    }
    const QScopedValueRollback guard(d->m_reconciling, true);
    d->finishMotion();
}
void TitleReel::Private::finishMotion()
{
    m_height.move(m_height.target, true);
    m_animatedSlots.clear();
    for (auto *slot : m_slots) {
        slot->x.move(slot->x.target, true);
        slot->y.move(slot->y.target, true);
        slot->viewport.move(slot->viewport.target, true);
        slot->writeFaces({ slot->active ? slot->glyph : TitleGlyph() });
        slot->roll.move(0, true);
        slot->entry.move(0, true);
        slot->alpha.move(slot->active ? 1 : 0, true);
        Q_EMIT slot->frameChanged();
    }
    prune();
    publishResources();
    Q_EMIT q->changed();
}
// Advance only live transitions from the common frame interval. Completed hidden slots can release
// faces, while queued visibility completion is guarded against a later reactivation.
void TitleReel::advance(double dt)
{
    if (d->m_reconciling || !d->m_active || !moving() || !std::isfinite(dt))
        return;
    const QScopedValueRollback guard(d->m_reconciling, true);
    d->m_height.advance(dt);
    bool resourcesDirty = false;
    for (auto *slot : d->m_animatedSlots) {
        const bool shown = slot->shown();
        slot->advance(dt);
        resourcesDirty |= shown != slot->shown();
    }
    d->m_animatedSlots.removeIf([](const auto *slot) {
        return !slot->moving();
    });
    if (!moving())
        d->prune();
    if (resourcesDirty)
        d->publishResources();
    Q_EMIT changed();
}
QList<QSize> TitleReel::resourceSizes() const
{
    return d->m_resourceSizes;
}
void TitleReel::Private::publishResources()
{
    QList<QSize> sizes;
    for (const auto *slot : m_slots)
        if (slot->shown())
            sizes.append(QSize(int(std::ceil(slot->capacityWidth * m_dpr)),
                               int(std::ceil(slot->capacityHeight * m_dpr))));
    if (sizes != m_resourceSizes) {
        m_resourceSizes = sizes;
        Q_EMIT q->resourcesChanged();
    }
}
QVariantMap TitleReel::diagnostics() const
{
    int faces = 0, active = 0;
    qint64 layerBytes = 0;
    for (const auto *slot : d->m_slots) {
        active += slot->active;
        faces += slot->faces.size();
        if (slot->shown())
            layerBytes += qint64(std::ceil(slot->capacityWidth * d->m_dpr))
                    * qint64(std::ceil(slot->capacityHeight * d->m_dpr))
                    * yalahoo::units::RgbaBytesPerPixel;
    }
    return { { u"slots"_s, slotCount() },
             { u"activeSlots"_s, active },
             { u"faces"_s, faces },
             { u"layoutMisses"_s, QVariant::fromValue(d->m_layout.misses()) },
             { u"cacheEntries"_s, d->m_layout.cacheEntries() },
             { u"cacheBytes"_s, d->m_layout.cacheBytes() },
             { u"layerRgba8Estimate"_s, layerBytes },
             { u"moving"_s, moving() } };
}
TitleReel::~TitleReel() = default;

}
