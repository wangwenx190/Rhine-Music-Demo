#!/usr/bin/env python3
"""Verify an immutable deployed bundle from a fresh unrelated directory."""
import argparse
import json
import os
from pathlib import Path
import platform
import subprocess
import tempfile
import sys

# The verifier must not mutate the package when importing its helper.
sys.dont_write_bytecode = True
from macos_bundle import inspect_bundle, verify_inventory


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if platform.system() != 'Darwin':
        raise SystemExit('The macOS product must be verified on native macOS.')
    local = Path(__file__).resolve().parent
    package = (args.package or (local if (local / 'yalahoo.app').is_dir()
                               else local.parents[1] / 'out/macos')).resolve()
    metadata = verify_inventory(package)
    bundle = package / 'yalahoo.app'
    closure = inspect_bundle(bundle)
    output = args.output.resolve() if args.output else Path(tempfile.mkdtemp(prefix='yalahoo-deployment-'))
    if output.is_relative_to(package):
        raise RuntimeError('Verification output must be outside the package')
    if args.output:
        output.mkdir(parents=True, exist_ok=False)
    environment = {key: value for key, value in os.environ.items() if not key.startswith(
        ('QT_', 'QML', 'QSG_', 'DYLD_', 'YALAHOO_', 'QCPAINTER_'))}
    environment.update(PATH='/usr/bin:/bin:/usr/sbin:/sbin', QSG_RHI_BACKEND='metal',
                       QT_MAC_DISABLE_FOREGROUND_APPLICATION_TRANSFORM='1',
                       YALAHOO_VERIFY_FOREGROUND='0', QCPAINTER_DEBUG_COLLECT='0')
    program = bundle / 'Contents/MacOS/yalahoo'
    timed_out = False
    with (output / 'stdout.txt').open('w') as stdout, (output / 'stderr.txt').open('w') as stderr:
        process = subprocess.Popen([str(program), '--verify-deployment', str(output / 'report.json')],
                                   cwd=output, env=environment, stdout=stdout, stderr=stderr)
        try:
            code = process.wait(timeout=60)
        except subprocess.TimeoutExpired:
            timed_out = True
            process.kill()
            code = process.wait()
    report = output / 'report.json'
    passed = not timed_out and code == 0 and report.is_file() and json.loads(report.read_text())['passed']
    evidence = dict(passed=passed, exitCode=code, timedOut=timed_out, qtVersion=metadata['qtVersion'],
                    os=platform.platform(), cleanPath=environment['PATH'],
                    workingDirectoryOutsidePackage=True, closure=closure)
    (output / 'environment.json').write_text(json.dumps(evidence, indent=2) + '\n')
    if not passed:
        raise SystemExit(f'Deployed product probe failed; evidence: {output}')
    print(f'Deployed product hidden QML/resources/media load passed; evidence: {output}')


if __name__ == '__main__':
    main()
