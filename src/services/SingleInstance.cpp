#include "SingleInstance.h"
#include <QCryptographicHash>
#include <QDir>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QTimer>
#include <QPointer>
#include <QDeadlineTimer>
#include <utility>

using namespace Qt::StringLiterals;
namespace {
constexpr auto ActivationTimeoutMilliseconds = 1500;
constexpr auto MaximumActivationBytes = 16;
constexpr auto EndpointDigestHexCharacters = 32;
}
namespace yalahoo {

struct SingleInstance::Private final
{
    Q_DISABLE_COPY_MOVE(Private)
    explicit Private(SingleInstance *owner) : q(owner)
    {
    }
    SingleInstance *const q;

    void accept();
    void receive(QLocalSocket &socket);
    // Destroy the endpoint before releasing its ownership lock.
    std::unique_ptr<QLockFile> lock;
    QLocalServer server;
    QString error;
    QPointer<QLocalSocket> activePeer;
    bool started = false;
};
SingleInstance::SingleInstance(QObject *parent)
    : QObject(parent), d(std::make_unique<Private>(this))
{
    connect(&d->server, &QLocalServer::newConnection, this, [this] {
        d->accept();
    });
}
SingleInstance::~SingleInstance() = default;
QString SingleInstance::error() const
{
    return d->error;
}
SingleInstance::Result SingleInstance::start(const QString &dataRoot, bool enabled)
{
    if (std::exchange(d->started, true)) {
        d->error = tr("Instance arbitration has already started.");
        return Result::Error;
    }
    if (!enabled)
        return Result::Primary;
    if (!QDir::isAbsolutePath(dataRoot) || !QDir().mkpath(dataRoot)) {
        d->error = tr("Cannot create the application data directory.");
        return Result::Error;
    }
    const QDir root(QDir(dataRoot).canonicalPath());
    const auto digest =
            QCryptographicHash::hash(root.absolutePath().toUtf8(), QCryptographicHash::Sha256)
                    .toHex()
                    .first(EndpointDigestHexCharacters);
    const auto endpoint = u"yalahoo-"_s + QString::fromUtf8(digest);
    d->lock = std::make_unique<QLockFile>(root.filePath(u"instance.lock"_s));
    // A live owner is never evicted because its application has been running for a long time.
    d->lock->setStaleLockTime(0);
    if (d->lock->tryLock(ActivationTimeoutMilliseconds)) {
        QLocalServer::removeServer(endpoint);
        d->server.setSocketOptions(QLocalServer::UserAccessOption);
        d->server.setMaxPendingConnections(1);
        if (d->server.listen(endpoint))
            return Result::Primary;
        d->error = tr("Cannot listen for application activation: ") + d->server.errorString();
        d->lock->unlock();
        return Result::Error;
    }
    if (d->lock->error() != QLockFile::LockFailedError) {
        d->error = tr("Cannot lock the application data directory.");
        return Result::Error;
    }
    // Startup only, before any window/audio/library side effects. The peer acknowledges acceptance;
    // a hung owner is an error, never permission to create a second writer.
    QLocalSocket peer;
    peer.setReadBufferSize(MaximumActivationBytes);
    QDeadlineTimer deadline(ActivationTimeoutMilliseconds);
    peer.connectToServer(endpoint);
    const auto request = "activate\n"_ba;
    if (peer.waitForConnected(int(deadline.remainingTime()))
        && peer.write(request) == request.size()) {
        while (peer.bytesToWrite() > 0 && !deadline.hasExpired()) {
            if (!peer.waitForBytesWritten(int(deadline.remainingTime())))
                break;
        }
        // QLocalSocket is a byte stream: readiness need not deliver a complete ACK.
        // Its bounded read buffer accumulates fragments under the same exchange deadline.
        while (!peer.canReadLine() && !deadline.hasExpired()) {
            if (!peer.waitForReadyRead(int(deadline.remainingTime())))
                break;
        }
        if (peer.canReadLine() && peer.readAll() == "ok\n"_ba)
            return Result::Secondary;
    }
    d->error = tr("The existing application instance did not accept activation. Please try again.");
    return Result::Error;
}
void SingleInstance::Private::accept()
{
    // Keep at most one incomplete activation; do not accumulate sockets/timers from repeated launches.
    while (auto *peer = server.nextPendingConnection()) {
        if (activePeer) {
            peer->abort();
            peer->deleteLater();
            continue;
        }
        activePeer = peer;
        peer->setParent(q);
        peer->setReadBufferSize(MaximumActivationBytes);
        QObject::connect(peer, &QLocalSocket::readyRead, q, [this, peer] {
            receive(*peer);
        });
        QObject::connect(peer, &QLocalSocket::disconnected, peer, &QObject::deleteLater);
        QTimer::singleShot(ActivationTimeoutMilliseconds, peer, &QObject::deleteLater);
        receive(*peer);
    }
}
void SingleInstance::Private::receive(QLocalSocket &socket)
{
    auto *peer = &socket;
    if (!peer->canReadLine())
        return;
    if (peer->readAll() != "activate\n"_ba) {
        peer->disconnectFromServer();
        return;
    }
    peer->write("ok\n"_ba);
    peer->disconnectFromServer();
    Q_EMIT q->activationRequested();
}
}
