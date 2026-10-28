// Wires navigation-only library selection in anonymous controller and UI fixtures.
// Receivers own the connection; all three objects must outlive the fixture interaction.

#pragma once
#include "LibrarySession.h"
#include "SelectionController.h"
#include "SceneAdapter.h"

namespace yalahoo::test {
inline void bindLibrarySelection(LibrarySession &library, SelectionController &selection,
                                 SceneAdapter &scene)
{
    scene.setSelection(&selection);
    library.navigate(selection.albumId());
    QObject::connect(&selection, &SelectionController::selectionChanged, &library,
                     [&library, &selection] {
                         library.navigate(selection.albumId());
                     });
}
}
