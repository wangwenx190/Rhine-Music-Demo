#include "VerificationOptions.h"
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <utility>

using namespace Qt::StringLiterals;
namespace yalahoo {
bool VerificationOptions::verificationRun() const
{
    return verification != VerificationMode::None;
}
bool VerificationOptions::appVerification() const
{
    return verificationRun() && verification != VerificationMode::Materials;
}
bool VerificationOptions::persistent() const
{
    return product.persistent() && !verificationRun();
}
VerificationParseResult parseVerificationOptions(const QStringList &arguments, bool reviewEnabled)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(
            QCoreApplication::translate("CommandLine", "yalahOO!!! native music archive"));
    parser.addHelpOption();
    parser.addOption({ u"verify-app"_s,
                       QCoreApplication::translate(
                               "CommandLine", "Verify hidden product QML creation, then exit") });
    addProductOptions(parser);
    parser.addOption({ u"verify-deployment"_s,
                       QCoreApplication::translate(
                               "CommandLine",
                               "Verify hidden product resources and media loading, then exit"),
                       QCoreApplication::translate("CommandLine", "report") });
    parser.addOption(
            { u"verify-overview"_s,
              QCoreApplication::translate("CommandLine", "Verify production overview and exit"),
              QCoreApplication::translate("CommandLine", "directory") });
    parser.addOption({ u"verify-detail"_s,
                       QCoreApplication::translate("CommandLine",
                                                   "Verify production detail motion and exit"),
                       QCoreApplication::translate("CommandLine", "directory") });
    parser.addOption(
            { u"verify-navigation"_s,
              QCoreApplication::translate("CommandLine", "Verify production navigation and exit"),
              QCoreApplication::translate("CommandLine", "directory") });
    if (reviewEnabled) {
        parser.addOption({ u"verify-materials"_s,
                           QCoreApplication::translate(
                                   "CommandLine", "Verify source material state and native output"),
                           QCoreApplication::translate("CommandLine", "fixture") });
        parser.addOption(
                { u"material-report"_s,
                  QCoreApplication::translate("CommandLine", "Material source/state report"),
                  QCoreApplication::translate("CommandLine", "report") });
    }
    if (!parser.parse(arguments))
        return { { }, parser.errorText(), { } };
    if (parser.isSet(u"help"_s) || parser.isSet(u"help-all"_s))
        return { { }, { }, parser.helpText() };
    const auto failure = [](const char *message) -> VerificationParseResult {
        return { { }, QString::fromUtf8(message), { } };
    };
    auto product = readProductOptions(parser);
    if (!product.options)
        return { { }, product.error, product.help, std::to_underlying(product.errorCode) };
    VerificationOptions options;
    options.product = std::move(*product.options);
    struct VerificationOption final
    {
        QLatin1StringView name;
        VerificationMode mode;
    };
    static constexpr VerificationOption modes[] = {
        { "verify-app"_L1, VerificationMode::App },
        { "verify-deployment"_L1, VerificationMode::Deployment },
        { "verify-overview"_L1, VerificationMode::Overview },
        { "verify-detail"_L1, VerificationMode::Detail },
        { "verify-navigation"_L1, VerificationMode::Navigation },
        { "verify-materials"_L1, VerificationMode::Materials }
    };
    for (const auto &entry : modes) {
        if (entry.mode == VerificationMode::Materials && !reviewEnabled)
            continue;
        if (!parser.isSet(entry.name))
            continue;
        if (options.verificationRun())
            return failure("Verification modes cannot be combined");
        options.verification = entry.mode;
        if (entry.mode != VerificationMode::App)
            options.verificationInput = parser.value(entry.name);
    }
    const bool dataOptions = parser.isSet(u"data-dir"_s) || parser.isSet(u"music-root"_s)
            || parser.isSet(u"import-legacy"_s) || parser.isSet(u"import-preferences"_s)
            || parser.isSet(u"rescan-library"_s);
    if (options.product.demonstration() && options.verificationRun())
        return failure("Demonstration cannot be combined with data or verification options");
    if (options.verificationRun() && dataOptions)
        return failure("Verification cannot be combined with data options");
    if (reviewEnabled) {
        if (parser.isSet(u"material-report"_s)
            && options.verification != VerificationMode::Materials)
            return failure("--material-report requires --verify-materials");
        if (options.verification == VerificationMode::Materials
            && (!parser.isSet(u"material-report"_s)
                || parser.value(u"material-report"_s).isEmpty()))
            return { { },
                     u"--verify-materials requires --material-report"_s,
                     { },
                     std::to_underlying(VerificationArgumentError::MaterialReportMissing) };
        options.materialReport = parser.value(u"material-report"_s);
    }
    if (options.verificationRun() && options.verification != VerificationMode::App
        && options.verificationInput.isEmpty())
        return failure("Verification requires a nonempty input or output path");
    return { std::move(options), { }, { } };
}
}
