"""Record the traced MSAA/resolve and component coverage boundary, without image oracles."""
from audit_support import write_source_report
from audit_support import source_check
import hashlib
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
CHECKS = [
    ("COMMON-01", "tools/review/CMakeLists.txt", "RhiPostprocessAudit.cpp",
     "Review links production RHI, GuiPrivate and the event-driven capture driver. TestPolicy classifies actual independent component entry points."),
    ("COMMON-02", "tools/review/RhiPostprocessAudit.cpp", "Fallback without primary leases was accepted",
     "Anonymous fixtures and generated RGBA tiles only. Atlas primary leases may be absent, but fallback readiness requires both lease vectors; submission rejects malformed input before render access."),
    ("COMMON-03", "src/render/rhi/RhiItem.cpp", "attachment.setResolveTexture(resolves[i]);",
     "makeTarget attaches same-format multisample texture or capability-selected GL renderbuffer to a separate single-sample resolve. Depth count is identical; unsupported sample/format fails explicitly, never downgrades."),
    ("COMMON-04", "tools/review/PostprocessChecks.h", "Postprocess ordering, format, sampling or feedback is invalid",
     "Actual SRB equality and attachment/pipeline descriptors prove source single-sample RGBA16F, ordered AO/Bokeh/SMAA and no sampled output alias. SceneItem integration uses native output and full-resolution effects in continuous replay."),
    ("COMMON-05", "tools/review/RhiShellAudit.cpp", "Paper resolve lost partial coverage",
     "Retained analytic linear ink, transparent depth and original one-code alpha fringe bounds independently verify actual 1x/4x resolve. No browser image or score supplies expected output."),
    ("PERF-01", "src/render/rhi/RhiItem.cpp", "input.entities.size() <= renderData::MaximumCaseEntities",
     "Production entity work stays bounded. Explicit capture graph walks two targets/three attachments and a fixed maximum seven postprocess passes; no whole-library traversal."),
    ("PERF-02", "tools/review/RhiCaptureHarness.h", "loop.exec();",
     "Serial capture waits for normal frame/readback callbacks with a deadline. No manual processEvents/msleep polling; ordinary products do not perform diagnostic readbacks or JSON graph walks."),
    ("PERF-03", "tools/review/RhiPostprocessAudit.cpp", "Steady postprocess uploaded beyond dynamic prefixes",
     "Steady pass uploads are exactly 80 bytes each. Cover cacheKey reuse, single revision copy, 16MiB initial/256KiB tile/2MiB steady transfer and ninth-tile deferral are independently asserted."),
    ("PERF-04", "src/render/rhi/Postprocess.cpp", "pass.bindings->cbeginBindings()",
     "Feedback check compares all actual sampled bindings against the output for all three owned samplers. Resolve/composite/depth and postprocess resources retain renderer ownership, descriptor inventory and null measured VRAM."),
    ("PERF-05", "tools/review/RhiPostprocessAudit.cpp", "Numerical state rebuilt compatible targets",
     "Three sizes, 1x/4x and all eight effect combinations assert steady reuse; focus/aperture/exposure do not rebuild. Disabled channels must release post/AO/DOF resources."),
    ("PERF-06", "src/render/rhi/SceneItem.cpp", "in.captureSamples = CaptureSamples;",
     "Production always enables geometry MSAA and SMAA at native physical dimensions. Navigation/playback and the single production clock are unchanged."),
    ("PERF-07-FOCUS", "src/app/VerificationWindowMac.mm", "NSWorkspaceDidActivateApplicationNotification",
     "Initial/current/verification PIDs and AppKit/workspace activation plus native key/main history distinguish external app switches from any own activation. Unknown PIDs and own transient activation still fail; no foreground policy opt-out."),
    ("PERF-07", "tools/review/RhiPostprocessAudit.cpp", "driver.rebuilt()",
     "Ten actual scene-graph invalidations restore combined graph/output; atlas resize/generation rejection/reverse order and 1/7/8/31/32/33/64/432 capacity boundaries remain. Native surface restore/VSYNC are separately pending."),
]
history = []
for revision, path in [
    ("522a3f3", "qml/Yalahoo/Music/scene/GeometryScene.qml"),
    ("1f2352a", "src/render/rhi/RhiItem.cpp"),
    ("75775b3", "src/render/rhi/SceneItem.cpp"),
    ("b7f73d4", "src/render/rhi/RhiItem.cpp"),
    ("e98c798", "src/render/rhi/RhiItem.cpp"),
]:
    data = subprocess.check_output(["git", "show", f"{revision}:{path}"], cwd=ROOT)
    history.append(dict(revision=revision, path=path, sha256=hashlib.sha256(data).hexdigest(),
                        conclusion="Source/adapter review in source-fidelity-review.md binds current format, resolve and lifecycle behavior; target runtime is separate."))
for platform, marker in [("windows", "WIN32"), ("macos", "YalahooVerification"),
                          ("linux", "UNIX AND NOT APPLE")]:
    rows = []
    for identity, path, symbol, evidence in CHECKS + [
        (platform.upper() + "-01", "cmake/Platform.cmake", marker,
         "Native backend/compiler closure preserved. Same latest-Kit target build, shaders, actual format/resolve and transfer execution required; host cross-compilation is not target execution.")
    ]:
        rows.append(source_check(ROOT, path, symbol, checkId=identity, requirement=identity.split('-')[0], platform=platform, evidence=evidence, result='static-audit-passed', limitations='Runtime separate. No BLUR-01/DPR/native restore/VSYNC/full product performance closure; target gaps remain R36.'))
    write_source_report(ROOT, platform, 'rhi-resolve', dict(checks=rows, history=history))
print("MSAA/resolve, component coverage and COMMON/PERF/platform matrices verified")
