import fs from 'node:fs/promises';
import path from 'node:path';
import {fileURLToPath, pathToFileURL} from 'node:url';

// Frozen source identity and imports are maintained by export-web-baseline.
// These destinations describe the current native owners, not a new task list.
export function destination(name) {
    const base = path.posix.basename(name);
    const rules = [
        [
            /^music-frame-timing\.ts$/, 'src/motion/FrameClock.cpp',
            'Single frame source and raw/motion time separation'
        ],
        [
            /^viewer-theme\.ts$/, 'src/render/SceneAdapter.cpp',
            'Scene theme interpolation and source material colors'
        ],
        [
            /^music-player\.ts$/, 'src/media/PlaybackController.cpp',
            'Public QtMultimedia song playback and retained album queue'
        ],
        [
            /^audio\.ts$/, 'src/media/InterfaceSoundController.cpp',
            'Independent event sound routing with available embedded assets'
        ],
        [
            /^demo-library\.ts$/, 'src/services/DemoLibrary.cpp',
            'Anonymous product demonstration library'
        ],
        [
            /^music-app\.ts$/,
            'src/app/main.cpp + qml/Yalahoo/Music/App.qml + qml/Yalahoo/Music/pages/MusicStage.qml',
            'Native composition, controller ownership and visual lifecycle'
        ],
        [/^music-types\.ts$/, 'src/core/MusicTypes.h', 'Library values and serialization'],
        [
            /^music-(order|search)\.ts$/, 'src/core/LibraryModels.cpp',
            'Cached ordering, grouping and search models'
        ],
        [
            /^data\.ts$/, 'src/services/LibrarySession.cpp',
            'Immutable library publication and independent navigation identity'
        ],
        [
            /^cover-atlas\.ts$/, 'src/services/CoverCache.cpp + src/render/rhi/AtlasUpload.cpp',
            'Bounded asynchronous covers and incremental atlas uploads'
        ],
        [
            /^music-(presentation|track-focus)\.ts$/,
            'src/presentation/ + qml/Yalahoo/Music/pages/Detail.qml + qml/Yalahoo/Music/components/TrackList.qml',
            'Detail content, navigation and virtualized track focus'
        ],
        [
            /^ui-transitions\.ts$/, 'src/presentation/SurfaceTransition.cpp',
            'Shared-clock interruptible surface transitions'
        ],
        [
            /^music-(title|title-reels|text-motion|transport-title)(\.ts|\.css)$/,
            'src/render/text/ + qml/Yalahoo/Music/components/TitleReel.qml + qml/Yalahoo/Music/components/TransportTitle.qml',
            'Shaped glyph and color text, bounded reels and shared-clock transport motion'
        ],
        [
            /^music-(ruler|ticks|wheel-navigation)\.ts$|^music-navigation-.*\.css$/,
            'src/render/navigation/ + qml/Yalahoo/Music/components/NavigationFrame.qml + qml/Yalahoo/Music/components/Ruler.qml',
            'Source navigation mathematics, input routing and C++ Canvas ruler'
        ],
        [
            /^music-(boot|boot-timing|opening)(\.ts|\.css)$/,
            'src/render/opening/ + src/render/StartupCoordinator.cpp + qml/Yalahoo/Music/pages/Opening.qml',
            'Native opening, readiness and interaction handoff'
        ],
        [
            /^music-(overview|overview-ui|overview-edge)(\.ts|\.css)$/,
            'src/render/OverviewController.cpp + src/render/OverviewLabelsModel.cpp + qml/Yalahoo/Music/pages/Overview.qml',
            'Overview framing, folding, labels and interaction'
        ],
        [
            /^music-real-columns\.ts$/, 'src/render/RealColumns.cpp + src/render/ColumnMemory.cpp',
            'Finite columns and session position memory'
        ],
        [
            /^music-archive-motion\.ts$/,
            'src/render/DetailMotion.cpp + src/render/ReturningCasePool.cpp + src/render/PointerDragController.cpp',
            'Extraction, returning cases and detail drag'
        ],
        [
            /^archive-loop\.ts$/, 'src/render/ArchivePool.cpp + src/render/SelectionController.cpp',
            'Bounded array and selection field'
        ],
        [
            /^music-array-layout\.ts$|^viewport-layout\.ts$/, 'src/render/LayoutMetrics.cpp',
            'Responsive logical viewport and source camera geometry'
        ],
        [
            /^music-camera\.ts$|^viewer-camera\.ts$/, 'src/render/SceneAdapter.cpp',
            'Scene frame and analytical camera transitions'
        ],
        [
            /^music-(motion-lab|motion-settings)\.ts$/,
            'src/motion/MotionController.cpp + src/render/SettingsController.cpp + qml/Yalahoo/Music/panels/MotionPanel.qml',
            'Motion speed and reduced-motion settings'
        ],
        [
            /^music-motion-ui\.ts$|^motion\.ts$/, 'src/motion/',
            'Shared clock, analytical springs and quintic tracks'
        ],
        [
            /^music-(lighting-lab|row-lighting)\.ts$/,
            'src/render/SelectionLighting.cpp + src/render/RowLightController.cpp + qml/Yalahoo/Music/panels/LightingPanel.qml',
            'Confirmed and temporary lighting controls and source world light field'
        ],
        [
            /settings|lab|theme/,
            'src/render/SettingsController.cpp + src/services/PreferencesStore.cpp + qml/Yalahoo/Music/panels/',
            'Persistent settings and native self-drawn panels'
        ],
        [
            /^scene\.ts$|model|quality-renderer|render-quality|music-lighting|cd-asset/,
            'src/render/rhi/ + src/render/SceneAdapter.cpp + src/render/RenderQuality.cpp',
            'Production RHI geometry, material, light field and postprocessing'
        ],
        [
            /^brand\.ts$|^asset-url\.ts$/, 'resources/branding/ + src/app/AssetCatalog.cpp',
            'Embedded assets and native product branding'
        ],
        [
            /^appearance\.ts$|^html\.ts$|^music\.css$|^style\.css$|^viewer-album-cover\.ts$/,
            'qml/Yalahoo/Music/components/ + qml/Yalahoo/Music/pages/',
            'Responsive layout, explicit controls and typography'
        ],
    ];
    return rules.find(([pattern]) => pattern.test(base))?.slice(1) ??
           [null, 'Frozen reference only; no native product implementation'];
}

export function serviceDestination(name) {
    const base = path.posix.basename(name);
    if (base === 'music-library.mjs')
        return [
            'src/library/ + src/media/MediaMetadataProbe.cpp',
            'Read-only local scan/import, metadata and artwork cache'
        ];
    if (/^(album-introductions|local-music-credits|qq-credits)\.mjs$/.test(base))
        return [
            'src/services/IMetadataProvider.h + src/services/OfflineMetadataProvider.cpp',
            'Asynchronous offline cache only; network execution deferred'
        ];
    return [null, 'Frozen Web tooling only; no native product service'];
}

const self = fileURLToPath(import.meta.url);
if (process.argv[1] && pathToFileURL(path.resolve(process.argv[1])).href === import.meta.url) {
    const root = path.resolve(path.dirname(self), '../..');
    const file = path.join(root, 'docs/contracts/source-map.json');
    const map = JSON.parse(await fs.readFile(file, 'utf8'));
    for (const entry of map.modules) {
        const [target, responsibility] = destination(entry.baselinePath);
        entry.target = entry.musicReachable ? target : null;
        entry.role = entry.target ? 'migrate' : 'reference-only';
        entry.responsibility = entry.musicReachable
                                   ? responsibility
                                   : 'Frozen reference only; no native product implementation';
    }
    for (const entry of map.services)
        [entry.target, entry.policy] = serviceDestination(entry.sourcePath);
    await fs.writeFile(file, JSON.stringify(map, null, 2) + '\n');
}
