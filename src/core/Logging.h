// Owns the process log sink, bounded file rotation and terminal formatting. Shutdown restores
// the previous Qt handler before releasing its file state.

#pragma once

#include <QLoggingCategory>
#include <QString>
#include <memory>

namespace yalahoo {
namespace loggingPolicy {
// Default bounded file-log retention policy.
inline constexpr auto SegmentMiB = 8;
inline constexpr auto BytesPerKiB = 1024;
inline constexpr auto ArchiveCount = 4;
inline constexpr auto RetentionDays = 14;
}

Q_DECLARE_LOGGING_CATEGORY(logApp)
Q_DECLARE_LOGGING_CATEGORY(logRender)
Q_DECLARE_LOGGING_CATEGORY(logLibrary)
Q_DECLARE_LOGGING_CATEGORY(logMedia)
Q_DECLARE_LOGGING_CATEGORY(logSettings)

// Construct after application identity, before QGuiApplication; outlive all Qt objects.
class LoggingSession final
{
    Q_DISABLE_COPY_MOVE(LoggingSession)
public:
    struct Options final
    {
        QString directory;
        qint64 segmentBytes =
                loggingPolicy::SegmentMiB * loggingPolicy::BytesPerKiB * loggingPolicy::BytesPerKiB;
        int archiveCount = loggingPolicy::ArchiveCount;
        int retentionDays = loggingPolicy::RetentionDays;
    };
    LoggingSession();
    explicit LoggingSession(Options options);
    ~LoggingSession();
    QString filePath() const;

private:
    struct State;
    std::unique_ptr<State> m_state;
};
}
