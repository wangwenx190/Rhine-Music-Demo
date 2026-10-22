#include "IMetadataProvider.h"
#include <algorithm>
#include <array>

using namespace Qt::StringLiterals;

namespace yalahoo {
namespace {
struct KindDefinition final
{
    MetadataKind kind;
    QLatin1StringView name, stateKey;
    QJsonObject Album::*state;
    bool ProviderCapabilities::*capability;
};
constexpr std::array Kinds{ KindDefinition{ MetadataKind::Introduction, "introduction"_L1,
                                            "introduction"_L1, &Album::introduction,
                                            &ProviderCapabilities::introductions },
                            KindDefinition{ MetadataKind::Credits, "credits"_L1, "creditsLookup"_L1,
                                            &Album::creditsLookup, &ProviderCapabilities::credits },
                            KindDefinition{ MetadataKind::Enrichment, "enrichment"_L1, "online"_L1,
                                            &Album::online, &ProviderCapabilities::enrichment } };
const KindDefinition *definition(MetadataKind kind)
{
    const auto found = std::ranges::find(Kinds, kind, &KindDefinition::kind);
    return found == Kinds.end() ? nullptr : &*found;
}
}
std::optional<MetadataKind> metadataKind(QStringView name)
{
    const auto found = std::ranges::find_if(Kinds, [name](const KindDefinition &entry) {
        return entry.name == name;
    });
    return found == Kinds.end() ? std::nullopt : std::optional(found->kind);
}
QString kindName(MetadataKind kind)
{
    const auto *entry = definition(kind);
    return entry ? QString(entry->name) : QString();
}
QString metadataStateKey(MetadataKind kind)
{
    const auto *entry = definition(kind);
    return entry ? QString(entry->stateKey) : QString();
}
QJsonObject metadataState(const Album &album, MetadataKind kind)
{
    const auto *entry = definition(kind);
    return entry ? album.*entry->state : QJsonObject();
}
bool supportsMetadata(const ProviderCapabilities &capabilities, MetadataKind kind)
{
    const auto *entry = definition(kind);
    return entry && capabilities.*entry->capability;
}
QString statusName(ProviderStatus status)
{
    switch (status) {
    case ProviderStatus::Matched:
        return u"Matched"_s;
    case ProviderStatus::Partial:
        return u"Partial"_s;
    case ProviderStatus::NotFound:
        return u"NotFound"_s;
    case ProviderStatus::Uncertain:
        return u"Uncertain"_s;
    case ProviderStatus::Error:
        return u"Error"_s;
    case ProviderStatus::Cancelled:
        return u"Cancelled"_s;
    default:
        return u"Deferred"_s;
    }
}

}
