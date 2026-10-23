// Parses and validates startup intent before any library, preference or instance mutation.
// Verification and session modes are exclusive; paths retain native Unicode and spaces.
#pragma once
#include <QStringList>
#include <optional>

namespace yalahoo {
enum class SessionMode {
    Product,
    Demo,
    LightingLab
};
enum class VerificationMode {
    None,
    App,
    Deployment,
    Overview,
    Detail,
    Navigation,
    Materials
};
struct ApplicationOptions final
{
    SessionMode session = SessionMode::Product;
    VerificationMode verification = VerificationMode::None;
    QString dataRoot, legacyImport, preferencesImport, verificationInput, materialReport;
    QStringList musicRoots;
    bool rescan = false;
    bool demonstration() const;
    bool verificationRun() const;
    bool appVerification() const;
    bool persistent() const;
};
enum class ApplicationArgumentError {
    InvalidCommandLine = 7,
    MaterialReportMissing = 12
};
struct ApplicationParseResult final
{
    std::optional<ApplicationOptions> options;
    QString error, help;
    ApplicationArgumentError errorCode = ApplicationArgumentError::InvalidCommandLine;
};
ApplicationParseResult parseApplicationOptions(const QStringList &arguments, bool reviewEnabled);
}
