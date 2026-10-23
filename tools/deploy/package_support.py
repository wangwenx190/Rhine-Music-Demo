"""Shared immutable-package hashing, runtime probing and transactional installation."""
from dataclasses import dataclass
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import uuid
import tempfile
import sys

helper_directory = Path(__file__).resolve().parent
if not (helper_directory / 'report_values.py').is_file():
    helper_directory = helper_directory.parent
sys.path.insert(0, str(helper_directory))
from report_values import read_report

PROBE_TIMEOUT_SECONDS = 60
HASH_BLOCK_BYTES = 1024 * 1024


def run(*arguments, env=None):
    return subprocess.run([str(value) for value in arguments], env=env, check=True,
                          capture_output=True, text=True).stdout.strip()


def isolated_environment(loader_prefix):
    prefixes = ('QT_', 'QML', 'QSG_', 'YALAHOO_', 'QCPAINTER_', loader_prefix)
    return {key: value for key, value in os.environ.items() if not key.startswith(prefixes)}


def verify_package_inventory(root, actual, *, system=None):
    metadata = json.loads((root / 'package.json').read_text())
    if (metadata.get('files') != actual or metadata.get('review') is not False
            or metadata.get('configuration') != 'Release'
            or (system is not None and metadata.get('system') != system)):
        raise RuntimeError('Production package inventory or configuration changed')
    return metadata


def digest(path):
    checksum = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(HASH_BLOCK_BYTES), b''):
            checksum.update(block)
    return checksum.hexdigest()


def package_inventory(root, *, file_permissions=False):
    root = root.resolve()
    files = {}
    for path in sorted(root.rglob('*')):
        relative = str(path.relative_to(root))
        if path.is_symlink():
            target = path.readlink()
            if target.is_absolute() or not path.resolve(strict=True).is_relative_to(root):
                raise RuntimeError(f'External package symlink: {path}')
            files[relative] = dict(symlink=str(target))
        elif path.is_file() and relative != 'package.json':
            mode = path.stat().st_mode
            permission = dict(mode=mode & 0o777) if file_permissions else dict(executable=bool(mode & 0o111))
            files[relative] = dict(bytes=path.stat().st_size, sha256=digest(path), **permission)
    return files


def copy_licenses(root, kit, destination):
    qt_licenses = next((path / 'Licenses' for path in (kit, *kit.parents)
                        if (path / 'Licenses').is_dir()), None)
    if qt_licenses is None:
        raise RuntimeError('Selected Qt installation license directory is missing')
    shutil.copytree(root / 'resources/licenses', destination / 'project')
    for name in ('NOTICE.txt', 'MiSans-license.pdf'):
        shutil.copyfile(root / 'resources/fonts' / name, destination / name)
    shutil.copyfile(root / 'resources/audio/README.md', destination / 'audio.md')
    shutil.copytree(qt_licenses, destination / 'Qt')


def verification_output(package, requested, prefix):
    output = requested.resolve() if requested else Path(tempfile.mkdtemp(prefix=prefix))
    if output.is_relative_to(package.resolve()):
        raise RuntimeError('Verification output must be outside the package')
    if requested:
        output.mkdir(parents=True, exist_ok=False)
    return output


def finish_verification(output, evidence):
    (output / 'environment.json').write_text(json.dumps(evidence, indent=2) + '\n', encoding='utf-8')
    if not evidence['passed']:
        raise SystemExit(f'Deployed product probe failed; evidence: {output}')
    print(f'Deployed product hidden QML/resources/media load passed; evidence: {output}')


@dataclass(frozen=True)
class ProbeResult:
    exit_code: int
    timed_out: bool
    report: dict

    @property
    def passed(self):
        return not self.timed_out and self.exit_code == 0 and self.report.get('passed') is True


def run_probe(command, output, environment, *, timeout=PROBE_TIMEOUT_SECONDS):
    report_path = output / 'report.json'
    timed_out = False
    with (output / 'stdout.txt').open('w', encoding='utf-8') as stdout, \
            (output / 'stderr.txt').open('w', encoding='utf-8') as stderr:
        try:
            process = subprocess.Popen([*map(str, command), '--verify-deployment', str(report_path)],
                                       cwd=output, env=environment, stdout=stdout, stderr=stderr)
        except OSError as error:
            stderr.write(f'Cannot launch deployment probe: {error}\n')
            return ProbeResult(-1, False, {})
        try:
            code = process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            timed_out = True
            process.kill()
            code = process.wait()
    return ProbeResult(code, timed_out, read_report(report_path))


def install_package(staging, output):
    backup = output.with_name(output.name + '-previous-' + uuid.uuid4().hex)
    if output.exists():
        output.rename(backup)
    try:
        staging.rename(output)
    except BaseException:
        if backup.exists() and not output.exists():
            backup.rename(output)
        raise
    if backup.exists():
        shutil.rmtree(backup)
