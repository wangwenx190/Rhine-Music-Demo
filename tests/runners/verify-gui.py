#!/usr/bin/env python3
"""Run one integrated GUI matrix and preserve its inputs, output and failure status."""
from native_replay import BACKEND_IDS, cmake_cache, executable_path, replay_environment
import argparse
import hashlib
import json
from pathlib import Path
import platform
import subprocess

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--backend', required=True, choices=BACKEND_IDS)
parser.add_argument('--narrow', action='store_true', help='Regress only viewports <=700 after a narrow-layout change')
parser.add_argument('--representative', action='store_true', help='Check product integration at desktop, portrait boundary and narrow sizes')
parser.add_argument('--visible', action='store_true', help='Use a native window on macOS')
parser.add_argument('--foreground', action='store_true', help="Use this round's authorized foreground policy")
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
if args.narrow and args.representative:
    parser.error('--narrow and --representative are mutually exclusive')
if args.foreground and not args.visible:
    parser.error('--foreground requires --visible')
if args.output.exists():
    parser.error('Choose a fresh output directory; preserve failed runs')
cache = cmake_cache(ROOT)
kit = Path(cache['YALAHOO_SELECTED_QT:PATH'])
executable = executable_path(ROOT, tool='ui-tests')
if cache.get('CMAKE_BUILD_TYPE:STRING') != 'Release' or not executable.is_file():
    parser.error('Build the latest native Kit Release ui-tests first')
args.output.mkdir(parents=True)
env = replay_environment(kit, args.backend, 1, args.foreground)
env['YALAHOO_VERIFY_VISIBLE'] = '1' if args.visible else '0'
files = [*ROOT.glob('src/**/*'), *ROOT.glob('qml/**/*.qml'), *ROOT.glob('resources/**/*'),
         ROOT / 'tests/render/StartupChecks.cpp', ROOT / 'tests/support/OffscreenSurface.h',
         ROOT / 'tests/render/UiTests.cpp', Path(__file__).resolve()]
manifest = dict(system=platform.platform(), backend=args.backend,
                compiler=cache.get('CMAKE_CXX_COMPILER:FILEPATH'),
                qtVersion=cache.get('YALAHOO_QT_VERSION:INTERNAL'), selectedKit=str(kit),
                binarySha256=hashlib.sha256(executable.read_bytes()).hexdigest(),
                sourceSha256={str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                              for p in sorted(files) if p.is_file()},
                fixture='576 invented albums, qrc portrait/landscape/square covers, no user data or media commands',
                narrowOnly=args.narrow, representative=args.representative,
                nativeWindowRequested=args.visible, foreground=args.foreground,
                scope='Production-stage matrix; GUI events/geometry/resources and own output checkpoints. Source structure, numeric motion and visual inspection remain separate.',
                completed=False, passed=False)
manifest_path = args.output / 'manifest.json'
def save():
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=2) + '\n', encoding='utf-8')
save()
with (args.output / 'run.log').open('w', encoding='utf-8') as log:
    try:
        command = [str(executable), '--gui', str((args.output / 'matrix').resolve())]
        if args.narrow:
            command.append('narrow')
        elif args.representative:
            command.append('--representative')
        result = subprocess.run(command,
                                cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT,
                                timeout=1800, check=False)
        code = result.returncode
    except subprocess.TimeoutExpired:
        code = 'timeout'
report_path = args.output / 'matrix/report.json'
report = json.loads(report_path.read_text(encoding='utf-8')) if report_path.is_file() else {}
sizes = ((1920, 1080), (1280, 720), (995, 858), (960, 900), (930, 900),
         (390, 844), (320, 720), (2560, 1080), (3440, 1440))
if args.narrow:
    sizes = tuple(size for size in sizes if size[0] <= 700)
elif args.representative:
    sizes = ((1280, 720), (930, 900), (390, 844))
expected = {(w, h, night, real, phase) for w, h in sizes for night in (False, True)
            for real in (False, True) for phase in
            (('browse', 'overview', 'detail', 'search') if real else
             ('browse', 'overview', 'detail', 'search', 'opening-mark', 'opening-scene'))}
captures = report.get('captures', [])
actual = {(c['width'], c['height'], c['night'], c['realistic'], c['phase']) for c in captures}
images = [args.output / 'matrix' / c['image'] for c in captures]
valid = (code == 0 and report.get('passed') and report.get('backend') == BACKEND_IDS[args.backend]
         and actual == expected and len(captures) == len(expected) and all(p.is_file() for p in images))
manifest.update(exitCode=code, completed=report_path.is_file(), passed=bool(valid),
                capturedCheckpoints=len(captures), expectedCheckpoints=len(expected),
                imageSha256={p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in images if p.is_file()})
save()
print(f"GUI {args.backend}: {len(captures)}/{len(expected)} checkpoints; passed={bool(valid)}")
raise SystemExit(0 if valid else 1)
