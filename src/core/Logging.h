#pragma once

#include <QLoggingCategory>
#include <QString>
#include <memory>

namespace yalahoo {
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
        qint64 segmentBytes = 8 * 1024 * 1024;
        int archiveCount = 4;
        int retentionDays = 14;
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
