#pragma once
#include "PresentationController.h"

namespace yalahoo {
class SceneAdapter;
class SelectionController;
class LibrarySession;
PresentationPorts detailPorts(SceneAdapter &scene, SelectionController &selection,
                              LibrarySession &library);
}
