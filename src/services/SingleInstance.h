// Arbitrates ordinary product launches per canonical data directory. The lock owns the local
// activation endpoint; verification and demonstration launches never enter this protocol.
#pragma once
#include <QObject>
#include <QString>
#include <memory>

class QLocalSocket;

namespace yalahoo {
class SingleInstance final : public QObject
{
    Q_OBJECT
    Q_DISABLE_COPY_MOVE(SingleInstance)
public:
    enum class Result {
        Primary,
        Secondary,
        Error
    };
    explicit SingleInstance(QObject *parent = nullptr);
    ~SingleInstance() override;
    Result start(const QString &dataRoot, bool enabled);
    QString error() const;
Q_SIGNALS:
    void activationRequested();

private:
    struct Private;
    std::unique_ptr<Private> d;
};
}
