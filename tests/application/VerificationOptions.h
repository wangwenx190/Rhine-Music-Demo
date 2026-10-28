// Parses the self-check protocol before constructing any persistent product state.
#pragma once
#include "ApplicationOptions.h"
#include <utility>
namespace yalahoo {
enum class VerificationMode {
    None,
    App,
    Deployment,
    Overview,
    Detail,
    Navigation,
    Materials,
};
struct VerificationOptions final
{
    ApplicationOptions product;
    VerificationMode verification = VerificationMode::None;
    QString verificationInput, materialReport;
    bool verificationRun() const;
    bool appVerification() const;
    bool persistent() const;
};
enum class VerificationArgumentError {
    MaterialReportMissing = 12
};
struct VerificationParseResult final
{
    std::optional<VerificationOptions> options;
    QString error, help;
    int errorCode = std::to_underlying(ApplicationArgumentError::InvalidCommandLine);
};
VerificationParseResult parseVerificationOptions(const QStringList &arguments, bool reviewEnabled);
}
