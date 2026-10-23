// Required normal output is part of verification success. Diagnostic-only output stays optional.
#pragma once
#include "Require.h"
#include <QImage>
#include <QString>
using Qt::StringLiterals::operator""_s;

namespace yalahoo::review {
inline void saveRequiredImage(const QImage &image, const QString &path)
{
    require(!image.isNull() && image.save(path),
            qPrintable(u"Cannot save required output: "_s + path));
}
}
