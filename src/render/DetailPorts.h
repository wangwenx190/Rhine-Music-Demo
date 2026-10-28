// Connects presentation readiness to selected-case motion. The narrow bridge keeps UI lifecycle
// decisions separate from archive simulation.

#pragma once

namespace yalahoo {
struct PresentationPorts;
class SceneAdapter;
class SelectionController;
class LibrarySession;
PresentationPorts detailPorts(SceneAdapter &scene, SelectionController &selection,
                              LibrarySession &library);
}
