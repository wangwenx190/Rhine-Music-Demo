// Retains the first profile for one explicit renderer submission. Later clean frames cannot
// overwrite its work flags, and profiles from an earlier in-flight frame are ignored.
#pragma once
#include <QJsonObject>
#include <QMutex>
#include <QMutexLocker>
#include <utility>

namespace yalahoo::review {
class SubmissionProfile final
{
    Q_DISABLE_COPY_MOVE(SubmissionProfile)
public:
    explicit SubmissionProfile(quint64 submission) : m_submission(submission)
    {
    }
    bool publish(QJsonObject profile)
    {
        using namespace Qt::StringLiterals;
        const QMutexLocker lock(&m_mutex);
        if (!m_profile.isEmpty() || profile[u"submission"_s].toInteger() != qint64(m_submission))
            return false;
        m_profile = std::move(profile);
        return true;
    }
    QJsonObject value() const
    {
        const QMutexLocker lock(&m_mutex);
        return m_profile;
    }

private:
    const quint64 m_submission;
    mutable QMutex m_mutex;
    QJsonObject m_profile;
};
}
