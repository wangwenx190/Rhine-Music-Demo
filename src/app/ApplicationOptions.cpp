#include "ApplicationOptions.h"
#include <QCommandLineParser>
#include <QCoreApplication>
#include <QDir>
#include <utility>

using namespace Qt::StringLiterals;
namespace yalahoo {
bool ApplicationOptions::demonstration() const
{
    return session != SessionMode::Product;
}
bool ApplicationOptions::verificationRun() const
{
    return verification != VerificationMode::None;
}
bool ApplicationOptions::appVerification() const
{
    return verificationRun() && verification != VerificationMode::Materials;
}
bool ApplicationOptions::persistent() const
{
    return !demonstration() && !verificationRun();
}
ApplicationParseResult parseApplicationOptions(const QStringList &arguments, bool reviewEnabled)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(
            QCoreApplication::translate("CommandLine", "yalahOO!!! native music archive"));
    parser.addHelpOption();
    parser.addOption({ u"verify-app"_s,
                       QCoreApplication::translate(
                               "CommandLine", "Verify hidden product QML creation, then exit") });
    parser.addOption(
            { u"demo"_s,
              QCoreApplication::translate(
                      "CommandLine",
                      "Use anonymous albums without reading saved data or starting audio") });
    parser.addOption({ u"lighting-lab"_s,
                       QCoreApplication::translate(
                               "CommandLine",
                               "Open lighting/motion controls with the anonymous demonstration") });
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
    parser.addOption({ u"data-dir"_s,
                       QCoreApplication::translate("CommandLine", "Override the native data root"),
                       QCoreApplication::translate("CommandLine", "directory") });
    parser.addOption(
            { u"music-root"_s,
              QCoreApplication::translate(
                      "CommandLine",
                      "Scan a local absolute music directory (repeatable, replaces saved roots)"),
              QCoreApplication::translate("CommandLine", "directory") });
    parser.addOption(
            { u"import-legacy"_s,
              QCoreApplication::translate(
                      "CommandLine", "Read a legacy music-data-v3 directory into the native cache"),
              QCoreApplication::translate("CommandLine", "directory") });
    parser.addOption(
            { u"import-preferences"_s,
              QCoreApplication::translate("CommandLine",
                                          "Import an explicitly exported browser preferences JSON"),
              QCoreApplication::translate("CommandLine", "file") });
    parser.addOption(
            { u"rescan-library"_s,
              QCoreApplication::translate("CommandLine", "Rescan saved local music directories") });
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
    const auto failure = [](const char *message) -> ApplicationParseResult {
        return { { }, QString::fromUtf8(message), { } };
    };
    if (!parser.positionalArguments().isEmpty())
        return failure("Unexpected positional arguments");
    ApplicationOptions options;
    if (parser.isSet(u"lighting-lab"_s))
        options.session = SessionMode::LightingLab;
    else if (parser.isSet(u"demo"_s))
        options.session = SessionMode::Demo;
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
    if (options.demonstration() && (dataOptions || options.verificationRun()))
        return failure("Demonstration cannot be combined with data or verification options");
    if (options.verificationRun() && dataOptions)
        return failure("Verification cannot be combined with data options");
    if (parser.isSet(u"import-legacy"_s)
        && (parser.isSet(u"music-root"_s) || parser.isSet(u"rescan-library"_s)))
        return failure("--import-legacy cannot be combined with scan options");
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
                     ApplicationArgumentError::MaterialReportMissing };
        options.materialReport = parser.value(u"material-report"_s);
    }
    // Validate every user-supplied input before a preference import can be scheduled.
    for (const auto &name :
         { u"data-dir"_s, u"music-root"_s, u"import-legacy"_s, u"import-preferences"_s }) {
        if (!parser.isSet(name))
            continue;
        for (const auto &path : parser.values(name))
            if (!QDir::isAbsolutePath(path))
                return { { }, u"--%1 requires an absolute path"_s.arg(name), { } };
    }
    if (options.verificationRun() && options.verification != VerificationMode::App
        && options.verificationInput.isEmpty())
        return failure("Verification requires a nonempty input or output path");
    options.dataRoot = parser.value(u"data-dir"_s);
    options.musicRoots = parser.values(u"music-root"_s);
    options.legacyImport = parser.value(u"import-legacy"_s);
    options.preferencesImport = parser.value(u"import-preferences"_s);
    options.rescan = parser.isSet(u"rescan-library"_s);
    return { std::move(options), { }, { } };
}
}
