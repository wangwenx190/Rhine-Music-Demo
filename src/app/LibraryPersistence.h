// Connects durable library state to the live session. Owns automatic enrichment scheduling;
// the store, provider, session and startup coordinator must outlive this binding.
#pragma once
#include <QObject>
#include <memory>

namespace yalahoo {
class LibraryStore;
class OnlineMetadataProvider;
class LibrarySession;
class StartupCoordinator;
class LibraryPersistence final : public QObject
{
    Q_DISABLE_COPY_MOVE(LibraryPersistence)
public:
    LibraryPersistence(LibraryStore &store, OnlineMetadataProvider &provider,
                       LibrarySession &library, StartupCoordinator &startup,
                       QObject *parent = nullptr);
    ~LibraryPersistence() override;

private:
    struct Private;
    std::unique_ptr<Private> d;
};
}
