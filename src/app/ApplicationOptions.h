// Parses and validates startup intent before any library, preference or instance mutation.
// Session modes are exclusive; paths retain native Unicode and spaces.
#pragma once
#include <QStringList>
#include <optional>

class QCommandLineParser;
namespace yalahoo {
enum class SessionMode {
    Product,
    Demo,
    LightingLab,
};
struct ApplicationOptions final
{
    SessionMode session = SessionMode::Product;
    QString dataRoot, legacyImport, preferencesImport;
    QStringList musicRoots;
    bool rescan = false;
    bool demonstration() const;
    bool persistent() const;
};
enum class ApplicationArgumentError {
    InvalidCommandLine = 7,
};
struct ApplicationParseResult final
{
    std::optional<ApplicationOptions> options;
    QString error, help;
    ApplicationArgumentError errorCode = ApplicationArgumentError::InvalidCommandLine;
};
ApplicationParseResult parseApplicationOptions(const QStringList &arguments);
void addProductOptions(QCommandLineParser &parser);
ApplicationParseResult readProductOptions(const QCommandLineParser &parser);
}
