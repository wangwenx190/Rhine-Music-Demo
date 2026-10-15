#!/usr/bin/env python3
"""Run serialized Release product overview replays; keep raw evidence outside Git."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess

root = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser()
parser.add_argument("--backend", choices=("metal", "opengl", "d3d11"), required=True)
parser.add_argument("--dpr", type=float, choices=(1, 1.5, 2), default=1)
parser.add_argument("--runs", type=int, default=1)
parser.add_argument("--foreground", action="store_true")
parser.add_argument("--lifecycle", action="store_true", help="Include lifecycle stress and repeated window rebuilds")
parser.add_argument("--visual", action="store_true", help="Run the isolated QML/resource replay")
parser.add_argument("--night", action="store_true")
parser.add_argument("--output", type=Path, required=True)
args = parser.parse_args()
if not 1 <= args.runs <= 10:
    parser.error("runs must be between 1 and 10")
if args.output.exists():
    parser.error("output already exists; preserve prior results and choose a fresh directory")
executable = root / "build/release/bin"
if platform.system() == "Darwin":
    executable /= "yalahoo.app/Contents/MacOS/yalahoo"
elif platform.system() == "Windows":
    executable /= "yalahoo.exe"
else:
    executable /= "yalahoo"
if not executable.is_file():
    parser.error("Build the qt-release preset first")
if args.visual:
    executable = root / "build/release/tools" / ("ui-audit.exe" if platform.system() == "Windows" else "ui-audit")
args.output.mkdir(parents=True)
environment = os.environ.copy()
environment.update({
    "QSG_RHI_BACKEND": args.backend,
    "QT_SCALE_FACTOR": str(args.dpr),
    "YALAHOO_VERIFY_FOREGROUND": "1" if args.foreground else "0",
    "YALAHOO_VERIFY_NIGHT": "1" if args.night else "0",
    "YALAHOO_VERIFY_LIFECYCLE": "1" if args.lifecycle else "0",
    "QSG_NO_VSYNC": "",
    "YALAHOO_VERIFY_VISIBLE": "0" if args.visual else os.environ.get("YALAHOO_VERIFY_VISIBLE", "0"),
    "QSG_RENDER_LOOP": "threaded",
})
manifest = {
    "binarySha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
    "sourceRevision": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip(),
    "workingTreeDiffSha256": hashlib.sha256(subprocess.check_output(["git", "diff"], cwd=root)).hexdigest(),
    "system": platform.platform(), "backend": args.backend, "dpr": args.dpr,
    "lifecycleStress": args.lifecycle, "foreground": args.foreground, "visual": args.visual, "night": args.night, "requestedRuns": args.runs, "runs": [],
}
manifest["completed"] = False
manifest["passed"] = False
(args.output / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n")
passed = True
for index in range(1, args.runs + 1):
    output = args.output / f"run-{index}"
    with (args.output / f"run-{index}.log").open("w") as log:
        result = subprocess.run([str(executable), "--overview" if args.visual else "--verify-overview", str(output.resolve())],
                                cwd=root, env=environment, stdout=log, stderr=subprocess.STDOUT,
                                timeout=150, check=False)
    report_path = output / "report.json"
    report = json.loads(report_path.read_text()) if report_path.is_file() else {}
    log = (args.output / f"run-{index}.log").read_text()
    bad_log = any(token in log for token in ("Binding loop", "Cannot assign", "ReferenceError", "TypeError", "Failed to compile"))
    if args.visual:
        captures = report.get("captures", [])
        lifecycle = report.get("lifecycle", [])
        context_valid = (len(captures) == 36 and all(c["dpr"] == args.dpr for c in captures)
                         and len(lifecycle) == (10 if args.lifecycle else 0) and all(lifecycle))
    else:
        context_valid = report.get("dpr") == args.dpr and report.get("cycles") == (10 if args.lifecycle else 0)
    valid = (result.returncode == 0 and report.get("passed") and not bad_log
             and report.get("backend") == args.backend and context_valid)
    passed = bool(valid) and passed
    manifest["runs"].append({"run": index, "exitCode": result.returncode,
                             "passed": bool(valid), "report": str(report_path)})
    print(f"{args.backend} DPR{args.dpr:g} run {index}: {'passed' if valid else 'FAILED'}", flush=True)
    manifest["passed"] = False
    (args.output / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n")
    if not valid:
        break
manifest["completed"] = len(manifest["runs"]) == args.runs
passed = passed and manifest["completed"]
manifest["passed"] = passed
(args.output / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n")
raise SystemExit(0 if passed else 1)
