#include "Localization.h"

#include <QCoreApplication>

using namespace Qt::StringLiterals;

static void initializeTranslationResource()
{
    static const bool registered = [] {
        Q_INIT_RESOURCE(YalahooCore_translations);
        return true;
    }();
    Q_UNUSED(registered);
}

namespace yalahoo {
Localization::Localization(const QLocale &locale)
{
    initializeTranslationResource();
    const bool available = m_translator.load(locale, u"yalahoo"_s, u"_"_s, u":/i18n"_s)
            || m_translator.load(u":/i18n/yalahoo_en.qm"_s);
    m_loaded = available && QCoreApplication::installTranslator(&m_translator);
}
Localization::~Localization()
{
    if (m_loaded)
        QCoreApplication::removeTranslator(&m_translator);
}
bool Localization::loaded() const
{
    return m_loaded;
}
}
