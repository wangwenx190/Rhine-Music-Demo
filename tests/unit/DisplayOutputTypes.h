#pragma once
#include "DisplayOutput.h"
#include <QtQml/qqmlregistration.h>

struct DisplayOutputTestType final
{
    Q_GADGET
    QML_FOREIGN(yalahoo::DisplayOutput)
    QML_NAMED_ELEMENT(DisplayOutput)
    QML_UNCREATABLE("Owned by test")
};
