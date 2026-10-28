#!/usr/bin/env python3
"""Verify an immutable native Linux package without creating a GPU context or visible window."""
import argparse
from pathlib import Path
import platform
import sys

sys.dont_write_bytecode = True
from linux_package import clean_environment, inspect_package, verify_inventory

from package_support import finish_verification, run_probe, verification_output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--package', type=Path)
    parser.add_argument('--output', type=Path)
    args = parser.parse_args()
    if platform.system() != 'Linux':
        raise SystemExit('The Linux product must be verified on native Linux.')
    local = Path(__file__).resolve().parent
    package = (args.package or (local if (local / 'package.json').is_file()
                               else local.parents[1] / 'out/linux')).resolve()
    metadata = verify_inventory(package)
    closure = inspect_package(package)
    output = verification_output(package, args.output, 'yalahoo-linux-')
    environment = clean_environment(package)
    environment.update(QT_QPA_PLATFORM='offscreen', QSG_RHI_BACKEND='opengl',
                       YALAHOO_VERIFY_FOREGROUND='0', QCPAINTER_DEBUG_COLLECT='0',
                       XDG_DATA_HOME=str(output / 'xdg-data'), XDG_CONFIG_HOME=str(output / 'xdg-config'),
                       XDG_CACHE_HOME=str(output / 'xdg-cache'))
    probe = run_probe([package / 'yalahoo.sh'], output, environment)
    data = probe.report
    # The runtime probe lists all loaded ELF images; static ldd alone cannot see dlopen.
    passed = probe.passed and bool(data.get('loadedImages'))
    if data.get('qtVersion') != metadata['qtVersion'] or data.get('platformPlugin') != 'offscreen':
        passed = False
    for path in data.get('pluginPaths', []) + data.get('qmlPaths', []):
        if not path.startswith('qrc:') and not Path(path).resolve().is_relative_to(package):
            passed = False
    evidence = dict(passed=bool(passed), exitCode=probe.exit_code, timedOut=probe.timed_out,
                    qtVersion=metadata['qtVersion'], os=platform.platform(), closure=closure,
                    workingDirectoryOutsidePackage=True, scope='No GPU/context/playback/device/presentation')
    finish_verification(output, evidence)


if __name__ == '__main__':
    main()
