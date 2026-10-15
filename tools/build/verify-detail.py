#!/usr/bin/env python3
"""Run serialized Release product detail replays; keep raw evidence outside Git."""
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
args.output.mkdir(parents=True)
environment = os.environ.copy()
environment.update({
    "QSG_RHI_BACKEND": args.backend,
    "QT_SCALE_FACTOR": str(args.dpr),
    "YALAHOO_VERIFY_FOREGROUND": "1" if args.foreground else "0",
    "YALAHOO_VERIFY_NIGHT": "1" if args.night else "0",
    "YALAHOO_VERIFY_LIFECYCLE": "1" if args.lifecycle else "0",
    "QSG_NO_VSYNC": "",
    "QSG_RENDER_LOOP": "threaded",
})
manifest = {
    "binarySha256": hashlib.sha256(executable.read_bytes()).hexdigest(),
    "sourceRevision": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=root, text=True).strip(),
    "workingTreeDiffSha256": hashlib.sha256(subprocess.check_output(["git", "diff"], cwd=root)).hexdigest(),
    "system": platform.platform(), "backend": args.backend, "dpr": args.dpr,
    "lifecycleStress": args.lifecycle, "foreground": args.foreground, "night": args.night, "requestedRuns": args.runs, "runs": [],
}
manifest["completed"] = False
manifest["passed"] = False
(args.output / "manifest.json").write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n")
passed = True
for index in range(1, args.runs + 1):
    output = args.output / f"run-{index}"
    with (args.output / f"run-{index}.log").open("w") as log:
        result = subprocess.run([str(executable), "--verify-detail", str(output.resolve())],
                                cwd=root, env=environment, stdout=log, stderr=subprocess.STDOUT,
                                timeout=150, check=False)
    report_path = output / "report.json"
    report = json.loads(report_path.read_text()) if report_path.is_file() else {}
    log = (args.output / f"run-{index}.log").read_text()
    bad_log = any(token in log for token in ("Binding loop", "Cannot assign", "ReferenceError", "TypeError", "Failed to compile"))
    valid = (result.returncode == 0 and report.get("passed") and not bad_log
             and report.get("backend") == args.backend and report.get("dpr") == args.dpr
             and report.get("lifecycleStressRequested") == args.lifecycle
             and report.get("fullWindowRebuilds") == (10 if args.lifecycle else 0))
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
