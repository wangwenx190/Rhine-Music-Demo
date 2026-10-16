"""Bind the R21 source review to symbols/hashes; target execution remains separate."""
import hashlib
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
mapping = json.loads((ROOT / "docs/contracts/history-rewrite.json").read_text())["revisions"]
baseline = mapping["57f4f845c20cce646a15dbf4008c2a162aaaf4e2"]
frozen = subprocess.check_output(["git", "show", f"{baseline}:scripts/music-library.mjs"], cwd=ROOT)
scanner = (ROOT / "src/library/DirectoryScanner.cpp").read_text()
source_extensions = set(re.findall(r"'\.([a-z0-9]+)'", frozen.decode().split("const BROWSER_EXTENSIONS")[0]))
native_extensions = set(re.findall(r'"([a-z0-9]+)"', scanner.split("audioExtensions{")[1].split("};")[0]))
if native_extensions != source_extensions or len(native_extensions) != 16:
    raise SystemExit("Frozen/native audio extensions differ")

# Evidence below records the actual manually reviewed call chains and failure paths.
# Marker assertions detect later source drift; they are not a substitute for that review.
CHECKS = [
    ("COMMON-01", "CMakeLists.txt", "add_subdirectory(src/library)",
     "Root CMake -> library target_sources(YalahooCore) -> DirectoryScanner/LibraryStore moc and QtConcurrent; main links existing Core. DirectoryScannerTest links the same implementation; Release native build supplies executable/moc proof."),
    ("COMMON-02", "src/library/DirectoryScanner.cpp", "QDir::toNativeSeparators(absolutePath).toUtf8()",
     "normalizeRoots accepts absolute local URLs, cleans lexically, caps 64 and removes nested/duplicate roots; QFileInfo/URL round trips Unicode. Native path bytes alone determine ID. symlink_status rejects links, iterator/stat errors roll back the whole root; no source content writes."),
    ("COMMON-03", "src/library/LibraryStore.cpp", "m_snapshot = result.snapshot",
     "N/A for new GPU algorithms/resources: GUI forwards the accepted library once to existing models/selection; worker never accesses QML or renderer. No shader/clip/format/sample changes. Shared hidden regression still required."),
    ("COMMON-04", "src/library/DirectoryScanner.cpp", "Track track = unchanged ? *oldTrack : Track{ }",
     "Scanner only enumerates/stats; changed/new tracks stay MediaSupport::Unknown. No QMediaPlayer or decoding/network API introduced. Unchanged fingerprints reuse local metadata; The accepted scan continues into the R22 MediaMetadataProbe stage; playback remains R24."),
    ("COMMON-05", "src/library/LibraryStore.cpp", "file.setDirectWriteFallback(false)",
     "load validates JSON/schema/root normalization transactionally; bad cache disables automatic writes. Worker serializes accepted snapshots, 256MiB read/write cap, QSaveFile write/commit errors preserve original. Source-level results and actual target tests are separate."),
    ("PERF-01", "src/library/DirectoryScanner.cpp", "tracks.insert(track.sourceUrl.toLocalFile(), &track)",
     "One ID/path index over prior library, O(A+T) space; traversal O(D+F) plus per-directory sorting, at most 64 bounded root membership passes for rollback. Iterative heap frontier avoids stack depth growth; no frame-loop traversal."),
    ("PERF-02", "src/library/LibraryStore.cpp", "m_worker.setMaxThreadCount(1)",
     "One watcher/job, one latest scan intent and one accepted save slot. All iteration/read/JSON/write stays in worker. Cancellation checked per file/directory and while indexing previous records; no per-file GUI signals or GUI worker waiting. R22 metadata runs on its separate event-loop thread after this scan worker returns."),
    ("PERF-03", "src/library/LibraryStore.cpp", "if (generation != m_generation || result.cancelled)",
     "Only current complete scan becomes const snapshot on GUI; stale result rejected before model replacement/save. Root changes during load retain baseline and cache safety but suppress obsolete display. Model resets once per accepted snapshot, never per file."),
    ("PERF-04", "src/library/DirectoryScanner.h", "Snapshot snapshot",
     "N/A for new GPU resources/passes. CPU live data is prior/current implicit-shared library, worker-built snapshot/index/frontier and one coalesced accepted save; serialize buffer exists only during single worker write. No new render allocation/upload path."),
    ("PERF-05", "src/library/DirectoryScanner.cpp", 'oldTrack->fields.value("_fingerprint") == stamp',
     "Existing metadata reused only by path and exact size:mtimeMs; change resets support/probe token while preserving unknown fields. IDs use native separators/UTF-8 SHA256, verified against frozen Node semantics without hashing audio contents."),
    ("PERF-06", "src/app/main.cpp", "library.setSnapshot(*snapshot)",
     "One GUI LibrarySession application triggers existing model/order/selection pipeline; no window reload, per-file sorting or hot-path disk work. 1500-album test records event-loop heartbeat during worker scan and one model reset; full product input/presentation cost remains R28."),
    ("PERF-07", "src/library/LibraryStore.cpp", "m_worker.waitForDone()",
     "Wait only at destruction; cancel unfinished scan, never persist unaccepted result, finish accepted coalesced save. Qt connections own callback lifetime. Source mutations/old cache destruction are absent; native Windows/Linux filesystem/ACL/junction checks remain R21-P."),
]

platform_checks = {
    "windows": ("WIN-01", "std::chrono::clock_cast<std::chrono::system_clock>(time)",
                "Wide filesystem path + Qt URL conversion; native backslashes for hash. Root comparison is case insensitive. MSVC's optional file_clock to_utc API uses clock_cast instead of assuming to_sys. Actual MSVC/NTFS/UNC/ACL/junction and atomic replacement execution pending R21-P."),
    "macos": ("MAC-01", "return fs::path(value.toUtf8().toStdString());",
              "POSIX UTF-8 paths, case-preserving IDs, file_clock -> system time. Local Release scanner/model/cache/permissions and Node stat checks are executable; physical UI/presentation is outside this non-graphical source review."),
    "linux": ("LNX-01", "return QString::fromUtf8(value.native());",
              "POSIX UTF-8 paths, case sensitive root comparisons, error_code directory iteration and atomic cache rename. Actual Linux build/filesystem/permissions/readonly execution pending R21-P; no OpenGL context needed."),
}
for platform, platform_check in platform_checks.items():
    check_id, marker, evidence = platform_check
    rows = []
    for identity, source_path, symbol, explanation in CHECKS + [(check_id, "src/library/DirectoryScanner.cpp", marker, evidence)]:
        data = (ROOT / source_path).read_bytes()
        if " ".join(symbol.split()) not in " ".join(data.decode().split()):
            raise SystemExit(f"R21 source drift: {source_path}::{symbol}")
        rows.append(dict(checkId=identity, requirement=explanation, platform=platform,
                         sourcePath=source_path, lineOrSymbol=symbol, evidence=explanation,
                         result="source-reviewed", sourceSha256=hashlib.sha256(data).hexdigest(),
                         limitations="Markers bind the manual source review; actual test/build execution is separate. R21-P retains missing target checks."))
    dependencies = []
    for path in ["src/library/CMakeLists.txt", "src/library/LibraryStore.h", "src/library/DirectoryScanner.h",
                 "src/services/LibrarySession.cpp", "tests/unit/DirectoryScannerTest.cpp",
                 "tests/unit/CMakeLists.txt", "tools/review/verify-scan-fingerprint.mjs"]:
        dependencies.append(dict(sourcePath=path, sourceSha256=hashlib.sha256((ROOT / path).read_bytes()).hexdigest()))
    output = ROOT / "verification" / f"{platform}-static" / "library-scan"
    output.mkdir(parents=True, exist_ok=True)
    (output / "source-matrix.json").write_text(json.dumps(dict(checks=rows, dependencies=dependencies,
        frozen=dict(revision=baseline, sourcePath="scripts/music-library.mjs",
                    lineOrSymbol="safeRootList/walkAlbums/albumEntryId/readAlbum/writeJsonAtomic/scan",
                    sourceSha256=hashlib.sha256(frozen).hexdigest())), indent=2) + "\n")
print("R21 frozen extension set and COMMON/PERF/platform source bindings verified; target runtime remains separate")
