#!/usr/bin/env python3
"""Separate product diagnostics, resource stress and formal presentation sampling."""
from native_replay import BACKEND_IDS, cmake_cache, executable_path, replay_environment
import argparse
import hashlib
import json
from pathlib import Path
import platform
import subprocess

ROOT = Path(__file__).resolve().parents[2]
SCENARIOS = [f"{theme}-{action}" for theme in ("day", "night")
             for action in ("wave", "ruler", "detail", "switch", "overview", "search")]


def parse_arguments(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--backend", required=True, choices=BACKEND_IDS)
    parser.add_argument("--mode", choices=("offscreen", "vsync", "uncapped"), default="offscreen")
    parser.add_argument("--scenario", choices=["matrix", *SCENARIOS])
    scope = parser.add_mutually_exclusive_group()
    scope.add_argument("--formal", action="store_true", help="One 120-second normal VSYNC acceptance sample")
    scope.add_argument("--stress", action="store_true", help="One resource stress replay without timed sampling")
    scope.add_argument("--observe-only", action="store_true",
                       help="Observe one native wave scenario without functional replay or large input generation")
    parser.add_argument("--seconds", type=int)
    parser.add_argument("--runs", type=int, choices=(1, 3),
                        help="One run by default; three only for an explicit comparison or fixed recheck")
    parser.add_argument("--dpr", type=float, choices=(1, 1.5, 2), default=1)
    parser.add_argument("--foreground", action="store_true")
    parser.add_argument("--lifecycle", action="store_true", help="Add native hide/minimize to --stress")
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args(argv)
    if args.seconds is None:
        args.seconds = 0 if args.stress else 120 if args.formal else 30
    if args.runs is None:
        args.runs = 1
    if args.stress:
        if args.seconds != 0 or args.runs != 1:
            parser.error("Resource stress runs once without timed sampling (--seconds 0 --runs 1)")
    elif not 12 <= args.seconds <= 120:
        parser.error("seconds must be 12..120")
    if args.formal and (args.seconds != 120 or args.mode != "vsync" or args.dpr != 1):
        parser.error("Formal acceptance requires normal VSYNC, DPR 1 and 120 seconds")
    if args.scenario is None:
        if args.mode == "offscreen" or args.stress:
            args.scenario = "matrix"
        else:
            parser.error("Native sampling requires --scenario with one fixed day/night scenario")
    if not args.stress and args.mode != "offscreen" and args.scenario == "matrix":
        parser.error("Choose one fixed scenario for native sampling")
    if args.observe_only and (args.mode == "offscreen" or args.scenario not in ("day-wave", "night-wave")
                              or args.runs != 1):
        parser.error("Observation requires one native day-wave/night-wave run")
    if args.mode == "offscreen":
        if platform.system() != "Darwin":
            parser.error("The genuine hidden RenderControl entry is currently implemented on macOS only")
        if args.foreground or args.lifecycle:
            parser.error("Offscreen runs have no foreground or native hide/minimize lifecycle")
    if args.lifecycle and not args.stress:
        parser.error("Native hide/minimize lifecycle requires a separate --stress run")
    if args.output.exists():
        parser.error("Choose a fresh output directory; preserve failed and raw runs")
    return args


def validate_report(report, args, exit_code):
    expected_switches, expected_rebuilds = (0, 0) if args.observe_only else (30, 10) if args.stress else (3, 2)
    valid = (exit_code == 0 and report.get("functionalPassed") is True
             and report.get("samplingComplete") is True
             and report.get("backend") == BACKEND_IDS[args.backend]
             and report.get("dpr") == args.dpr
             and report.get("scenario") == args.scenario
             and report.get("stressRequested") is args.stress
             and report.get("samplingRequestedSeconds") == args.seconds
             and report.get("observationOnly", False) is args.observe_only
             and report.get("switches") == expected_switches
             and report.get("rebuilds") == expected_rebuilds)
    if args.seconds:
        valid = (valid and report.get("samplingSeconds", 0) >= args.seconds
                 and bool(report.get("stages")))
    if args.mode != "offscreen":
        valid = (valid and report.get("swapIntervalRequested") == (0 if args.mode == "uncapped" else 1)
                 and report.get("qsgNoVsyncRequested") is (args.mode == "uncapped"))
        if args.seconds:
            valid = valid and report.get("nativeSamplingValid") is True
    if args.lifecycle:
        valid = valid and report.get("nativeWindowTransitions") == 4
    if args.formal:
        valid = (valid and report.get("performanceGateEnforced") is True
                 and report.get("performanceGatePassed") is True)
    if args.observe_only:
        start, end = report.get("samplingStartedNs", 0), report.get("samplingEndedNs", 0)
        streams = [[frame.get("atNs", 0) for frame in report.get("guiFrames", [])],
                   report.get("synchronizationTimestampsNs", []),
                   report.get("qtPresentationTimestampsNs", [])]
        during_sampling = [[stamp for stamp in stream if start <= stamp <= end] for stream in streams]
        valid = (valid and report.get("windowPolicy", {}).get("passed") is True
                 and report.get("performanceGateEnforced") is False
                 and report.get("continuouslyExposedDuringSampling") is True
                 and start > 0 and end > start
                 and all(len(stream) >= 2 and all(a < b for a, b in zip(stream, stream[1:]))
                         for stream in during_sampling))
    return bool(valid)


def main(argv=None):
    args = parse_arguments(argv)
    cache = cmake_cache(ROOT)
    kit = Path(cache["YALAHOO_SELECTED_QT:PATH"])
    executable = executable_path(ROOT, tool="ui-audit")
    if cache.get("CMAKE_BUILD_TYPE:STRING") != "Release" or not executable.is_file():
        raise SystemExit("Build the latest native Kit qt-release ui-audit first")
    args.output.mkdir(parents=True)
    environment = replay_environment(kit, args.backend, args.dpr, args.foreground)
    environment.update(YALAHOO_VERIFY_VISIBLE="0" if args.mode == "offscreen" else "1",
                       YALAHOO_VERIFY_LIFECYCLE="1" if args.lifecycle else "0",
                       YALAHOO_VERIFY_STRESS="1" if args.stress else "0")
    if args.mode == "uncapped":
        environment["QSG_NO_VSYNC"] = "1"
    source_files = [*ROOT.glob("src/**/*.cpp"), *ROOT.glob("src/**/*.h"), *ROOT.glob("qml/**/*.qml"),
                    *ROOT.glob("resources/shaders/**/*"), ROOT / "tools/review/StartupAudit.cpp",
                    ROOT / "tools/review/OffscreenSurface.h", Path(__file__).resolve()]
    manifest = dict(system=platform.platform(), compiler=cache.get("CMAKE_CXX_COMPILER:FILEPATH"),
                    binarySha256=hashlib.sha256(executable.read_bytes()).hexdigest(),
                    sourceSha256={str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                                  for p in sorted(source_files) if p.is_file()},
                    fixture=("StartupAudit::fixture: 576 albums with embedded qrc covers, no audio or generated large covers"
                             if args.observe_only else
                             "StartupAudit::fixture: generated 576 albums/96 distinct 2048-square JPEGs; test::wave(600000), no private library"),
                    backend=args.backend, dpr=args.dpr, mode=args.mode, scenario=args.scenario, seconds=args.seconds,
                    formalRequested=args.formal, stressRequested=args.stress, observationOnly=args.observe_only,
                    formalAcceptancePassed=None,
                    foreground=args.foreground, lifecycle=args.lifecycle, requestedRuns=args.runs,
                    runs=[], completed=False, passed=False)
    manifest_path = args.output / "manifest.json"

    def save():
        manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    save()
    passed = True
    for index in range(1, args.runs + 1):
        output = args.output / f"run-{index}"
        with (args.output / f"run-{index}.log").open("w", encoding="utf-8") as log:
            try:
                command = [str(executable), "--startup", str(output.resolve()), str(args.seconds), args.scenario]
                if args.observe_only:
                    command.append("observe")
                result = subprocess.run(command,
                                        cwd=ROOT, env=environment, stdout=log, stderr=subprocess.STDOUT,
                                        timeout=1200, check=False)
                code = result.returncode
            except subprocess.TimeoutExpired:
                code = "timeout"
        report_path = output / "report.json"
        try:
            report = json.loads(report_path.read_text(encoding="utf-8")) if report_path.is_file() else {}
            valid = isinstance(report, dict) and validate_report(report, args, code)
        except (ValueError, TypeError):
            report = {}
            valid = False
        manifest["runs"].append(dict(index=index, exitCode=code, passed=valid, report=str(report_path),
                                      scope=report.get("measurementScope") if isinstance(report, dict) else None))
        passed = passed and valid
        save()
        print(f"{args.backend} {args.mode} run {index}: {'passed' if valid else 'FAILED'}", flush=True)
        # A declared formal/recheck batch retains every run, including failures.
    manifest["completed"] = len(manifest["runs"]) == args.runs
    manifest["passed"] = passed and manifest["completed"]
    if args.formal:
        manifest["formalAcceptancePassed"] = manifest["passed"]
    save()
    return 0 if manifest["passed"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
