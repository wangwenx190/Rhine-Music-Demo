#pragma once
#include <QPainterPath>
#include <QRegularExpression>
#include <stdexcept>
#include <cmath>

namespace yalahoo {
// Grammar of the project's authored reveal centerlines; all coordinates are absolute.
inline QPainterPath parseBrandCenterline(const QString &d)
{
    const QRegularExpression token("[MLQCVZ]|[-+]?(?:[0-9]*\\.)?[0-9]+");
    QStringList tokens;
    auto matches = token.globalMatch(d);
    while (matches.hasNext())
        tokens.append(matches.next().captured());
    QString residue = d;
    residue.remove(token);
    residue.remove(QRegularExpression("[ ,\\t\\r\\n]"));
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
        if (command == "M")
            path.moveTo(point());
        else if (command == "L")
            path.lineTo(point());
        else if (command == "V")
            path.lineTo(path.currentPosition().x(), number());
        else if (command == "Q") {
            const auto a = point(), b = point();
            path.quadTo(a, b);
        } else if (command == "C") {
            const auto a = point(), b = point(), c = point();
            path.cubicTo(a, b, c);
        } else if (command == "Z")
            path.closeSubpath();
        else
            throw std::runtime_error("Invalid reveal command");
    }
    path.setCachingEnabled(true);
    return path;
}
}
