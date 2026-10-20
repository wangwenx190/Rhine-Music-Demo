"""Check material or lighting output using an already built Release Review target."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess


ROOT = Path(__file__).resolve().parents[2]


def validate_report(path, fixture, count):
    report = json.loads(path.read_text())
    cases = report.get("cases", [])
    if (not report.get("passed")
            or report.get("validation") != "source-state-and-own-output"
            or report.get("warnings") or len(cases) != count
            or report.get("fixtureSha256") != hashlib.sha256(fixture.read_bytes()).hexdigest()
            or report.get("maxProjectionPixels", float("inf")) > 2
            or not report.get("windowPolicy", {}).get("passed")):
        raise RuntimeError("Invalid or stale material/source-state report")
    for case in cases + report.get("restores", []):
        if (not case.get("passed") or not case.get("sourceParameterMatch")
                or case.get("webProjectionPixels", float("inf")) > 2
                or case.get("foregroundPixels", 0) <= 100 or case.get("frames", 0) < 4
                or case.get("paperProbeError", float("inf")) > 3
                or case.get("atlasCpuPayloadBytes") != 0
                or case.get("atlasSubmissions") != 0):
            raise RuntimeError(f"Invalid material state: {case.get('index')}")
        if not (path.parent / case["screenshot"]).is_file():
            raise RuntimeError("Missing own-output image")
    restores = report.get("restores", [])
    if len(restores) != (10 if count == 99 else 1) or any(
            not item.get("passed") or not item.get("sceneGraphRecreated")
            or not item.get("qmlRootRecreated") for item in restores):
        raise RuntimeError("Missing resource restore checks")
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("kind", choices=("materials", "lighting"))
    args = parser.parse_args()
    host = {"Darwin": "macos", "Windows": "windows", "Linux": "linux"}[platform.system()]
    output = ROOT / "verification" / host / "material-review" / args.kind
    output.mkdir(parents=True, exist_ok=True)
    cache = (ROOT / "build/release/CMakeCache.txt").read_text().splitlines()
    if "CMAKE_BUILD_TYPE:STRING=Release" not in cache or "YALAHOO_REVIEW:BOOL=ON" not in cache:
        raise RuntimeError("Prepare the Release Review build before running this component check")
    chain = json.loads((ROOT / "build/release/toolchain.json").read_text())
    environment = os.environ.copy()
    environment.update(YALAHOO_VERIFY_FOREGROUND="0", YALAHOO_VERIFY_VISIBLE="0",
                       YALAHOO_VERIFY_LIFECYCLE="0", YALAHOO_VERIFY_STRESS="0",
                       QT_PLUGIN_PATH=str(Path(chain["qtKit"]) / "plugins"))
    environment["PATH"] = str(Path(chain["qtKit"]) / "bin") + os.pathsep + environment["PATH"]

    def run(name, command):
        with (output / (name + ".log")).open("w") as log:
            subprocess.run(command, cwd=ROOT, env=environment, stdout=log,
                           stderr=subprocess.STDOUT, check=True)

    run("render-tests", ["ctest", "--preset", "qt-release-render" if host == "macos"
                         else "qt-release-visible", "--output-on-failure", "--no-tests=error", "-R",
                         f"^runtime-{args.kind}$"])
    fixture = ROOT / "tests/fixtures" / args.kind / (
        "single-case.json" if args.kind == "materials" else "visual.json")
    source = ROOT / "build/release/verification" / args.kind / "runtime.json"
    report = validate_report(source, fixture, 99 if args.kind == "materials" else 48)
    (output / "runtime.json").write_text(json.dumps(report, indent=2) + "\n")
    print(f"{args.kind}: source/state and own-output checks passed; logs: {output}")


if __name__ == "__main__":
    main()
