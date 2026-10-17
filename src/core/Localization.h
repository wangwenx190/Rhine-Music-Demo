#pragma once

#include <QLocale>
#include <QTranslator>

namespace yalahoo {
class Localization final
{
public:
    explicit Localization(const QLocale &locale = QLocale());
    ~Localization();
    Localization(const Localization &) = delete;
    Localization &operator=(const Localization &) = delete;
    bool loaded() const;

private:
    QTranslator m_translator;
    bool m_loaded = false;
};
}
