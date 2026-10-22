#!/usr/bin/env python3
"""Verify an immutable deployed bundle from a fresh unrelated directory."""
import argparse
import os
from pathlib import Path
import platform
import sys

# The verifier must not mutate the package when importing its helper.
sys.dont_write_bytecode = True
from macos_bundle import inspect_bundle, verify_inventory

from package_support import finish_verification, run_probe, verification_output


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
    output = verification_output(package, args.output, 'yalahoo-deployment-')
    environment = {key: value for key, value in os.environ.items() if not key.startswith(
        ('QT_', 'QML', 'QSG_', 'DYLD_', 'YALAHOO_', 'QCPAINTER_'))}
    environment.update(PATH='/usr/bin:/bin:/usr/sbin:/sbin', QSG_RHI_BACKEND='metal',
                       QT_MAC_DISABLE_FOREGROUND_APPLICATION_TRANSFORM='1',
                       YALAHOO_VERIFY_FOREGROUND='0', QCPAINTER_DEBUG_COLLECT='0')
    program = bundle / 'Contents/MacOS/yalahoo'
    probe = run_probe([program], output, environment)
    passed = probe.passed
    evidence = dict(passed=passed, exitCode=probe.exit_code, timedOut=probe.timed_out, qtVersion=metadata['qtVersion'],
                    os=platform.platform(), cleanPath=environment['PATH'],
                    workingDirectoryOutsidePackage=True, closure=closure)
    finish_verification(output, evidence)


if __name__ == '__main__':
    main()
