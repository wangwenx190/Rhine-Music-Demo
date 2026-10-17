#include "TitleReel.h"
#include <QSet>
#include <QFontMetricsF>
#include <QGuiApplication>
#include <algorithm>
#include <cmath>

using namespace Qt::StringLiterals;

namespace yalahoo {
ReelTrack::Sample ReelTrack::sample() const
{
    if (!moving())
        return { target, 0 };
    if (sampled) {
        const double position = std::max(0.0, elapsed / duration) * 48;
        const int index = std::min(47, int(position));
        return { display(), (points[index + 1] - points[index]) * 48 / duration };
    }
    const double t = std::max(0.0, elapsed / duration);
    const double delta = start - target;
    const double advance = initialVelocity * duration + 10 * delta;
    const double decay = std::exp(-10 * t);
    return { target + (delta + advance * t) * decay,
             (advance - 10 * (delta + advance * t)) * decay / duration };
}
double ReelTrack::display() const
{
    if (!moving())
        return target;
    const double position = std::max(0.0, elapsed / duration) * 48;
    const int index = std::min(47, int(position));
    return points[index] + (points[index + 1] - points[index]) * (position - index);
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
    const double limit = std::max(std::abs(start - to), 1.0) * 12 / duration;
    initialVelocity = std::clamp(from.velocity, -limit, limit);
    target = to;
    elapsed = instant || (std::abs(to - start) < .001 && std::abs(initialVelocity) < .001)
            ? duration
            : 0;
    if (!moving()) {
        points.fill(target);
        return;
    }
    for (int i = 0; i < 49; ++i) {
        const double t = i / 48.0;
        points[i] = i == 48 ? target
                            : target
                        + (start - target
                           + (initialVelocity * duration + 10 * (start - target)) * t)
                                * std::exp(-10 * t);
    }
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
    return glyph.size * .15;
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
    const double position = track.elapsed / track.duration * 48;
    const int index = std::min(47, int(position));
    return blurPoints[index] + (blurPoints[index + 1] - blurPoints[index]) * (position - index);
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
    faces.clear();
    m_values.clear();
    m_faceCapacity = std::max(m_faceCapacity, int(values.size()));
    double offset = 0;
    for (auto face : values) {
        if (face.height == 0)
            face.height = glyph.height;
        capacityWidth = std::max(capacityWidth, face.width + glyph.size * .3 + .3);
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
        alpha.duration = next.text.isEmpty() ? .460 * .65 : .460;
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
        for (int i = 0; i < 49; ++i) {
            const double t = std::clamp((i / 48.0 - .14) / .86, 0.0, 1.0);
            entry.points[i] = i == 48 ? 0 : glyph.height * (1 + 10 * t) * std::exp(-10 * t);
        }
        blurOnEntry = true;
        blurPoints[0] = 0;
        blurPoints[48] = 0;
        for (int i = 1; i < 48; ++i) {
            const double speed = std::abs((entry.points[i + 1] - entry.points[i - 1]) * 48
                                          / (2 * entry.duration * glyph.height));
            blurPoints[i] = std::clamp((speed - 1) / 5, 0.0, 1.0);
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
        if (faces[i].offset >= position + viewport.sample().value - .001)
            break;
        values.append(faces[i].glyph);
    }
    const auto &last = values.last();
    if (!sameGlyphInk(last, next))
        values.append(next);
    writeFaces(values);
    const double target = faces.last().offset;
    const double from = std::min(offset, target);
    const double velocity = std::max(0.0, std::min(sample.velocity, (target - from) * 10000 / 460));
    roll.move(target, false, { from, velocity });
    if (transport) {
        blurOnEntry = false;
        const double speed = 24;
        blurPoints[0] = previousBlur;
        blurPoints[48] = 0;
        for (int i = 1; i < 48; ++i) {
            const double blurVelocity =
                    std::abs((roll.points[i + 1] - roll.points[i - 1]) * 48
                             / (2 * roll.duration * std::max(1.0, glyph.height)));
            blurPoints[i] = std::clamp((blurVelocity - speed / 6) / (speed - speed / 6), 0.0, 1.0);
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
TitleReel::TitleReel(QObject *parent) : QAbstractListModel(parent)
{
    if (qGuiApp)
        connect(qGuiApp, &QGuiApplication::fontDatabaseChanged, this, [this] {
            m_layout.clear();
            schedule(true);
        });
}
int TitleReel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_slots.size();
}
QVariant TitleReel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.model() != this || index.column() != 0 || role != Qt::UserRole
        || index.row() < 0 || index.row() >= m_slots.size())
        return { };
    return QVariant::fromValue(m_slots[index.row()]);
}
QHash<int, QByteArray> TitleReel::roleNames() const
{
    return { { Qt::UserRole, "slotObject"_ba } };
}
void TitleReel::classBegin()
{
}
void TitleReel::componentComplete()
{
    m_complete = true;
    reconcile();
}
FrameClock *TitleReel::clock() const
{
    return m_clock;
}
void TitleReel::setClock(FrameClock *clock)
{
    if (m_clock == clock)
        return;
    disconnect(m_connection);
    m_clock = clock;
    if (clock)
        m_connection = connect(clock, &FrameClock::frame, this, [this](double, double dt) {
            advance(dt);
        });
    Q_EMIT clockChanged();
}
QString TitleReel::text() const
{
    return m_text;
}
void TitleReel::setText(const QString &text)
{
    if (m_text == text)
        return;
    m_text = text;
    schedule(false);
}
QFont TitleReel::font() const
{
    return m_font;
}
void TitleReel::setFont(const QFont &font)
{
    if (m_font == font)
        return;
    m_font = font;
    schedule(true);
}
QFont TitleReel::subtitleFont() const
{
    return m_subtitle;
}
void TitleReel::setSubtitleFont(const QFont &font)
{
    if (m_subtitle == font)
        return;
    m_subtitle = font;
    schedule(true);
}
double TitleReel::pixelSize() const
{
    return m_size;
}
void TitleReel::setPixelSize(double value)
{
    if (!std::isfinite(value) || value <= 0 || m_size == value)
        return;
    m_size = value;
    schedule(true);
}
double TitleReel::availableWidth() const
{
    return m_width;
}
void TitleReel::setAvailableWidth(double value)
{
    if (!std::isfinite(value) || value < 0 || m_width == value)
        return;
    m_width = value;
    schedule(true);
}
double TitleReel::budget() const
{
    return m_budget;
}
void TitleReel::setBudget(double value)
{
    if (!std::isfinite(value) || value < 0 || m_budget == value)
        return;
    m_budget = value;
    schedule(false);
}
double TitleReel::dpr() const
{
    return m_dpr;
}
void TitleReel::setDpr(double value)
{
    if (!std::isfinite(value) || value <= 0 || m_dpr == value)
        return;
    m_dpr = value;
    schedule(true);
}
bool TitleReel::narrow() const
{
    return m_narrow;
}
void TitleReel::setNarrow(bool value)
{
    if (m_narrow == value)
        return;
    m_narrow = value;
    schedule(true);
}
bool TitleReel::transport() const
{
    return m_transport;
}
void TitleReel::setTransport(bool value)
{
    if (m_transport == value)
        return;
    m_transport = value;
    schedule(true);
}
bool TitleReel::reducedMotion() const
{
    return m_reduced;
}
void TitleReel::setReducedMotion(bool value)
{
    if (m_reduced == value)
        return;
    m_reduced = value;
    if (value)
        finish();
    Q_EMIT inputChanged();
}
bool TitleReel::active() const
{
    return m_active;
}
void TitleReel::setActive(bool value)
{
    if (m_active == value)
        return;
    m_active = value;
    if (!value && !m_finishPending) {
        // Visibility is evaluated by QML bindings. Publishing finished motion here
        // can synchronously reevaluate the same binding through the transport.
        m_finishPending = true;
        QMetaObject::invokeMethod(
                this,
                [this] {
                    m_finishPending = false;
                    if (!m_active)
                        finish();
                },
                Qt::QueuedConnection);
    }
    Q_EMIT inputChanged();
}
int TitleReel::slotCount() const
{
    return m_slots.size();
}
double TitleReel::layoutHeight() const
{
    return m_height.display();
}
double TitleReel::naturalWidth() const
{
    return m_target.naturalWidth;
}
double TitleReel::fittedSize() const
{
    return m_target.size;
}
QString TitleReel::fittedText() const
{
    return m_target.fitted;
}
bool TitleReel::moving() const
{
    return m_height.moving() || !m_animatedSlots.isEmpty();
}
ReelSlot *TitleReel::slotAt(int index) const
{
    return index >= 0 && index < m_slots.size() ? m_slots[index] : nullptr;
}
void TitleReel::schedule(bool instant)
{
    m_instant |= instant;
    m_dirty = true;
    Q_EMIT inputChanged();
    if (!m_complete || m_scheduled)
        return;
    m_scheduled = true;
    QMetaObject::invokeMethod(
            this,
            [this] {
                if (m_scheduled)
                    reconcile();
            },
            Qt::QueuedConnection);
}
void TitleReel::reconcile()
{
    m_scheduled = false;
    if (!m_dirty)
        return;
    if (m_width <= 0)
        return;
    m_dirty = false;
    const bool instant = m_instant || m_reduced || !m_active || m_displayed.isEmpty()
            || (m_displayed == m_text && !moving()) || (m_clock && !m_clock->applicationVisible());
    m_instant = false;
    const double previousNaturalWidth = m_target.naturalWidth;
    m_target = m_layout.measure(m_text, m_font, m_subtitle, m_size, m_width, m_budget, m_narrow,
                                m_dpr, m_transport);
    QSet<QString> keys;
    bool added = false;
    for (const auto &glyph : m_target.glyphs) {
        keys.insert(glyph.key);
        auto *slot = m_index.value(glyph.key);
        if (!slot) {
            slot = new ReelSlot(this);
            slot->key = glyph.key;
            slot->glyph = glyph;
            slot->writeFaces({ TitleGlyph() });
            slot->x.move(glyph.x, true);
            slot->y.move(glyph.y, true);
            slot->viewport.move(glyph.height, true);
            beginInsertRows({ }, m_slots.size(), m_slots.size());
            m_slots.append(slot);
            endInsertRows();
            m_index.insert(glyph.key, slot);
            added = true;
        }
        const auto old = slot->glyph;
        slot->transport = m_transport;
        slot->roll.sampled = m_transport;
        slot->x.sampled = m_transport;
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
    for (auto *slot : m_slots) {
        if (!slot->active || keys.contains(slot->key))
            continue;
        slot->active = false;
        slot->rollTo({ }, instant);
        Q_EMIT slot->frameChanged();
    }
    m_height.move(m_target.height, instant);
    m_animatedSlots.clear();
    for (auto *slot : m_slots)
        if (slot->moving())
            m_animatedSlots.append(slot);
    m_displayed = m_text;
    publishResources();
    if (added)
        Q_EMIT slotsChanged();
    if (instant)
        finish();
    else if (!moving())
        prune();
    if (previousNaturalWidth != m_target.naturalWidth)
        Q_EMIT naturalWidthChanged();
    Q_EMIT changed();
}
void TitleReel::prune()
{
    int idle = 0;
    for (auto *slot : m_slots)
        idle += !slot->active;
    bool removed = false;
    for (int i = 0; i < m_slots.size() && idle > 64;) {
        auto *slot = m_slots[i];
        if (slot->active) {
            ++i;
            continue;
        }
        m_index.remove(slot->key);
        beginRemoveRows({ }, i, i);
        m_slots.removeAt(i);
        endRemoveRows();
        slot->deleteLater();
        --idle;
        removed = true;
    }
    if (removed)
        Q_EMIT slotsChanged();
}
void TitleReel::finish()
{
    if (m_scheduled) {
        reconcile();
        // An instant reconciliation already completed and published every slot.
        if (!moving())
            return;
    }
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
    Q_EMIT changed();
}
void TitleReel::advance(double dt)
{
    if (!m_active || !moving() || !std::isfinite(dt))
        return;
    m_height.advance(dt);
    bool resourcesDirty = false;
    for (auto *slot : m_animatedSlots) {
        const bool shown = slot->shown();
        slot->advance(dt);
        resourcesDirty |= shown != slot->shown();
    }
    m_animatedSlots.removeIf([](const auto *slot) {
        return !slot->moving();
    });
    if (!moving())
        prune();
    if (resourcesDirty)
        publishResources();
    Q_EMIT changed();
}
QList<QSize> TitleReel::resourceSizes() const
{
    return m_resourceSizes;
}
void TitleReel::publishResources()
{
    QList<QSize> sizes;
    for (const auto *slot : m_slots)
        if (slot->shown())
            sizes.append(QSize(int(std::ceil(slot->capacityWidth * m_dpr)),
                               int(std::ceil(slot->capacityHeight * m_dpr))));
    if (sizes != m_resourceSizes) {
        m_resourceSizes = sizes;
        Q_EMIT resourcesChanged();
    }
}
QVariantMap TitleReel::diagnostics() const
{
    int faces = 0, active = 0;
    qint64 layerBytes = 0;
    for (const auto *slot : m_slots) {
        active += slot->active;
        faces += slot->faces.size();
        if (slot->shown())
            layerBytes += qint64(std::ceil(slot->capacityWidth * m_dpr))
                    * qint64(std::ceil(slot->capacityHeight * m_dpr)) * 4;
    }
    return { { u"slots"_s, slotCount() },
             { u"activeSlots"_s, active },
             { u"faces"_s, faces },
             { u"layoutMisses"_s, QVariant::fromValue(m_layout.misses()) },
             { u"cacheEntries"_s, m_layout.cacheEntries() },
             { u"cacheBytes"_s, m_layout.cacheBytes() },
             { u"layerRgba8Estimate"_s, layerBytes },
             { u"moving"_s, moving() } };
}
}
