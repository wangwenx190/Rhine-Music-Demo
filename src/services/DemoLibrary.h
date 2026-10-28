// Builds the anonymous bundled demonstration library. Stable fixture identities use qrc assets
// and never read or persist the user music library.

#pragma once
#include "MusicTypes.h"
namespace yalahoo {
// Product assets and invented titles only; no filesystem or playable sources.
LibrarySnapshot demonstrationLibrary();
}
