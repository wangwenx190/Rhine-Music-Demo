"""R22 source review bindings. Runtime results remain in CTest's local reports."""
from audit_support import BASELINE
from audit_support import source_check
import hashlib
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
mapping = json.loads((ROOT / "docs/contracts/history-rewrite.json").read_text())["revisions"]
baseline = mapping[BASELINE]
frozen = subprocess.check_output(["git", "show", f"{baseline}:scripts/music-library.mjs"], cwd=ROOT)
probe = (ROOT / "src/media/MediaMetadataProbe.cpp").read_text()
for forbidden in [r"\bThumbnailImage\b", r"\bQAudioOutput\b", r"\bQAudio(?:Sink|Decoder|BufferOutput)\b",
                  r"\bQNetwork\w*", r"m_player\s*\.\s*play\s*\("]:
    if re.search(forbidden, probe):
        raise SystemExit(f"Probe violates the public silent metadata boundary: {forbidden}")
for key in ["Title", "AlbumTitle", "AlbumArtist", "ContributingArtist", "TrackNumber", "Date", "Genre", "Duration", "CoverArtImage"]:
    if f"QMediaMetaData::{key}" not in probe:
        raise SystemExit(f"Missing public metadata mapping: {key}")

# These explanations bind an actual manual control-flow/ownership review. Marker
# matches only detect drift; they do not establish correctness or target execution.
CHECKS = [
    ("COMMON-01", "CMakeLists.txt", "add_subdirectory(src/media)",
     "Root adds media/library sources to the same C++23 Core; Multimedia public link, QObject moc, same Kit and MediaMetadataTest link the production path. Existing qrc/qsb closures are unchanged; actual native Release build is separate."),
    ("COMMON-02", "src/library/LegacyImporter.cpp", "resolvedDestination(dataRoot)",
     "Explicit absolute local URLs only. Resolved source/destination directories cannot nest, including existing symlink ancestors. Imported track paths are native direct children; path IDs and producer/rule references remap deterministically. Only index/config/rules and allowed referenced artwork are read; no browser DB or PCM tree copy."),
    ("COMMON-03", "src/render/rhi/SceneItem.cpp", "void SceneItem::refreshPool()",
     "No shader/clip/sample/pass changes. Library replacement compares only bounded pool source descriptors; immutable content-addressed paths encode native revisions. Release only affected leases, invalidate only that identity's CPU results; stable atlas generation/pixel serials preserve other uploads. Existing normal high-resolution and returning ownership remain bounded."),
    ("COMMON-04", "src/media/MediaMetadataProbe.cpp", "slot.timer->start(ProbeTimeoutMilliseconds)",
     "Worker thread constructs two reused QMediaPlayers with no outputs/play. Source status, metadata and duration events lead to queued completion guarded by task serial. Cancel disconnects before clearing source; atomic generation cancellation rejects old results. Loaded metadata does not promote playback support; invalid/timeout retain the indexed track and fallback."),
    ("COMMON-05", "src/library/ArtworkStore.cpp", "file.setDirectWriteFallback(false)",
     "30MiB encoded input and valid dimensions precede file image decode; PNG/JPEG/WebP handlers enforce Qt's effective image allocation limit. Corrupt or allocation-rejected priority candidates fall back. Already-decoded embedded QImage is checked against the same effective limit before encoding, with opaque media decoder allocations explicitly outside public control. Content hash cache files and native index use owner-only QSaveFile; failures never mutate old source/song."),
    ("PERF-01", "src/media/MediaMetadataProbe.cpp", "void resolveGenres(LibrarySnapshot &snapshot)",
     "Snapshot cursor visits O(A+T) with two active slots, no per-track queued QObject/request list. Cached fingerprint plus Qt-version records skip media source loads. Image priority ranks and root-single case-folded stem lookup are indexed once, avoiding N*I audio/image scans. Track sorting costs sum(Ta log Ta); genre name/alias index avoids A*G normalization. Temporary indexes are O(A+T+genre aliases), covered by the bounded serialized library input."),
    ("PERF-02", "src/library/LibraryStore.cpp", "d->m_probe.start(result.snapshot, d->m_dataRoot, d->m_cancel, generation)",
     "Store holds one scan/import/save job or metadata batch, one last intent and one coalesced accepted save. Probe owns a dedicated event-loop thread; metadata/images never touch GUI/render. The 64-record cached admission slice yields to cancellations; no per-track GUI callbacks, polling or GUI waits. Thread join occurs only at destruction."),
    ("PERF-03", "src/library/LibraryStore.cpp", "void LibraryStore::accept(const JobResult &result, Kind kind, quint64 generation)",
     "Scan -> complete metadata batch -> current-generation const snapshot -> one GUI model reset -> serialized atomic save. Stale scan/import/probe completion cannot publish or persist. LibraryStore::probing exposes preparation separately; roots changed during probe preserve only the latest intent."),
    ("PERF-04", "src/library/ArtworkStore.cpp", "QImageReader::allocationLimit()",
     "Image copying is sequential on the worker: at most 30MiB encoded input plus Qt-limited image expansion, then content-addressed file. QT_IMAGEIO_MAXALLOC controls Qt's effective per-image limit; zero explicitly disables that guard. Cover workers remain globally limited to three; paint/mip estimates exclude source/codec/color-conversion memory and are not a total reservation. Two media backends retain opaque metadata/images; their internal allocation is not claimed measured/bounded by encoded file size. GPU graph/budgets unchanged; no new GPU textures/readbacks or atlas-wide resets."),
    ("PERF-05", "src/services/CoverCache.cpp", "void CoverCache::invalidate(const QString &identity)",
     "Invalidation cancels identity's queued/running work and removes only its completed LRU; pointer-token checks reject obsolete same-path results. Cache hits don't signal ready/redecode. SceneItem remaps surviving same-cover selected ownership across snapshot replacement, preserving tile serials; returning snapshots retain immutable old pixels until their owners release them."),
    ("PERF-06", "src/services/PreferencesStore.cpp", "bool PreferencesStore::importJson(const QUrl &url)",
     "Explicit browser JSON import is capped at 1MiB and parsed in serial worker. One import plus a revision guard; newer navigation intent rejects stale import. Unknown JSON merges, accepted settings publish once, writes coalesce. No QML/window creation, network or changes to the media playback controller."),
    ("PERF-07", "src/media/MediaMetadataProbe.cpp", "worker->abort()",
     "Cancellation stops timers/disconnects sources, advances slot serials and clears batch data; two reusable source owners die on their event-loop thread. Shutdown joins only at teardown. All production media work uses public platform-neutral Qt API; target backend support/error semantics still require native Windows/Linux tests."),
]
PLATFORMS = {
    "windows": ("WIN-01", "Wide paths/UNC and QSaveFile replacement rely on Qt; MSVC same-Kit build and actual Windows media backend, Unicode/ACL/timeout/cover parsing remain R22-P in R36."),
    "macos": ("MAC-01", "Apple Silicon Release exercises Qt Multimedia and event-loop thread, real RIFF/ID3 CoverArtImage loading, generation replacement and hidden Metal shared regressions; no audio output or foreground window is needed for metadata."),
    "linux": ("LNX-01", "UTF-8/case-sensitive paths use Qt and existing native filesystem hash. Target compiler/plugins/audio format errors and permissions remain R22-P in R36. No OpenGL context is needed by metadata/import tests; graphical regressions remain R36."),
}
reference = ROOT.parent / "Rhine-Music-Database/Qt/tests/UiChecks.cpp"
reference_info = dict(available=reference.exists(), sourcePath="Qt/tests/UiChecks.cpp",
                      lineOrSymbol="cachedSelection + repeated resource hits / same album cover upload assertions",
                      decision="Only assertion intent adapted to existing native cache/atlas/owner tests; no implementation or test code copied.",
                      license="MIT; Copyright (c) 2026 LBEILC (repository LICENSE; NOTICE is not a blanket license)")
if reference.exists():
    reference_info["sourceSha256"] = hashlib.sha256(reference.read_bytes()).hexdigest()
for platform, (check_id, explanation) in PLATFORMS.items():
    rows = []
    checks = CHECKS + [(check_id, "src/media/MediaMetadataProbe.cpp", "class QtMetadataSource final", explanation)]
    for identity, path, symbol, evidence in checks:
        rows.append(source_check(ROOT, path, symbol, checkId=identity, requirement=evidence, platform=platform, evidence=evidence, result='source-reviewed', limitations='Manual source review binding; actual build/test outcomes remain separate. Missing targets retain R22-P/R36 gates.'))
    paths = ["src/media/CMakeLists.txt", "src/media/MediaMetadataProbe.h", "src/library/CMakeLists.txt",
             "src/library/DirectoryScanner.cpp", "src/library/LibraryStore.h", "src/library/LegacyImporter.h",
             "src/services/PreferencesStore.h", "src/app/main.cpp", "src/core/LibraryModels.cpp", "src/core/MusicTypes.cpp",
             "tools/review/ArchivePoolAudit.cpp",
             "src/render/rhi/SceneItem.h", "src/render/rhi/AtlasAllocator.cpp",
             "tests/unit/MediaMetadataTest.cpp", "tests/unit/CoverCacheTest.cpp", "tests/unit/CMakeLists.txt"]
    dependencies = [dict(sourcePath=path, sourceSha256=hashlib.sha256((ROOT / path).read_bytes()).hexdigest()) for path in paths]
    output = ROOT / "verification" / f"{platform}-static" / "media-metadata"
    output.mkdir(parents=True, exist_ok=True)
    (output / "source-matrix.json").write_text(json.dumps(dict(checks=rows, dependencies=dependencies,
        frozen=dict(revision=baseline, sourcePath="scripts/music-library.mjs",
                    lineOrSymbol="readAlbum/walkAlbums/resolveGenre/init/METADATA_VERSION/COMPATIBLE_METADATA_VERSIONS",
                    sourceSha256=hashlib.sha256(frozen).hexdigest()), reference=reference_info), indent=2) + "\n")
print("R22 public metadata mapping and COMMON/PERF/platform source bindings verified; target execution is separate")
