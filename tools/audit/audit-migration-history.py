"""Recover the reviewed Git/plan closure; keep provenance separate from runtime acceptance."""
from audit_support import source_check
import hashlib
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
FIRST, LAST = "4f785b1", "dd5f959"
INDEX = ROOT / "docs/contracts/source-fidelity-review.md"
OUTPUT = ROOT / "verification/r19h/history"


def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT)


def digest(data):
    return hashlib.sha256(data).hexdigest()


index = INDEX.read_text()
indexed = set(re.findall(r"(?<![0-9a-f])[0-9a-f]{7}(?:[0-9a-f]{33})?(?![0-9a-f])", index))
revisions = git("rev-list", "--reverse", f"{FIRST}^..{LAST}").decode().splitlines()
commits, headings = [], {}
for revision in revisions:
    if not any(revision.startswith(short) for short in indexed):
        raise SystemExit(f"Unreviewed migration commit: {revision}")
    paths = git("diff-tree", "--root", "--no-commit-id", "--name-only", "-r", revision).decode().splitlines()
    changes = []
    for path in paths:
        patch = git("show", "--format=", revision, "--", path)
        changes.append(dict(sourcePath=path, patchSha256=digest(patch)))
    plan = git("show", f"{revision}:QT_QUICK_MIGRATION_PLAN.md").decode()
    for line in plan.splitlines():
        match = re.match(r"^#{2,6}\s+(R\d+[A-Z0-9]*(?:-[A-Z0-9]+)?)\b", line)
        if match and int(re.match(r"R(\d+)", match[1])[1]) <= 19:
            headings.setdefault(match[1], dict(firstRevision=revision, heading=line))
    commits.append(dict(revision=revision, subject=git("show", "-s", "--format=%s", revision).decode().strip(),
                        indexReferences=[line for line in index.splitlines() if revision[:7] in line],
                        changes=changes))
# The global style commit touched shader macros and adjacent C++ string literals.
# Normalize only translation-phase line splicing, whitespace/comments and adjacent
# ordinary literals; preserve every other token and all literal contents.
TOKEN = re.compile(
    r'//[^\n]*|/\*[\s\S]*?\*/|R"([^ ()\\\t\n]{0,16})\([\s\S]*?\)\1"'
    r'|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|[A-Za-z_]\w*'
    r'|(?:\d+(?:\.\d*)?|\.\d+)(?:[eE][+-]?\d+)?[fFuUlL]*|[^\s]')


def semantic_tokens(data):
    source = re.sub(r"\\\r?\n", "", data.decode())
    result = []
    for match in TOKEN.finditer(source):
        token = match.group()
        if token.startswith(("//", "/*")):
            continue
        if token.startswith('"') and result and result[-1].startswith('"'):
            result[-1] = result[-1][:-1] + token[1:]
        else:
            result.append(token)
    return result


formatting = []
for path in git("diff-tree", "--no-commit-id", "--name-only", "-r", "5b2df80").decode().splitlines():
    if Path(path).suffix not in (".cpp", ".h", ".vert", ".frag", ".glsl"):
        continue
    # Only pre-existing implementation files are part of a formatting equivalence claim.
    before = subprocess.run(["git", "show", f"5b2df80^:{path}"], cwd=ROOT, capture_output=True)
    if before.returncode:
        continue
    after = git("show", f"5b2df80:{path}")
    equal = semantic_tokens(before.stdout) == semantic_tokens(after)
    formatting.append(dict(sourcePath=path, translationTokensEqual=equal,
                           beforeSha256=digest(before.stdout), afterSha256=digest(after)))
    if not equal:
        raise SystemExit(f"Formatting changed implementation tokens; manual classification required: {path}")

missing = set(headings) - set(re.findall(r"\bR\d+[A-Z0-9]*(?:-[A-Z0-9]+)?\b", index))
plan = (ROOT / "QT_QUICK_MIGRATION_PLAN.md").read_text()
# Platform handoffs are owned by the active plan, not duplicated in this index.
missing -= set(re.findall(r"\bR\d+[A-Z0-9]*(?:-[A-Z0-9]+)?\b", plan))
if missing:
    raise SystemExit(f"Unclassified historical task headings: {sorted(missing)}")

CHECKS = [
    ("COMMON-01", "cmake/TestPolicy.cmake", "hidden-render",
     "Root module/Review closure and native Kit are retained; source math/structure and actual hidden component rendering are independently classified."),
    ("COMMON-02", "src/app/ApplicationIdentity.h", "yalahoo",
     "Identity is initialized before application/standard paths. Source assets remain qrc aliases and explicit local URLs; no user library input or output in this review."),
    ("COMMON-03", "src/render/rhi/RhiItem.cpp", "clipSpaceCorrMatrix",
     "Shared immutable matrices reach geometry, shadow, shell and pick. Explicit resolve/capture and complete DOF/AO preserve source color/depth; independent component probes establish formulas, never source PNGs."),
    ("COMMON-03-REVIEW", "src/render/rhi/ReviewGlCapabilities.h", "caps.multisampledTexture = false",
     "Review-only Cocoa GL function capability correction chooses Qt's existing same-format 4x renderbuffer/resolve path. Latest-Kit private APIs are compiled at configure time; actual descriptors are read on the render thread, value snapshots alone reach GUI. No Kit patch, lower AA or product renderer change."),
    ("COMMON-04", "src/app/main.cpp", "playback.source().isEmpty()",
     "Anonymous runtime/detail/overview replays preserve navigation/playback separation. Public QtMultimedia source ownership remains separate from visual state; target media/device execution follows the current platform handoff."),
    ("COMMON-05", "tools/review/CoverAudit.cpp", "selectedUploadBytes",
     "Cover contain points and UV formulas are checked separately from actual GPU upload/lease/rebuild. Old report generators reject before evidence mutation. Runtime failure and source audit remain distinct."),
    ("PERF-01", "src/render/SelectionController.cpp", "SelectionController::publish",
     "Indexed library/column identity and fixed 432 shelves plus four owned copies bound per-frame work. Sort keys/depth/projections are lifted from comparisons; no per-frame full-library scan."),
    ("PERF-02", "src/services/CoverCache.cpp", "tryStart",
     "Three global workers and bounded descriptor/result/cache stages; generation/wanted checks precede paint/publication. GUI/render do not decode, wait or issue production readbacks."),
    ("PERF-03", "src/render/rhi/AtlasUpload.cpp", "generation",
     "Lease/serial/generation rejection precedes copy; immutable snapshots share revisions and partial tile uploads. Stable scene/cover inputs do not recopy pixels or rebuild all instances."),
    ("PERF-04", "src/render/rhi/ResourcePlan.h", "resourcePlan",
     "Attachment format/size/MSAA/resolve/mips and simultaneous old/new descriptors are validated before allocation. Camera/theme/focus retain compatible targets; driver VRAM remains unknown."),
    ("PERF-05", "src/render/rhi/SceneItem.cpp", "SceneItem::updatePolish",
     "Dirty frame/content revisions coalesce polish. Source math, local text shaping and constant shader data are cached with full keys; no source-less sampling scale retained."),
    ("PERF-06", "qml/Yalahoo/Music/pages/MusicStage.qml", "FrameAnimation",
     "One active visible frame clock; bounded text slots and virtual lists, latest navigation/drag and finite returning copies. Hidden activity stops; restore first interval is discarded."),
    ("PERF-07", "src/app/VerificationWindowMac.mm", "NSNormalWindowLevel",
     "Verification lifecycle uses actual native visibility/surface behavior. Default hidden policy is restored after authorized tests; native window, RenderControl, GPU rebuild and physical input evidence stay separate."),
]
matrices = {}
for platform, platform_id, path, symbol, reason in [
    ("windows", "WIN-01", "cmake/Platform.cmake", "WIN32", "MSVC/D3D11 branch and same-source qsb compile closure; target PE, runtime/deployment and native transfers still need Windows."),
    ("macos", "MAC-01", "src/app/GraphicsBackend.h", "QSurfaceFormat::CoreProfile", "Metal/threaded and OpenGL4.1/basic select before application creation. Current native/hidden runtime is validated separately; no screen/VSYNC claim from texture readback."),
    ("linux", "LNX-01", "cmake/Platform.cmake", "UNIX AND NOT APPLE", "Shared Qt/XDG/OpenGL path has no empty implementation; xcb/Wayland/GPU and deployment require the target environment."),
]:
    rows = []
    for identity, source, marker, evidence in CHECKS + [(platform_id, path, symbol, reason)]:
        rows.append(source_check(ROOT, source, marker, checkId=identity, requirement=evidence, platform=platform, evidence=evidence, result='static-audit-passed', limitations='Runtime, quality and native/physical inputs are separate; missing target facts remain in the migration plan R36.'))
    matrices[platform] = rows
OUTPUT.mkdir(parents=True, exist_ok=True)
(OUTPUT / "source-matrix.json").write_text(json.dumps(dict(
    range=dict(first=FIRST, last=LAST), indexSha256=digest(INDEX.read_bytes()),
    commits=commits, historicalTasks=headings, formattingEquivalence=formatting, checks=matrices,
    runtimeAcceptance=None), ensure_ascii=False, indent=2) + "\n")
print(f"History closure: {len(commits)} commits, {len(headings)} actual/planned historical headings; static COMMON/PERF/platform checks passed. Runtime not inferred.")
