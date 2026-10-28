// Loads bundled Qt translations before user-visible objects are constructed. The translator
// owner must outlive every object that uses the installed catalog.

#pragma once

#include <QLocale>
#include <QTranslator>

namespace yalahoo {
class Localization final
{
    Q_DISABLE_COPY_MOVE(Localization)
public:
    explicit Localization(const QLocale &locale = QLocale());
    ~Localization();
    bool loaded() const;

private:
    QTranslator m_translator;
    bool m_loaded = false;
};
}
