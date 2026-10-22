// Owns an isolated settings session for persistence and field-routing tests.
// Dependencies are declared in lifetime order so settings disconnects before its controllers.

#pragma once
#include "PreferencesStore.h"
#include "OfflineMetadataProvider.h"
#include "LibrarySession.h"
#include "MotionController.h"
#include "SceneAdapter.h"
#include "SelectionController.h"
#include "SettingsController.h"

namespace yalahoo::test {
struct SettingsFixture final
{
    Q_DISABLE_COPY_MOVE(SettingsFixture)
    explicit SettingsFixture(const QString &directory)
        : preferences(directory),
          library(&provider),
          scene(motion.clock()),
          selection(&motion),
          settings(&preferences, &library, &motion, &scene, &selection)
    {
    }
    PreferencesStore preferences;
    OfflineMetadataProvider provider;
    LibrarySession library;
    MotionController motion;
    SceneAdapter scene;
    SelectionController selection;
    SettingsController settings;
};
}
