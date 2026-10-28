#include "ReturningCasePool.h"
#include <algorithm>

namespace yalahoo {
bool ReturningCasePool::canExchange(ArchiveCell incoming, bool outgoing) const
{
    return !outgoing || m_cases.size() < capacity
            || std::any_of(
                    cbegin(), cend(),
                    [&](const ReturningCase &copy) {
                        return copy.cell == incoming;
                    });
}
std::optional<ReturningCase> ReturningCasePool::take(ArchiveCell cell)
{
    const auto it = std::find_if(begin(), end(), [&](const ReturningCase &copy) {
        return copy.cell == cell;
    });
    if (it == end())
        return std::nullopt;
    auto result = *it;
    m_cases.erase(it);
    return result;
}
void ReturningCasePool::append(ReturningCase copy)
{
    Q_ASSERT(m_cases.size() < capacity);
    m_cases.append(std::move(copy));
}
}
