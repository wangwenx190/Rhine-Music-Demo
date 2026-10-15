"""Bind the R19C1 call-chain audit to current sources; outputs stay local."""
import hashlib
import json
from pathlib import Path

root = Path(__file__).resolve().parents[2]
# These are reviewed call-chain conclusions. Presence guards detect source drift;
# they do not prove the conclusions or replace the Release tests/GPU checks.
checks = [
    ("COMMON-01", "src/render/rhi/CMakeLists.txt", "ShellMaterial.cpp",
     "YalahooRhi lists the shared material implementation and qsb inputs; Quick/Scene linkage "
     "and platform C++23 requirements remain. QML parameter tests create real QtQuick3D materials."),
    ("COMMON-02", "qml/Yalahoo/Music/scene/GlassMaterial.qml", "required property Texture roomTexture",
     "Room/DFG/LTC textures remain injected from CaseMaterials qrc FixedTextureData, with no "
     "runtime disk/Node/network path. Source exporter uses mapped Git and locked local dev dependencies."),
    ("COMMON-03", "src/render/rhi/RhiItem.cpp", "copy(u.material, material.transport);",
     "Transport and coat now enter std140 unchanged; the entity lid override changes roughness only. "
     "The 816-byte layout, bindings, clip/Y correction, capture and pipeline formats are unchanged. "
     "Native shell algorithms/LOD/sorting are still R19C2/D, not inferred from parameter equality."),
    ("COMMON-04", "src/app/MaterialReview.cpp", "if (calibration)",
     "ROI calibration/factor fixtures return before any show/capture. No player, output device, "
     "worker cancellation or music path is modified; Qt/renderer ownership and teardown are retained."),
    ("COMMON-05", "tests/unit/RhiMaterialTest.cpp", "continuousSourceAndQmlParameters",
     "45 frozen material samples check C++ and real QML properties; 210 earlier endpoint samples "
     "and lighting trajectories remain. ArchivePoolAudit checks actual upload bytes. No image "
     "score, browser witness or historical passing report proves this round."),
    ("PERF-01", "src/render/rhi/RhiItem.cpp", "uniformBases[kind + 1] = uniformTemplate(kind);",
     "Seven templates per frame and one lid scalar per relevant entity remain bounded by 432+4; "
     "no library traversal, key normalization, I/O or extra per-entity QColor conversion is added."),
    ("PERF-02", "src/render/rhi/RhiItem.cpp", "pending.size() < 2",
     "Only explicit capture reads submitted CPU bytes after normal render submission; there is no "
     "new production readback/wait/worker/event loop. Existing at-most-two captures stay bounded."),
    ("PERF-03", "src/render/rhi/RhiItem.cpp", "updates->updateDynamicBuffer(uniforms.get(), 0, cursor",
     "Input snapshots and unchanged resource payloads remain implicitly shared; uniformScratch "
     "uploads only cursor bytes. Removing arithmetic scales adds no pixel copy or texture upload."),
    ("PERF-04", "src/render/rhi/RhiItem.cpp", "p->capture.metadata[\"submittedShellUniforms\"]",
     "At most 3*(432+4) material tuples are serialized only for capture. No product attachment, "
     "MSAA/resolve edge, PSO/SRB compatibility key or resource lifetime changes with optics values."),
    ("PERF-05", "src/render/rhi/ShellMaterial.cpp", "ShellMaterial shellMaterial",
     "Analytic source theme/clarity interpolation remains; image-derived factors are removed rather "
     "than approximated. Environment override is supplied once, without the base .65 multiplier."),
    ("PERF-06", "qml/Yalahoo/Music/scene/GlassMaterial.qml", "property real environmentIntensity",
     "QML keeps the same bindings and shared SelectionLighting clock with fewer dependencies; "
     "there are no new ticks, delegates, input events or hidden animation work."),
    ("PERF-07", "tools/review/ArchivePoolAudit.cpp", "submittedShellUniforms",
     "Capture owns value-only tuples; no pointers into temporary uniformScratch escape the renderer. "
     "Existing actual sceneGraphInvalidated, bounded replay, cover leases and shutdown tests remain."),
]
platforms = {
    "windows": ("WIN-01", "cmake/Platform.cmake", "WIN32",
                "MSVC options remain WIN32-only; scalar/QML changes add no OS API. D3D11 adapters "
                "remain behind Q_OS_WIN, qsb emits HLSL50. Native Windows compiler/GPU unavailable here."),
    "macos": ("MAC-01", "cmake/Platform.cmake", "YalahooVerification",
              "APPLE links the native verification policy before application/window creation. "
              "Current Release Metal checks use RenderControl; no show/activation or VSYNC claim."),
    "linux": ("LNX-01", "cmake/Platform.cmake", "UNIX AND NOT APPLE",
              "Linux keeps real Qt/GLSL/resource implementations and portable scalar math, with no "
              "Windows library or placeholder. Native xcb/Wayland Kit/compiler/GPU unavailable here."),
}
for platform, platform_check in platforms.items():
    rows = []
    for check_id, path, symbol, evidence in checks + [platform_check]:
        content = (root / path).read_bytes()
        if " ".join(symbol.split()) not in " ".join(content.decode().split()):
            raise SystemExit(f"Audit source drift: {path}::{symbol}")
        rows.append({
            "checkId": check_id, "requirement": evidence, "platform": platform,
            "sourcePath": path, "lineOrSymbol": symbol,
            "sourceSha256": hashlib.sha256(content).hexdigest(), "evidence": evidence,
            "result": "static-audit-passed",
            "limitations": "Manual parameter/call-chain scope only; actual runs are listed separately "
                           "in source-fidelity-review.md. Target runtime remains R34/R19C2/D.",
        })
    output = root / f"verification/{platform}-static/material-parameters"
    output.mkdir(parents=True, exist_ok=True)
    (output / "audit-matrix.json").write_text(json.dumps(rows, indent=2) + "\n")
    (output / "audit-report.md").write_text(
        f"# R19C1 {platform} static parameter audit\n\n"
        + "\n\n".join(f"{row['checkId']} `{row['sourcePath']}::{row['lineOrSymbol']}`: "
                       + row['evidence'] for row in rows)
        + "\n\nStatic audit passed; native target runtime is separate (R34).\n")
print("R19C1 static matrices generated for Windows, macOS and Linux")
