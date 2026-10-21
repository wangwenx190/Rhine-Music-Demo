"""Common Release replay transport; component validators own their assertions."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess

ROOT = Path(__file__).resolve().parents[2]
BACKEND_IDS = {"metal": 6, "d3d11": 4, "d3d12": 8, "opengl": 3, "vulkan": 5}
QML_ERRORS = ("Binding loop", "Cannot assign", "ReferenceError", "TypeError", "Failed to compile")
MAXIMUM_RUNS = 10
REPLAY_TIMEOUT_SECONDS = 150


def write_json(path, value):
    path.write_text(json.dumps(value, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")


def executable_path(root=ROOT, tool=None):
    if tool:
        return root / "build/release/tools" / (tool + (".exe" if platform.system() == "Windows" else ""))
    relative = {"Darwin": "yalahoo.app/Contents/MacOS/yalahoo", "Windows": "yalahoo.exe"}
    return root / "build/release/bin" / relative.get(platform.system(), "yalahoo")


def replay_environment(kit, backend, dpr, foreground=False):
    environment = os.environ.copy()
    environment.update(QSG_RHI_BACKEND=backend, QT_SCALE_FACTOR=str(dpr),
                       QT_PLUGIN_PATH=str(kit / "plugins"),
                       YALAHOO_VERIFY_FOREGROUND="1" if foreground else "0",
                       QT_MAC_DISABLE_FOREGROUND_APPLICATION_TRANSFORM="1",
                       QSG_RENDER_LOOP="basic" if platform.system() == "Darwin" and backend == "opengl" else "threaded")
    environment.pop("QSG_NO_VSYNC", None)
    if platform.system() == "Windows":
        environment["PATH"] = str(kit / "bin") + os.pathsep + environment.get("PATH", "")
    return environment


def replay_parser():
    parser = argparse.ArgumentParser()
    parser.add_argument("--backend", choices=BACKEND_IDS, required=True)
    parser.add_argument("--dpr", type=float, choices=(1, 1.5, 2), default=1)
    parser.add_argument("--runs", type=int, default=1)
    parser.add_argument("--foreground", action="store_true")
    parser.add_argument("--lifecycle", action="store_true", help="Include lifecycle stress and repeated window rebuilds")
    parser.add_argument("--night", action="store_true")
    parser.add_argument("--output", type=Path, required=True)
    return parser


def run_replays(parser, args, command, validate, **scope):
    if not 1 <= args.runs <= MAXIMUM_RUNS:
        parser.error(f"runs must be between 1 and {MAXIMUM_RUNS}")
    if args.output.exists():
        parser.error("output already exists; preserve prior results and choose a fresh directory")
    visual = getattr(args, "visual", False)
    executable = executable_path(tool="ui-audit" if visual else None)
    if not executable.is_file():
        parser.error("Build the qt-release preset first")
    chain = json.loads((ROOT / "build/release/toolchain.json").read_text(encoding="utf-8"))
    if chain.get("configuration") != "Release":
        parser.error("The native replay requires a Release build")
    args.output.mkdir(parents=True)
    environment = replay_environment(Path(chain["qtKit"]), args.backend, args.dpr, args.foreground)
    environment.update(YALAHOO_VERIFY_NIGHT="1" if args.night else "0",
                       YALAHOO_VERIFY_LIFECYCLE="1" if args.lifecycle else "0",
                       YALAHOO_VERIFY_WINDOW_LIFECYCLE="1" if getattr(args, "window_lifecycle", False) else "0")
    if visual:
        environment["YALAHOO_VERIFY_VISIBLE"] = "0"
    manifest = dict(binarySha256=hashlib.sha256(executable.read_bytes()).hexdigest(),
                    sourceRevision=subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
                    workingTreeDiffSha256=hashlib.sha256(subprocess.check_output(["git", "diff"], cwd=ROOT)).hexdigest(),
                    system=platform.platform(), backend=args.backend, dpr=args.dpr,
                    lifecycleStress=args.lifecycle, foreground=args.foreground, night=args.night,
                    requestedRuns=args.runs, runs=[], completed=False, passed=False, **scope)
    manifest_path = args.output / "manifest.json"
    write_json(manifest_path, manifest)
    passed = True
    for index in range(1, args.runs + 1):
        output = args.output / f"run-{index}"
        log_path = args.output / f"run-{index}.log"
        with log_path.open("w", encoding="utf-8") as log:
            try:
                code = subprocess.run([str(executable), command, str(output.resolve())], cwd=ROOT,
                                      env=environment, stdout=log, stderr=subprocess.STDOUT,
                                      timeout=REPLAY_TIMEOUT_SECONDS, check=False).returncode
            except subprocess.TimeoutExpired:
                code = "timeout"
        report_path = output / "report.json"
        try:
            report = json.loads(report_path.read_text(encoding="utf-8")) if report_path.is_file() else {}
            valid = bool(isinstance(report, dict) and code == 0 and report.get("passed")
                         and report.get("backend") == args.backend and validate(report, args)
                         and not any(token in log_path.read_text(encoding="utf-8") for token in QML_ERRORS))
        except (ValueError, TypeError, KeyError):
            valid = False
        passed = valid and passed
        manifest["runs"].append(dict(run=index, exitCode=code, passed=valid, report=str(report_path)))
        print(f"{args.backend} DPR{args.dpr:g} run {index}: {'passed' if valid else 'FAILED'}", flush=True)
        write_json(manifest_path, manifest)
        if not valid:
            break
    manifest["completed"] = len(manifest["runs"]) == args.runs
    manifest["passed"] = passed and manifest["completed"]
    write_json(manifest_path, manifest)
    return 0 if manifest["passed"] else 1
