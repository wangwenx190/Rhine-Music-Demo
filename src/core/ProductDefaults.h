// Owns user preference defaults, option names and shared speed bounds for persistence and playback.
// Product initialization and independent controllers consume the same policy values.

#pragma once
#include <QStringList>

namespace yalahoo::productDefaults {
inline constexpr auto MinimumMotionSpeed = .25;
inline constexpr auto MaximumMotionSpeed = 3.0;
inline constexpr auto DefaultSongVolume = .65;
inline constexpr auto DefaultBackgroundVolume = .18;
inline constexpr auto DefaultFeedbackVolume = .22;
inline const QStringList &lightingModes()
{
    using namespace Qt::StringLiterals;
    static const QStringList modes{ u"baseline"_s, u"area"_s, u"guided"_s, u"hybrid"_s };
    return modes;
}
}
