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
bool ApplicationOptions::persistent() const
{
    return !demonstration();
}
void addProductOptions(QCommandLineParser &parser)
{
    parser.addOption(
            { u"demo"_s,
              QCoreApplication::translate(
                      "CommandLine",
                      "Use anonymous albums without reading saved data or starting audio") });
    parser.addOption({ u"lighting-lab"_s,
                       QCoreApplication::translate(
                               "CommandLine",
                               "Open lighting/motion controls with the anonymous demonstration") });
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
}
ApplicationParseResult readProductOptions(const QCommandLineParser &parser)
{
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
    const bool dataOptions = parser.isSet(u"data-dir"_s) || parser.isSet(u"music-root"_s)
            || parser.isSet(u"import-legacy"_s) || parser.isSet(u"import-preferences"_s)
            || parser.isSet(u"rescan-library"_s);
    if (options.demonstration() && dataOptions)
        return failure("Demonstration cannot be combined with data options");
    if (parser.isSet(u"import-legacy"_s)
        && (parser.isSet(u"music-root"_s) || parser.isSet(u"rescan-library"_s)))
        return failure("--import-legacy cannot be combined with scan options");
    // Validate every user-supplied input before a preference import can be scheduled.
    for (const auto &name :
         { u"data-dir"_s, u"music-root"_s, u"import-legacy"_s, u"import-preferences"_s }) {
        if (!parser.isSet(name))
            continue;
        for (const auto &path : parser.values(name))
            if (!QDir::isAbsolutePath(path))
                return { { }, u"--%1 requires an absolute path"_s.arg(name), { } };
    }
    options.dataRoot = parser.value(u"data-dir"_s);
    options.musicRoots = parser.values(u"music-root"_s);
    options.legacyImport = parser.value(u"import-legacy"_s);
    options.preferencesImport = parser.value(u"import-preferences"_s);
    options.rescan = parser.isSet(u"rescan-library"_s);
    return { std::move(options), { }, { } };
}
ApplicationParseResult parseApplicationOptions(const QStringList &arguments)
{
    QCommandLineParser parser;
    parser.setApplicationDescription(
            QCoreApplication::translate("CommandLine", "yalahOO!!! native music archive"));
    parser.addHelpOption();
    addProductOptions(parser);
    if (!parser.parse(arguments))
        return { { }, parser.errorText(), { } };
    if (parser.isSet(u"help"_s) || parser.isSet(u"help-all"_s))
        return { { }, { }, parser.helpText() };
    return readProductOptions(parser);
}
}
