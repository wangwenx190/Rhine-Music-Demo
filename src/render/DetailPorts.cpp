#include "SelectionController.h"
#include "PresentationController.h"
#include "DetailPorts.h"
#include "SceneAdapter.h"
#include "LibrarySession.h"

namespace yalahoo {
PresentationPorts detailPorts(SceneAdapter &scene, SelectionController &selection,
                              LibrarySession &library)
{
    PresentationPorts ports;
    ports.archiveReady = [&selection] {
        return selection.overviewProgress() < .02 && selection.archiveReady();
    };
    ports.archiveInteractive = [&selection] {
        return selection.archiveInteractive();
    };
    ports.presentationReady = [&selection] {
        return selection.detailMotion().phase() == DetailMotion::Phase::Presented;
    };
    ports.enterCamera = [&scene] {
        scene.requestDetail(true);
    };
    ports.returnCamera = [&scene] {
        scene.requestDetail(false);
    };
    ports.select = [&selection, &library](const PresentationSelection &request) {
        if (selection.select(request.albumId))
            library.navigate(request.albumId, request.trackId);
    };
    ports.canSelect = [&selection](const PresentationSelection &request) {
        return selection.canSelect(request.albumId);
    };
    ports.adjacentAlbum = [&selection](const QString &id, int direction) {
        return selection.adjacentAlbum(id, direction);
    };
    ports.navigationEnabled = [&selection](const QString &id, int direction) {
        return selection.albumNavigationEnabled(id, direction);
    };
    ports.switchDetail = [&selection, &library](const PresentationSelection &request) {
        if (selection.switchDetail(request.albumId))
            library.navigate(request.albumId, request.trackId);
    };
    return ports;
}
}
