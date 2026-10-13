#pragma once
#include "LayoutMetrics.h"
#include <QCanvasPainterItem>
#include <QtQml/qqmlregistration.h>
struct UiLayoutMetrics
{
    Q_GADGET
    QML_FOREIGN(yalahoo::LayoutMetrics)
    QML_NAMED_ELEMENT(LayoutMetrics)
    QML_UNCREATABLE("Injected by review")
};
struct CanvasPainterBase
{
    Q_GADGET
    QML_FOREIGN(QCanvasPainterItem)
    QML_ANONYMOUS
};
