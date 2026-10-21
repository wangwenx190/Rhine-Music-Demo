"""Bind the R30 deployment source review; native Windows execution remains independent."""
#!/usr/bin/env python3
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import subprocess
import tempfile
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[2]
# Each row binds a manually reviewed call chain, rather than treating a token as proof.
CHECKS = [
    ("COMMON-01", "CMakeLists.txt", "CMAKE_CXX_STANDARD 23",
     "Root enforces C++23 and runs real compiler/Qt ABI and module probes. QtKit re-enumerates the highest native Kit; missing newest modules fail without downgrade. App links YalahooCore/Scene/Rhi/Navigation/Text/Opening and public Qt modules; static qrc initializers and qsb outputs are linked into the executable. Production has Review OFF; no Quick3D target link, research QML, or research material resources."),
    ("COMMON-02", "cmake/Deployment.cmake", "set(deployment_qml",
     "file(GENERATE) serializes the actual application QML list and selected target/Kit. The deployer copies each listed relative path from root qml to an owned scan root, preserving directory imports; no source mutation. Product C++ types stay statically registered. windeployqt scans that root and the real PE, then qt.conf resolves QML/plugins relative to the deployed executable. Fonts, shaders, lookup/geometry/audio and branding use qrc, not the working directory."),
    ("COMMON-03", "src/app/GraphicsBackend.h", "QSGRendererInterface::Direct3D11",
     "Windows ordinary startup explicitly selects D3D11; explicit OpenGL remains supported and is deferred to R36. Deployed shaders are the same qt_add_shaders multi-target qsb linked into Rhi/Opening/Navigation/Text/App. Deployment does not change clip/Y/depth, formats, MSAA/resolve, capture ordering, effects or sampling. CanvasPainter remains linked in the product and is required in the output closure."),
    ("COMMON-04", "src/app/main.cpp", "yalahoo::PlaybackController playback",
     "App owns PlaybackController/BackgroundMusicController/InterfaceSoundController outside the QML visual lifetime. Public QtMultimedia source/seek/volume/device/error/EndOfMedia logic and queued metadata generation rejection remain unchanged. Shutdown stops all three lanes before QGuiApplication teardown. windeployqt collects Qt multimedia plugins and bundled media dependencies; every deployed PE import/delay import must resolve, while actual device/routing/decoding remains R30-P/R24-P/R25-P."),
    ("COMMON-05", "tools/deploy/verify-windows.ps1", "functionalReportRequired",
     "Clean verification hashes the inventory, uses a fresh output/current directory outside the package, clears child Qt/QML/graphics overrides and restricts PATH to Windows/System32. Verification waits for this GUI Process.ExitCode plus this product/report.json passed flag, rejects timeout/missing/failure, and preserves stdout/stderr/environment. Demo/help/normal zero exit is not a quality/audio/performance pass. No Windows binary or GPU execution is inferred from this source review."),
    ("WIN-01", "cmake/yalahoo.rc.in", "IDI_ICON1 ICON",
     "The WIN32 branch enables RC and embeds the original R03B ICO with UTF-8 source paths and yalahOO!!! product metadata. WIN32_EXECUTABLE gives the GUI entry; technical output is yalahoo.exe and Qt manifest identifier yalahoo.music. QGuiApplication receives all original PNG sizes, including the small optical variants; the settings attribution uses the same SVG. Actual PE resource/Explorer/taskbar evidence requires Windows."),
    ("WIN-01", "tools/deploy/windows.ps1", "Invoke-NativeTool $deployTool",
     "Native Windows guard precedes mutation. The script imports discovered MSVC, configures an isolated production Release, verifies MSVC/Windows/Review metadata and selected Kit deployment tool versions, then calls windeployqt. It requires CanvasPainter/Quick/Multimedia/QML/platform/SVG outputs, rejects development components, checks imports via dumpbin, includes architecture-matched official VC redist and attribution, and inventories only collected runtime files. All staging is owned, finally-cleaned, and replacement restores an old package on rename failure. It never copies a whole Kit, user library, cache, or arbitrary development output."),
    ("WIN-01", "tools/launchers/run-native.ps1", "$start.ArgumentList.Add",
     "Root launcher and demo/lighting launchers resolve relative to PSScriptRoot. A complete production package is preferred; otherwise a real Release executable plus configured Windows Qt DLL is required. Child-only environment removes inherited QML/plugin overrides, adding the selected Kit only for development. PowerShell 7 ArgumentList preserves empty/space/Unicode arguments; Process.WaitForExit and its ExitCode avoid stale LASTEXITCODE from a WIN32 GUI. No web service/build/download is started."),
    ("PERF-01", "tools/deploy/windows.ps1", "$inventory = foreach",
     "Deployment is an explicit offline operation, O(total bytes) copy/hash plus one bounded native dependency inspection per package PE, with O(file/import count) metadata. It does not run in the application or per frame. Runtime work remains <=432 array entities plus bounded owned slots, with no per-library icon or QML scan."),
    ("PERF-02", "src/services/CoverCache.cpp", "void CoverCache::pump",
     "No runtime wait, worker or queue is introduced. Existing cover IO/decode stays on <=3 workers with bounded pending/cache/upload state; GUI/render consume immutable snapshots. Blocking build/deploy/hash/process waits occur only in explicit developer scripts. Verification drains child stdout and stderr asynchronously before waiting to avoid pipe deadlock."),
    ("PERF-03", "src/app/main.cpp", "QIcon applicationIcon",
     "The application builds one QIcon at process initialization from fixed qrc PNG variants, and publishes it once before any QML window is created. No image copying or icon regeneration is added to motion/selection frames. Settings SVG is an ordinary Qt image keyed by source/physical size; no per-frame readback or scene upload is introduced."),
    ("PERF-04", "src/render/rhi/ResourcePlan.h", "navigation-ruler-depth-upper-bound",
     "Product attachment formats, MSAA/resolve, mips, uploads, Canvas transparent color/depth bounds and resize overlap retain the existing ResourcePlan. Package scripts add no render pass, shader variant, atlas or texture pool. The small settings icon uses Qt image/cache ownership; Qt internal/cache/driver bytes remain distinct from resource descriptor estimates. Windows dynamic peak/format support remains required."),
    ("PERF-05", "qml/Yalahoo/Music/panels/SettingsPanel.qml", "settingsBrandIcon",
     "Attribution icon binds sourceSize to the screen DPR and constant logical 48px; a DPR change invalidates that Qt image cache, not unrelated scene resources. Native window QIcon chooses an existing size. Shader algorithms, source camera/material/lighting/motion constants and text cache keys are untouched, so existing component results remain valid unless affected by the resource/attribution addition."),
    ("PERF-06", "qml/Yalahoo/Music/panels/SettingsPanel.qml", "Accessible.ignored: true",
     "One noninteractive decorative image is added within the existing virtual/scroll settings attribution. It adds no timer, handler, focus, polling or hidden control input. Existing FrameClock/InputRouter/virtualized delegates remain authoritative; settings layout/scroll and QML import checks cover this affected host."),
    ("PERF-07", "tools/deploy/windows.ps1", "$backup = Join-Path",
     "All deployment work has owned staging and exact repository output paths. Configure/tool/dependency failures leave the existing package intact; commit-by-rename has restoration on failure and cleanup in finally. Caller environment is restored after windeployqt; launcher/clean verifier change only child environments. Qt owns application QIcon and QML image lifetimes; existing render epoch/resize/hide/shutdown ownership remains unchanged. Real Windows native lifecycle is deferred, never marked passed."),
]
DEPENDENCIES = {
    "CMakeLists.txt", "cmake/Cxx23.cmake", "cmake/QtKit.cmake", "cmake/Platform.cmake",
    "cmake/Deployment.cmake", "cmake/yalahoo.rc.in", "cmake/Assets.cmake",
    "cmake/BrowseQml.cmake", "src/app/CMakeLists.txt", "src/app/main.cpp",
    "src/app/GraphicsBackend.h", "src/app/AssetCatalog.cpp", "src/app/QmlTypes.h",
    "src/core/AppIdentity.cpp", "src/library/LibraryStore.cpp", "src/library/DirectoryScanner.cpp",
    "src/services/CoverCache.cpp", "src/media/PlaybackController.cpp",
    "src/media/BackgroundMusicController.cpp", "src/media/InterfaceSoundController.cpp",
    "src/render/CMakeLists.txt", "src/render/rhi/CMakeLists.txt", "src/render/rhi/ResourcePlan.h",
    "src/render/rhi/RhiItem.cpp", "src/render/rhi/SceneItem.cpp",
    "src/render/navigation/RulerInk.cpp", "src/render/opening/OpeningInk.cpp",
    "tools/deploy/windows.ps1", "tools/deploy/verify-windows.ps1", "tools/build/enter-msvc.ps1",
    "tools/launchers/run-native.ps1", "tools/launchers/demo.ps1", "tools/launchers/lighting-lab.ps1",
    "yalahoo.ps1", "qml/Yalahoo/Music/panels/SettingsPanel.qml", "resources/manifests/assets.cmake",
    "resources/manifests/branding.json", "resources/icons/yalahoo/yalahoo.ico",
    "resources/icons/yalahoo/yalahoo.svg", "docs/platforms/windows.md",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-directory", type=Path, default=ROOT / "build/release")
    parser.add_argument("--scan-qml", action="store_true", help="run this build's actual Kit import scanner")
    args = parser.parse_args()
    metadata = json.loads((args.build_directory / "deployment.json").read_text(encoding="utf-8"))
    qml_files = [ROOT / "qml" / path for path in metadata["qmlFiles"]]
    if any(not path.is_file() or not path.is_relative_to(ROOT / "qml") for path in qml_files):
        raise SystemExit("Production QML contains an invalid path")
    if len(set(qml_files)) != len(qml_files) or not qml_files:
        raise SystemExit("Invalid application QML closure")
    # Check generated metadata against the actual compiled resource list.
    qrc = args.build_directory / "src/app/.qt/rcc/Yalahoo_raw_qml_0.qrc"
    compiled = {Path(node.text).resolve() for node in ET.parse(qrc).iter("file")
                if node.text.endswith(".qml")}
    expected = set(qml_files)
    if not expected <= compiled or (not metadata["review"] and expected != compiled):
        raise SystemExit("Deployment QML diverges from actual compiled application resources")
    source_paths = DEPENDENCIES | {str(path.relative_to(ROOT)) for path in qml_files}
    source_paths |= {str(path.relative_to(ROOT)) for path in
                     (ROOT / "resources/icons/yalahoo/png").glob("*.png")}
    hashes = {path: hashlib.sha256((ROOT / path).read_bytes()).hexdigest() for path in sorted(source_paths)}
    scan_evidence = None
    if args.scan_qml:
        kit = Path(metadata["qtKit"])
        scanner = next((path for folder in ("libexec", "bin") for name in
                        ("qmlimportscanner", "qmlimportscanner.exe")
                        if (path := kit / folder / name).is_file()), None)
        if scanner is None:
            raise SystemExit("Selected Kit import scanner is missing")
        with tempfile.TemporaryDirectory(prefix="yalahoo-production-qml-") as directory:
            for source in qml_files:
                destination = Path(directory) / source.relative_to(ROOT / "qml")
                destination.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(source, destination)
            scan = subprocess.run([str(scanner), "-rootPath", directory,
                                   "-importPath", str(kit / "qml")],
                                  text=True, encoding="utf-8", capture_output=True, check=True)
            imports = json.loads(scan.stdout)
        modules = sorted({entry["name"] for entry in imports if entry.get("type") == "module"})
        if any(name.startswith(("QtQuick3D", "QtCanvas2D")) for name in modules):
            raise SystemExit("Research-only QML entered the production deployment closure")
        if not {"QtQuick", "QtQuick.Templates", "QtQuick.Dialogs", "QtMultimedia"} <= set(modules):
            raise SystemExit("Production scanner did not discover the required imports")
        scan_evidence = dict(qtVersion=metadata["qtVersion"], system=metadata["system"],
                             modules=modules, scannerSha256=hashlib.sha256(scanner.read_bytes()).hexdigest(),
                             stderr=scan.stderr, scope="host Kit production-QML scan; no target GPU or PE claim")
    for platform in ("windows", "macos", "linux"):
        checks = []
        for check_id, path, symbol, evidence in CHECKS:
            source = (ROOT / path).read_text(encoding="utf-8")
            if symbol not in source:
                raise SystemExit(f"Review binding changed: {path}::{symbol}")
            checks.append(dict(checkId=check_id, requirement=evidence, platform=platform,
                               sourcePath=path, lineOrSymbol=symbol,
                               line=source[:source.index(symbol)].count("\n") + 1,
                               evidence=evidence, result="source-reviewed", sourceSha256=hashes[path],
                               limitations="Windows PE/build/deploy/device/presentation: R30-P (R36); OpenGL: R36. "
                               + ("This platform is outside the Windows packaging branch; shared resource/startup impact only."
                                  if platform != "windows" else "No native Windows execution on this host inferred.")))
        output = ROOT / "verification" / f"{platform}-static" / "windows-deployment"
        output.mkdir(parents=True, exist_ok=True)
        (output / "source-matrix.json").write_text(json.dumps(dict(checks=checks, sourceSha256=hashes,
                                                                 qmlScan=scan_evidence), indent=2) + "\n", encoding="utf-8")
    print("R30 source bindings and compiled production QML closure verified; Windows execution is independent")


if __name__ == "__main__":
    main()
