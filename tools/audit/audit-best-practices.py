"""Run compiler-backed native safety checks; architecture needs manual review."""
import argparse
import concurrent.futures
import hashlib
import json
from pathlib import Path
import re
import shutil
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[2]
CHECKS = ",".join((
    "-*", "clang-analyzer-core.*", "clang-analyzer-cplusplus.*",
    "clang-analyzer-deadcode.*", "clang-analyzer-unix.*", "bugprone-dangling-handle",
    "bugprone-use-after-move", "bugprone-infinite-loop",
    "bugprone-undefined-memory-manipulation", "bugprone-sizeof-expression",
))


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-directory", type=Path, default=ROOT / "build/release")
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--clang-tidy", type=Path)
    parser.add_argument("--jobs", type=int, default=3)
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    output = args.output.resolve()
    output.mkdir(parents=True, exist_ok=False)
    build = args.build_directory.resolve()
    commands = json.loads((build / "compile_commands.json").read_text())
    cache = (build / "CMakeCache.txt").read_text()
    if not re.search(r"^CMAKE_BUILD_TYPE:STRING=Release$", cache, re.MULTILINE):
        parser.error("Use the configured Release build")
    tidy = args.clang_tidy or shutil.which("clang-tidy")
    if not tidy:
        compiler = re.search(r"^YALAHOO_CLANG_FORMAT:FILEPATH=(.+)$", cache, re.MULTILINE)
        if compiler:
            candidate = Path(compiler[1]).parent / "clang-tidy"
            if candidate.is_file():
                tidy = candidate
    if not tidy:
        parser.error("Install clang-tidy or pass --clang-tidy")
    sdk_args = []
    if sys.platform == "darwin":
        sdk = subprocess.check_output(["xcrun", "--show-sdk-path"], text=True).strip()
        sdk_args = ["--extra-arg=-isysroot" + sdk]
    sources = []
    for entry in commands:
        path = Path(entry["file"])
        if not path.is_absolute():
            path = Path(entry["directory"]) / path
        path = path.resolve()
        if path.is_relative_to(ROOT) and path.parts[len(ROOT.parts)] in ("src", "tests", "tools"):
            if path.suffix in (".cpp", ".mm"):
                sources.append(path)
    sources = sorted(set(sources))
    if not sources:
        parser.error("Compile database has no native source units")

    def analyze(path):
        relative = str(path.relative_to(ROOT))
        before = digest(path)
        log = relative.replace("/", "__") + ".log"
        with (output / log).open("w") as stream:
            result = subprocess.run(
                [str(tidy), "-p", str(build), str(path), "--checks=" + CHECKS, *sdk_args],
                cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT, check=False,
            )
        text = (output / log).read_text()
        diagnostics = [line for line in text.splitlines() if re.search(r": (warning|error):", line)]
        stable = before == digest(path)
        return {"sourcePath": relative, "sha256": before, "exitCode": result.returncode,
                "sourceStable": stable, "diagnostics": diagnostics, "log": log,
                "result": "clean" if stable and result.returncode == 0 and not diagnostics else "review-required"}

    with concurrent.futures.ThreadPoolExecutor(max_workers=args.jobs) as executor:
        units = list(executor.map(analyze, sources))
    clean = all(unit["result"] == "clean" for unit in units)
    report = {
        "compilerChecksClean": clean, "checks": CHECKS,
        "tool": subprocess.check_output([str(tidy), "--version"], text=True).strip(),
        "toolchain": json.loads((build / "toolchain.json").read_text()),
        "compileDatabaseSha256": digest(build / "compile_commands.json"),
        "translationUnits": units,
        "limitations": [
            "Automatic safety analysis is not an architecture or best-practices compliance proof.",
            "QML logic sites need manual classification using qml-best-practices.md.",
            "Only branches compiled by this target are analyzed; target builds/runtime remain separate.",
            "Headers are analyzed through including units; uninstantiated templates may not be exercised.",
        ],
    }
    (output / "report.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n")
    print(json.dumps({"compilerChecksClean": clean, "units": len(units),
                      "report": str(output / "report.json")}))
    return 0 if clean else 1


if __name__ == "__main__":
    sys.exit(main())
