// Each settings transition must produce current telemetry before resource reuse is assessed.
#pragma once
#include <QJsonObject>
#include <QString>

namespace yalahoo::review {
void checkSettingsTransition(auto &check, auto &render, const QJsonObject &sample,
                             const QJsonObject &baseline, const QString &name)
{
    using namespace Qt::StringLiterals;
    const bool rendered = render();
    check(name + u"-resource-render"_s, rendered);
    check(name + u"-keeps-resources"_s,
          rendered && !sample.isEmpty()
                  && sample[u"resourceGeneration"_s] == baseline[u"resourceGeneration"_s]
                  && sample[u"postprocessRevision"_s] == baseline[u"postprocessRevision"_s]);
}
}
