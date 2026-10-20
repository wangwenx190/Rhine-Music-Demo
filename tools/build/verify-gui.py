#!/usr/bin/env python3
"""Run one integrated GUI matrix and preserve its inputs, output and failure status."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import platform
import subprocess

ROOT = Path(__file__).resolve().parents[2]
parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--backend', required=True, choices=('metal', 'd3d11', 'd3d12', 'opengl', 'vulkan'))
parser.add_argument('--narrow', action='store_true', help='Regress only viewports <=700 after a narrow-layout change')
parser.add_argument('--visible', action='store_true', help='Use a native window on macOS')
parser.add_argument('--foreground', action='store_true', help="Use this round's authorized foreground policy")
parser.add_argument('--output', type=Path, required=True)
args = parser.parse_args()
if args.foreground and not args.visible:
    parser.error('--foreground requires --visible')
if args.output.exists():
    parser.error('Choose a fresh output directory; preserve failed runs')
cache = dict(line.split('=', 1) for line in (ROOT / 'build/release/CMakeCache.txt').read_text(encoding='utf-8').splitlines()
             if '=' in line and not line.startswith(('#', '//')))
kit = Path(cache['YALAHOO_SELECTED_QT:PATH'])
executable = ROOT / 'build/release/tools' / ('ui-audit.exe' if platform.system() == 'Windows' else 'ui-audit')
if cache.get('CMAKE_BUILD_TYPE:STRING') != 'Release' or not executable.is_file():
    parser.error('Build the latest native Kit Release ui-audit first')
args.output.mkdir(parents=True)
env = os.environ.copy()
env.update(QSG_RHI_BACKEND=args.backend, QT_SCALE_FACTOR='1',
           QT_PLUGIN_PATH=str(kit / 'plugins'), YALAHOO_VERIFY_FOREGROUND='1' if args.foreground else '0',
           YALAHOO_VERIFY_VISIBLE='1' if args.visible else '0', QT_MAC_DISABLE_FOREGROUND_APPLICATION_TRANSFORM='1',
           QSG_RENDER_LOOP='basic' if platform.system() == 'Darwin' and args.backend == 'opengl' else 'threaded')
env.pop('QSG_NO_VSYNC', None)
if platform.system() == 'Windows':
    env['PATH'] = str(kit / 'bin') + os.pathsep + env.get('PATH', '')
files = [*ROOT.glob('src/**/*'), *ROOT.glob('qml/**/*.qml'), *ROOT.glob('resources/**/*'),
         ROOT / 'tools/review/StartupAudit.cpp', ROOT / 'tools/review/OffscreenSurface.h',
         ROOT / 'tools/review/UiAudit.cpp', Path(__file__).resolve()]
manifest = dict(system=platform.platform(), backend=args.backend,
                compiler=cache.get('CMAKE_CXX_COMPILER:FILEPATH'),
                qtVersion=cache.get('YALAHOO_QT_VERSION:INTERNAL'), selectedKit=str(kit),
                binarySha256=hashlib.sha256(executable.read_bytes()).hexdigest(),
                sourceSha256={str(p.relative_to(ROOT)): hashlib.sha256(p.read_bytes()).hexdigest()
                              for p in sorted(files) if p.is_file()},
                fixture='576 invented albums, qrc portrait/landscape/square covers, no user data or media commands',
                narrowOnly=args.narrow, nativeWindowRequested=args.visible, foreground=args.foreground,
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
        result = subprocess.run(command,
                                cwd=ROOT, env=env, stdout=log, stderr=subprocess.STDOUT,
                                timeout=1800, check=False)
        code = result.returncode
    except subprocess.TimeoutExpired:
        code = 'timeout'
report_path = args.output / 'matrix/report.json'
report = json.loads(report_path.read_text(encoding='utf-8')) if report_path.is_file() else {}
backend_ids = dict(metal=6, d3d11=4, d3d12=8, opengl=3, vulkan=5)
sizes = ((1920, 1080), (1280, 720), (995, 858), (960, 900), (930, 900),
         (390, 844), (320, 720), (2560, 1080), (3440, 1440))
if args.narrow:
    sizes = tuple(size for size in sizes if size[0] <= 700)
expected = {(w, h, night, real, phase) for w, h in sizes for night in (False, True)
            for real in (False, True) for phase in
            (('browse', 'overview', 'detail', 'search') if real else
             ('browse', 'overview', 'detail', 'search', 'opening-mark', 'opening-scene'))}
captures = report.get('captures', [])
actual = {(c['width'], c['height'], c['night'], c['realistic'], c['phase']) for c in captures}
images = [args.output / 'matrix' / c['image'] for c in captures]
valid = (code == 0 and report.get('passed') and report.get('backend') == backend_ids[args.backend]
         and actual == expected and len(captures) == len(expected) and all(p.is_file() for p in images))
manifest.update(exitCode=code, completed=report_path.is_file(), passed=bool(valid),
                capturedCheckpoints=len(captures), expectedCheckpoints=len(expected),
                imageSha256={p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in images if p.is_file()})
save()
print(f"GUI {args.backend}: {len(captures)}/{len(expected)} checkpoints; passed={bool(valid)}")
raise SystemExit(0 if valid else 1)
