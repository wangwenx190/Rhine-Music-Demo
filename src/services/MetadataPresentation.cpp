#include "MetadataPresentation.h"
#include <QCoreApplication>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonValue>
#include <QStringList>

using namespace Qt::StringLiterals;

namespace yalahoo {
namespace {
QString jsonText(const QVariant &value)
{
    const auto json = QJsonDocument(QJsonArray{ QJsonValue::fromVariant(value) })
                              .toJson(QJsonDocument::Compact);
    return QString::fromUtf8(json.mid(1, json.size() - 2));
}
QString contentText(const QVariantMap &result)
{
    const auto kind = result.value(u"kind"_s).toString();
    const auto payload = result.value(u"payload"_s).toMap();
    if (kind == u"introduction"_s) {
        const auto description = payload.value(u"description"_s).toString();
        return description.isEmpty()
                ? QCoreApplication::translate("Yalahoo", "No introduction available")
                : description;
    }
    QStringList lines;
    if (kind == u"credits"_s) {
        for (const auto &value : payload.value(u"producers"_s).toList()) {
            const auto producer = value.toMap();
            auto line = producer.value(u"name"_s).toString();
            const auto role = producer.value(u"role"_s).toString();
            if (!role.isEmpty())
                line += u" \u00b7 "_s + role;
            const auto source = producer.value(u"source"_s);
            if (source.isValid() && !source.isNull() && source != QVariant(QString()))
                line += u" \u00b7 "_s + jsonText(source);
            lines.append(line);
        }
        return lines.isEmpty() ? QCoreApplication::translate("Yalahoo", "No credits available")
                               : lines.join('\n'_L1);
    }
    const auto online = payload.value(u"online"_s).toMap();
    auto title = online.value(u"title"_s).toString();
    if (title.isEmpty())
        title = online.value(u"albumTitle"_s).toString();
    lines = { title, online.value(u"releaseId"_s).toString(),
              online.value(u"releaseGroupId"_s).toString() };
    for (const auto &value : payload.value(u"genres"_s).toList())
        lines.append(value.toString());
    lines.removeAll(QString());
    return lines.isEmpty()
            ? QCoreApplication::translate("Yalahoo", "No MusicBrainz metadata available")
            : lines.join('\n'_L1);
}
}
QString metadataStatusText(const QString &status)
{
    if (status == "Deferred"_L1)
        return QCoreApplication::translate("MetadataStatus", "Deferred");
    if (status == "Matched"_L1)
        return QCoreApplication::translate("MetadataStatus", "Matched");
    if (status == "Partial"_L1)
        return QCoreApplication::translate("MetadataStatus", "Partial");
    if (status == "NotFound"_L1)
        return QCoreApplication::translate("MetadataStatus", "Not found");
    if (status == "Uncertain"_L1)
        return QCoreApplication::translate("MetadataStatus", "Uncertain");
    if (status == "Error"_L1)
        return QCoreApplication::translate("MetadataStatus", "Error");
    if (status == "Cancelled"_L1)
        return QCoreApplication::translate("MetadataStatus", "Cancelled");
    if (status == "Pending"_L1)
        return QCoreApplication::translate("MetadataStatus", "Pending");
    return status;
}
QVariantMap metadataPresentation(QVariantMap result, const QString &albumTitle)
{
    auto state =
            QCoreApplication::translate("Yalahoo", "Current album: %1\nQuery status: %2")
                    .arg(albumTitle,
                         metadataStatusText(result.value(u"status"_s, u"Deferred"_s).toString()));
    const auto checkedAt = result.value(u"checkedAt"_s).toString();
    const auto retryAt = result.value(u"retryAt"_s).toString();
    const auto error = result.value(u"error"_s).toString();
    if (!checkedAt.isEmpty())
        state += '\n'_L1 + QCoreApplication::translate("Yalahoo", "Checked at: %1").arg(checkedAt);
    if (!retryAt.isEmpty())
        state += '\n'_L1 + QCoreApplication::translate("Yalahoo", "Retry at: %1").arg(retryAt);
    if (!error.isEmpty())
        state += '\n'_L1 + error;
    QStringList sources;
    const auto source = result.value(u"source"_s).toMap();
    for (auto it = source.cbegin(); it != source.cend(); ++it)
        sources.append(it.value().canConvert<QString>() ? it.value().toString()
                                                        : jsonText(it.value()));
    QVariantList candidates;
    const auto payload = result.value(u"payload"_s).toMap();
    const auto field =
            result.value(u"kind"_s).toString() == "credits"_L1 ? u"creditsLookup"_s : u"online"_s;
    for (const auto &value : payload.value(field).toMap().value(u"candidates"_s).toList()) {
        auto candidate = value.toMap();
        QStringList parts{ candidate.value(u"title"_s).toString(),
                           candidate.value(u"date"_s).toString(),
                           candidate.value(u"country"_s).toString(),
                           candidate.value(u"label"_s).toString(),
                           candidate.value(u"id"_s).toString() };
        parts.removeAll(QString());
        candidate.insert(u"display"_s, parts.join(u" / "_s));
        candidates.append(candidate);
    }
    result.insert(u"releaseCandidates"_s, candidates);
    result.insert(u"stateText"_s, state);
    result.insert(u"contentText"_s, contentText(result));
    result.insert(
            u"sourceText"_s,
            QCoreApplication::translate("Yalahoo", "Source: %1").arg(sources.join(u" \u00b7 "_s)));
    return result;
}
}
