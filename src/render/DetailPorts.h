#pragma once

namespace yalahoo {
struct PresentationPorts;
class SceneAdapter;
class SelectionController;
class LibrarySession;
PresentationPorts detailPorts(SceneAdapter &scene, SelectionController &selection,
                              LibrarySession &library);
}
