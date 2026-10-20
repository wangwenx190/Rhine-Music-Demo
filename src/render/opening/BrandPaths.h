// Defines the bundled brand outline as literal path data. Opening rendering reuses these paths
// without loading mutable global graphics objects.

#pragma once
#include <QPainterPath>
#include <QRegularExpression>
#include <stdexcept>
#include <cmath>

namespace yalahoo {
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
    qsizetype at = 0;
    auto number = [&]() {
        bool ok = false;
        const double value = at < tokens.size() ? tokens[at++].toDouble(&ok) : 0;
        if (!ok || !std::isfinite(value))
            throw std::runtime_error("Invalid reveal coordinate");
        return value;
    };
    auto point = [&]() {
        const double x = number(), y = number();
        return QPointF(x, y);
    };
    QPainterPath path;
    while (at < tokens.size()) {
        const auto command = tokens[at++];
        if (command == "M"_L1)
            path.moveTo(point());
        else if (command == "L"_L1)
            path.lineTo(point());
        else if (command == "V"_L1)
            path.lineTo(path.currentPosition().x(), number());
        else if (command == "Q"_L1) {
            const auto a = point(), b = point();
            path.quadTo(a, b);
        } else if (command == "C"_L1) {
            const auto a = point(), b = point(), c = point();
            path.cubicTo(a, b, c);
        } else if (command == "Z"_L1)
            path.closeSubpath();
        else
            throw std::runtime_error("Invalid reveal command");
    }
    path.setCachingEnabled(true);
    return path;
}
}
