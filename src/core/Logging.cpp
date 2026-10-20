#include "Logging.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QMutex>
#include <QMutexLocker>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QUuid>
#include <algorithm>
#include <cstdio>
#include <string>
#ifdef Q_OS_WIN
#  include <qt_windows.h>
#else
#  include <unistd.h>
#endif

using namespace Qt::StringLiterals;

namespace yalahoo {
Q_LOGGING_CATEGORY(logApp, "yalahoo.app", QtInfoMsg)
Q_LOGGING_CATEGORY(logRender, "yalahoo.render", QtInfoMsg)
Q_LOGGING_CATEGORY(logLibrary, "yalahoo.library", QtInfoMsg)
Q_LOGGING_CATEGORY(logMedia, "yalahoo.media", QtInfoMsg)
Q_LOGGING_CATEGORY(logSettings, "yalahoo.settings", QtInfoMsg)

namespace {
QMutex sinkMutex;

bool prepareConsole()
{
#ifdef Q_OS_WIN
    // Preserve inherited pipes/files. Attach only when no stderr handle exists.
    auto output = GetStdHandle(STD_ERROR_HANDLE);
    if (!output || output == INVALID_HANDLE_VALUE) {
        AttachConsole(ATTACH_PARENT_PROCESS);
        output = GetStdHandle(STD_ERROR_HANDLE);
    }
    SetConsoleCP(CP_UTF8);
    SetConsoleOutputCP(CP_UTF8);
    DWORD mode = 0;
    return output && output != INVALID_HANDLE_VALUE && GetFileType(output) == FILE_TYPE_CHAR
            && GetConsoleMode(output, &mode)
            && SetConsoleMode(output,
                              mode | ENABLE_PROCESSED_OUTPUT | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#else
    return isatty(fileno(stderr)) == 1;
#endif
}

void consoleWrite(QtMsgType type, const QByteArray &bytes, bool color)
{
    const char *sequence = color
            ? (type == QtWarningMsg                                  ? "\033[33m"
                       : type == QtCriticalMsg || type == QtFatalMsg ? "\033[31m"
                                                                     : "")
            : "";
#ifdef Q_OS_WIN
    const auto output = GetStdHandle(STD_ERROR_HANDLE);
    const auto write = [output](const char *data, qsizetype size) {
        while (size > 0) {
            DWORD written = 0;
            const auto count = DWORD(std::min<qsizetype>(size, 1024 * 1024));
            if (!WriteFile(output, data, count, &written, nullptr) || !written)
                break;
            data += written;
            size -= written;
        }
    };
    write(sequence, qsizetype(std::char_traits<char>::length(sequence)));
    write(bytes.constData(), bytes.size());
    if (*sequence)
        write("\033[0m", 4);
#else
    std::fputs(sequence, stderr);
    std::fwrite(bytes.constData(), 1, size_t(bytes.size()), stderr);
    if (*sequence)
        std::fputs("\033[0m", stderr);
    std::fflush(stderr);
#endif
}
}

struct LoggingSession::State final
{
    Q_DISABLE_COPY_MOVE(State)
    State() = default;
    static State *active;
    Options options;
    QString stem;
    QFile file;
    std::unique_ptr<QLockFile> runLock;
    QtMessageHandler previous = nullptr;
    QDateTime nextCleanup;
    bool color = false;
    bool fileFailed = false;

    QString segment(int index) const
    {
        return QDir(options.directory).filePath(stem + u".%1.log"_s.arg(index));
    }

    void cleanup()
    {
        const auto now = QDateTime::currentDateTimeUtc();
        nextCleanup = now.addSecs(3600);
        const auto cutoff = now.addDays(-options.retentionDays);
        const QRegularExpression owned(
                u"^(yalahoo-\\d{8}T\\d{9}Z-\\d+-[0-9a-f-]{36})\\.\\d+\\.log$"_s);
        const QDir directory(options.directory);
        for (const auto &entry : directory.entryInfoList({ u"yalahoo-*.log"_s }, QDir::Files)) {
            const auto match = owned.match(entry.fileName());
            if (!match.hasMatch() || entry.isSymLink() || entry.lastModified() >= cutoff
                || match.captured(1) == stem)
                continue;
            QLockFile owner(directory.filePath(match.captured(1) + u".lock"_s));
            owner.setStaleLockTime(0);
            if (owner.tryLock()) {
                if (!QFile::remove(entry.absoluteFilePath()))
                    emergency(u"Expired log removal failed: "_s + entry.absoluteFilePath());
            }
        }
    }

    void emergency(const QString &reason)
    {
        // Do not recurse into Qt logging from its handler. Qt still owns formatting.
        const QMessageLogContext context(__FILE__, __LINE__, Q_FUNC_INFO, "yalahoo.app");
        auto bytes = qFormatLogMessage(QtCriticalMsg, context, reason).toUtf8();
        bytes.append('\n');
        consoleWrite(QtCriticalMsg, bytes, color);
    }

    bool open()
    {
        file.setFileName(segment(0));
        if (!file.open(QIODevice::WriteOnly | QIODevice::NewOnly)
            || !file.setPermissions(QFileDevice::ReadOwner | QFileDevice::WriteOwner)) {
            failFile();
            return false;
        }
        return true;
    }

    void failFile()
    {
        const auto reason = u"File logging disabled: path=%1 error=%2"_s.arg(file.fileName(),
                                                                             file.errorString());
        file.close();
        fileFailed = true;
        emergency(reason);
    }

    bool rotate()
    {
        file.close();
        const auto oldest = segment(options.archiveCount);
        if (QFile::exists(oldest) && !QFile::remove(oldest)) {
            emergency(u"Log rotation could not remove: "_s + oldest);
            fileFailed = true;
            return false;
        }
        for (int index = options.archiveCount - 1; index >= 0; --index) {
            const auto source = segment(index);
            if (QFile::exists(source) && !QFile::rename(source, segment(index + 1))) {
                emergency(u"Log rotation could not rename: "_s + source);
                fileFailed = true;
                return false;
            }
        }
        return open();
    }

    void write(const QByteArray &bytes)
    {
        if (fileFailed)
            return;
        if (QDateTime::currentDateTimeUtc() >= nextCleanup)
            cleanup();
        // Rotate before a complete record where possible. Oversized records span
        // segments on UTF-8 character boundaries, without exceeding the byte cap.
        if (file.size() && bytes.size() > options.segmentBytes - file.size() && !rotate())
            return;
        qsizetype offset = 0;
        while (offset < bytes.size()) {
            const qint64 room = options.segmentBytes - file.size();
            auto count = qsizetype(std::min<qint64>(room, bytes.size() - offset));
            while (count && offset + count < bytes.size()
                   && (static_cast<unsigned char>(bytes[offset + count]) & 0xc0) == 0x80)
                --count;
            if (!count) {
                if (!rotate())
                    return;
                continue;
            }
            if (file.write(bytes.constData() + offset, count) != count || !file.flush()) {
                failFile();
                return;
            }
            offset += count;
            if (offset < bytes.size() && !rotate())
                return;
        }
    }

    static void message(QtMsgType type, const QMessageLogContext &context, const QString &message)
    {
        // Qt can invoke this concurrently from GUI, render and worker threads.
        const QMutexLocker lock(&sinkMutex);
        auto bytes = qFormatLogMessage(type, context, message).toUtf8();
        if (bytes.isNull())
            return;
        bytes.append('\n');
        consoleWrite(type, bytes, active && active->color);
        if (active)
            active->write(bytes);
    }
};
LoggingSession::State *LoggingSession::State::active = nullptr;

LoggingSession::LoggingSession() : LoggingSession(Options{ })
{
}

LoggingSession::LoggingSession(Options options) : m_state(std::make_unique<State>())
{
    if (options.directory.isEmpty())
        options.directory =
                QDir(QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation))
                        .filePath(u"logs"_s);
    options.segmentBytes = std::max<qint64>(256, options.segmentBytes);
    options.archiveCount = std::clamp(options.archiveCount, 1, 32);
    options.retentionDays = std::max(1, options.retentionDays);
    m_state->options = std::move(options);
    m_state->color = prepareConsole();
    qSetMessagePattern(u"%{time yyyy-MM-ddTHH:mm:ss.zzz t} [%{type}] [%{category}] "
                       u"[pid=%{pid} tid=%{threadid}] %{message} "
                       u"(%{file}:%{line} %{function})"_s);
    m_state->stem =
            u"yalahoo-%1-%2-%3"_s
                    .arg(QDateTime::currentDateTimeUtc().toString(u"yyyyMMdd'T'HHmmsszzz'Z'"_s))
                    .arg(QCoreApplication::applicationPid())
                    .arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
    const auto &directory = m_state->options.directory;
    if (QFileInfo(directory).isSymLink() || !QDir().mkpath(directory)) {
        m_state->fileFailed = true;
        m_state->emergency(u"Cannot create log directory: "_s + directory);
    } else {
        m_state->cleanup();
        m_state->runLock =
                std::make_unique<QLockFile>(QDir(directory).filePath(m_state->stem + u".lock"_s));
        m_state->runLock->setStaleLockTime(0);
        if (!m_state->runLock->tryLock()) {
            m_state->fileFailed = true;
            m_state->emergency(u"Cannot lock log session: "_s + directory);
        } else {
            m_state->open();
        }
    }
    {
        const QMutexLocker lock(&sinkMutex);
        Q_ASSERT(!State::active);
        State::active = m_state.get();
        m_state->previous = qInstallMessageHandler(State::message);
    }
    qCInfo(logApp) << "Logging started" << "file=" << filePath()
                   << "segmentBytes=" << m_state->options.segmentBytes
                   << "archives=" << m_state->options.archiveCount
                   << "retentionDays=" << m_state->options.retentionDays;
}

LoggingSession::~LoggingSession()
{
    qCInfo(logApp) << "Logging stopped";
    const QMutexLocker lock(&sinkMutex);
    qInstallMessageHandler(m_state->previous);
    State::active = nullptr;
}

QString LoggingSession::filePath() const
{
    return m_state->segment(0);
}
}
