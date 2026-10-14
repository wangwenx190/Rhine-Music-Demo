#pragma once
#include "ArchivePool.h"
#include "ScalarSpring.h"
#include <optional>

namespace yalahoo {
struct ReturningCase
{
    ArchiveCell cell;
    QString albumId;
    ScalarSpring lift;
    double rotation = 0;
    std::optional<double> returnY;
    quint64 objectId = 0;
    double clarity = 0;
};
// Selected + three returning owners share the four detailed cover leases.
// No eviction: admission waits, or atomically reclaims the same physical cell.
class ReturningCasePool
{
public:
    static constexpr qsizetype capacity = 3;
    ReturningCasePool()
    {
        m_cases.reserve(capacity);
    }
    bool canExchange(ArchiveCell incoming, bool outgoing) const;
    std::optional<ReturningCase> take(ArchiveCell cell);
    void append(ReturningCase copy);
    void clear()
    {
        m_cases.clear();
    }
    auto begin()
    {
        return m_cases.begin();
    }
    auto end()
    {
        return m_cases.end();
    }
    auto begin() const
    {
        return m_cases.cbegin();
    }
    auto end() const
    {
        return m_cases.cend();
    }
    auto cbegin() const
    {
        return m_cases.cbegin();
    }
    auto cend() const
    {
        return m_cases.cend();
    }
    qsizetype size() const
    {
        return m_cases.size();
    }
    template <typename Predicate>
    void removeIf(Predicate predicate)
    {
        m_cases.removeIf(predicate);
    }

private:
    QVector<ReturningCase> m_cases;
};
}
