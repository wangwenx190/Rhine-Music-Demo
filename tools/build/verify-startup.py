#!/usr/bin/env python3
"""Serialized R28 product lifecycle/local-cost and native presentation sampling."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument("--backend", required=True, choices=("metal", "d3d11", "opengl", "vulkan"))
parser.add_argument("--mode", choices=("offscreen", "vsync", "uncapped"), default="offscreen")
scenarios = ["day-wave", "day-ruler", "day-detail", "day-switch", "day-overview", "day-search",
             "night-wave", "night-ruler", "night-detail", "night-switch", "night-overview", "night-search"]
parser.add_argument("--scenario", choices=["matrix", *scenarios])
parser.add_argument("--seconds", type=int, default=120)
parser.add_argument("--runs", type=int, default=3)
parser.add_argument("--dpr", type=float, choices=(1, 1.5, 2), default=1)
parser.add_argument("--foreground", action="store_true")
parser.add_argument("--lifecycle", action="store_true")
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
if not 12 <= args.seconds <= 120 or not 1 <= args.runs <= 10:
    parser.error("seconds must be 12..120 and runs 1..10; formal sampling requires 120 seconds and >=3 runs")
if args.scenario is None:
    if args.mode == "offscreen":
        args.scenario = "matrix"
    else:
        parser.error("Native formal sampling requires --scenario with one fixed day/night scenario")
if args.mode != "offscreen" and args.scenario == "matrix":
    parser.error("Choose a fixed scenario for native formal sampling; the matrix is a local diagnostic")
if args.mode == "offscreen" and platform.system() != "Darwin":
    parser.error("The current genuine hidden RenderControl entry is implemented on macOS only")
if args.mode == "offscreen" and args.foreground:
    parser.error("Offscreen sampling has no foreground window")
if args.mode == "offscreen" and args.lifecycle:
    parser.error("Native hide/minimize lifecycle requires a native mode")
if args.output.exists():
    parser.error("Choose a fresh output directory; failed and raw runs must be retained")
cache_path = root / "build/release/CMakeCache.txt"
cache = dict(line.split("=", 1) for line in cache_path.read_text().splitlines()
             if "=" in line and not line.startswith(("#", "//")))
kit = Path(cache["YALAHOO_SELECTED_QT:PATH"])
executable = root / "build/release/tools" / ("ui-audit.exe" if platform.system() == "Windows" else "ui-audit")
if cache.get("CMAKE_BUILD_TYPE:STRING") != "Release" or not executable.is_file():
    parser.error("Build the latest native Kit qt-release ui-audit first")
args.output.mkdir(parents=True)
environment = os.environ.copy()
environment.update({"QSG_RHI_BACKEND": args.backend, "QT_SCALE_FACTOR": str(args.dpr),
                    "QT_PLUGIN_PATH": str(kit / "plugins"),
                    "YALAHOO_VERIFY_VISIBLE": "0" if args.mode == "offscreen" else "1",
                    "YALAHOO_VERIFY_FOREGROUND": "1" if args.foreground else "0",
                    "YALAHOO_VERIFY_LIFECYCLE": "1" if args.lifecycle else "0",
                    "QT_MAC_DISABLE_FOREGROUND_APPLICATION_TRANSFORM": "1",
                    "QSG_RENDER_LOOP": "basic" if platform.system() == "Darwin" and args.backend == "opengl" else "threaded",
                    "QCPAINTER_DEBUG_COLLECT": "0"})
if args.mode == "uncapped":
    environment["QSG_NO_VSYNC"] = "1"
else:
    environment.pop("QSG_NO_VSYNC", None)
if platform.system() == "Windows":
    environment["PATH"] = str(kit / "bin") + os.pathsep + environment.get("PATH", "")
source_files = [*root.glob("src/**/*.cpp"), *root.glob("src/**/*.h"), *root.glob("qml/**/*.qml"),
                *root.glob("resources/shaders/*"), root / "tools/review/StartupAudit.cpp",
                root / "tools/review/OffscreenSurface.h", Path(__file__).resolve()]
manifest = dict(system=platform.platform(), compiler=cache.get("CMAKE_CXX_COMPILER:FILEPATH"),
                binarySha256=hashlib.sha256(executable.read_bytes()).hexdigest(),
                sourceSha256={str(p.relative_to(root)): hashlib.sha256(p.read_bytes()).hexdigest()
                              for p in sorted(source_files) if p.is_file()},
                fixture="StartupAudit::fixture: generated 576 albums/96 distinct 2048-square JPEGs; test::wave(600000), no private library",
                backend=args.backend, dpr=args.dpr, mode=args.mode, scenario=args.scenario, seconds=args.seconds,
                foreground=args.foreground, lifecycle=args.lifecycle, requestedRuns=args.runs, runs=[], completed=False, passed=False)
manifest_path = args.output / "manifest.json"
def save():
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n")
save()
backend_ids = {"metal": 6, "d3d11": 4, "opengl": 3, "vulkan": 5}
passed = True
for index in range(1, args.runs + 1):
    output = args.output / f"run-{index}"
    with (args.output / f"run-{index}.log").open("w") as log:
        try:
            result = subprocess.run([str(executable), "--startup", str(output.resolve()), str(args.seconds), args.scenario],
                                    cwd=root, env=environment, stdout=log, stderr=subprocess.STDOUT,
                                    timeout=1200, check=False)
            code = result.returncode
        except subprocess.TimeoutExpired:
            code = "timeout"
    report_path = output / "report.json"
    report = json.loads(report_path.read_text()) if report_path.is_file() else {}
    valid = (code == 0 and report.get("functionalPassed") and report.get("samplingComplete")
             and report.get("backend") == backend_ids[args.backend] and report.get("dpr") == args.dpr
             and report.get("switches") == 30 and report.get("rebuilds") == 10)
    if args.mode != "offscreen":
        valid = (valid and report.get("nativeSamplingValid")
                 and report.get("swapIntervalRequested") == (0 if args.mode == "uncapped" else 1)
                 and report.get("qsgNoVsyncRequested") == (args.mode == "uncapped"))
        if args.lifecycle:
            valid = valid and report.get("nativeWindowTransitions") == 4
        if args.mode == "vsync" and args.dpr == 1:
            valid = valid and report.get("performanceGatePassed")
    manifest["runs"].append(dict(index=index, exitCode=code, passed=bool(valid), report=str(report_path),
                                  scope=report.get("measurementScope")))
    passed = passed and bool(valid)
    save()
    print(f"{args.backend} {args.mode} run {index}: {'passed' if valid else 'FAILED'}", flush=True)
    # Preserve and complete the declared repeat batch, including failed performance samples.
manifest["completed"] = len(manifest["runs"]) == args.runs
manifest["passed"] = passed and manifest["completed"]
save()
raise SystemExit(0 if manifest["passed"] else 1)
