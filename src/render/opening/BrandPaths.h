// Defines the bundled brand outline as literal path data. Opening rendering reuses these paths
// without loading mutable global graphics objects.

#pragma once
#include <QPainterPath>
#include <QRegularExpression>
#include <stdexcept>
#include <cmath>
#include <algorithm>
#include <iterator>

namespace yalahoo {
namespace brandPaths {
struct Reader final
{
    const QStringList &tokens;
    qsizetype at = 0;
    QPainterPath path;
    double number()
    {
        bool ok = false;
        const double value = at < tokens.size() ? tokens[at++].toDouble(&ok) : 0;
        if (!ok || !std::isfinite(value))
            throw std::runtime_error("Invalid reveal coordinate");
        return value;
    }
    QPointF point()
    {
        const double x = number(), y = number();
        return { x, y };
    }
};
}
// Grammar of the project's authored reveal centerlines; all coordinates are absolute.
inline QPainterPath parseBrandCenterline(const QString &d)
{
    using namespace Qt::StringLiterals;
    static const QRegularExpression token(u"[MLQCVZ]|[-+]?(?:[0-9]*\\.)?[0-9]+"_s);
    QStringList tokens;
    auto matches = token.globalMatch(d);
    while (matches.hasNext())
        tokens.append(matches.next().captured());
    QString residue = d;
    residue.remove(token);
    static const QRegularExpression separators(u"[ ,\\t\\r\\n]"_s);
    residue.remove(separators);
    if (!residue.isEmpty())
        throw std::runtime_error("Unsupported reveal path grammar");
    using brandPaths::Reader;
    struct Command final
    {
        QLatin1StringView name;
        void (*apply)(Reader &);
    };
    static constexpr Command commands[] = { { "M"_L1,
                                              [](Reader &reader) {
                                                  reader.path.moveTo(reader.point());
                                              } },
                                            { "L"_L1,
                                              [](Reader &reader) {
                                                  reader.path.lineTo(reader.point());
                                              } },
                                            { "V"_L1,
                                              [](Reader &reader) {
                                                  reader.path.lineTo(
                                                          reader.path.currentPosition().x(),
                                                          reader.number());
                                              } },
                                            { "Q"_L1,
                                              [](Reader &reader) {
                                                  const auto a = reader.point(), b = reader.point();
                                                  reader.path.quadTo(a, b);
                                              } },
                                            { "C"_L1,
                                              [](Reader &reader) {
                                                  const auto a = reader.point(), b = reader.point(),
                                                             c = reader.point();
                                                  reader.path.cubicTo(a, b, c);
                                              } },
                                            { "Z"_L1, [](Reader &reader) {
                                                 reader.path.closeSubpath();
                                             } } };
    Reader reader{ tokens, 0, { } };
    while (reader.at < tokens.size()) {
        const auto name = tokens[reader.at++];
        const auto command =
                std::find_if(std::begin(commands), std::end(commands), [&](const Command &entry) {
                    return entry.name == name;
                });
        if (command == std::end(commands))
            throw std::runtime_error("Invalid reveal command");
        command->apply(reader);
    }
    reader.path.setCachingEnabled(true);
    return reader.path;
}
}
