#include "LibraryPersistence.h"
#include "LibraryStore.h"
#include "LibrarySession.h"
#include "OnlineMetadataProvider.h"
#include "StartupCoordinator.h"
#include <QTimer>
#include <utility>

using namespace Qt::StringLiterals;
namespace yalahoo {
struct LibraryPersistence::Private final
{
    Q_DISABLE_COPY_MOVE(Private)
    Private(LibraryStore &store, LibrarySession &library) : store(store), library(library)
    {
        automatic.setSingleShot(true);
    }
    void enrichWhenReady()
    {
        if (!automaticPending || store.scanning())
            return;
        const auto current = library.snapshot();
        const auto stored = store.snapshot();
        if (current->scan != stored->scan || current->albums.size() != stored->albums.size())
            return;
        automaticPending = false;
        const auto config = current->fields.value(u"metadataConfiguration"_s).toObject();
        if (config.value(u"networkEnabled"_s).toBool() && config.value(u"onlineEnabled"_s).toBool())
            library.requestMetadataBatch(u"enrichment"_s, true);
    }
    LibraryStore &store;
    LibrarySession &library;
    QTimer automatic;
    bool automaticPending = false;
};
LibraryPersistence::LibraryPersistence(LibraryStore &store, OnlineMetadataProvider &provider,
                                       LibrarySession &library, StartupCoordinator &startup,
                                       QObject *parent)
    : QObject(parent), d(std::make_unique<Private>(store, library))
{
    const auto configure = [&provider](const Snapshot &snapshot) {
        provider.configure(snapshot->fields.value(u"metadataConfiguration"_s).toObject());
    };
    connect(&store, &LibraryStore::snapshotReady, this, configure);
    connect(&store, &LibraryStore::metadataConfigurationChanged, this, configure);
    connect(&store, &LibraryStore::metadataConfigurationChanged, this,
            [&library](const Snapshot &snapshot) {
                library.adoptMetadataConfiguration(snapshot);
            });
    connect(&library, &LibrarySession::metadataDelivered, this,
            [&store](const ProviderResult &result) {
                store.stageMetadata(result);
            });
    connect(&library, &LibrarySession::metadataProgressChanged, this, [this] {
        if (!d->library.metadataProgress().value(u"active"_s).toBool())
            d->store.commitMetadata();
    });
    connect(&store, &LibraryStore::metadataCommitted, this, [&library](Snapshot snapshot) {
        library.adoptMetadataSnapshot(std::move(snapshot));
    });
    connect(&store, &LibraryStore::statusChanged, this, [this] {
        if (d->store.scanning())
            d->library.cancelAllMetadata();
    });
    connect(&store, &LibraryStore::scanFinished, this, [this](quint64, quint64) {
        d->automaticPending = true;
        d->automatic.start();
    });
    connect(&d->automatic, &QTimer::timeout, this, [this] {
        d->enrichWhenReady();
    });
    connect(&library, &LibrarySession::snapshotChanged, this, [this] {
        d->enrichWhenReady();
    });
    connect(&store, &LibraryStore::snapshotReady, this, [&startup](Snapshot snapshot) {
        startup.receive(std::move(snapshot));
    });
    connect(&store, &LibraryStore::failed, this, [&store, &startup](const QString &) {
        if (!startup.statistics()[u"initialReady"_s].toBool())
            startup.receive(store.snapshot());
    });
}
LibraryPersistence::~LibraryPersistence() = default;
}
