// Tracks source-derived clarity independently for each case lifecycle. Selected and returning
// cases retain their own reveal state instead of sharing a global detail value.

#pragma once
#include <optional>

namespace yalahoo {
// Frozen DecryptionController's material state only. The reference film overlay
// is independent; music consumes its REVEAL curve and refrosting value.
class CaseClarity final
{
public:
    void enter(bool alreadyClear = false);
    void leave();
    void select(double clarity = 0);
    void advance(double dt, bool ready, bool reduced);
    double value() const
    {
        return m_value;
    }
    static double reveal(double time);

private:
    double m_value = 0;
    bool m_active = false;
    std::optional<double> m_elapsed;
};
}
