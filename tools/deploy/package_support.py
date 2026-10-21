"""Shared immutable-package hashing, runtime probing and transactional installation."""
from dataclasses import dataclass
import hashlib
import json
import shutil
import subprocess
import uuid

PROBE_TIMEOUT_SECONDS = 60
HASH_BLOCK_BYTES = 1024 * 1024


def digest(path):
    checksum = hashlib.sha256()
    with path.open('rb') as stream:
        for block in iter(lambda: stream.read(HASH_BLOCK_BYTES), b''):
            checksum.update(block)
    return checksum.hexdigest()


@dataclass(frozen=True)
class ProbeResult:
    exit_code: int
    timed_out: bool
    report: dict


def run_probe(command, output, environment, *, timeout=PROBE_TIMEOUT_SECONDS):
    report_path = output / 'report.json'
    timed_out = False
    with (output / 'stdout.txt').open('w', encoding='utf-8') as stdout, \
            (output / 'stderr.txt').open('w', encoding='utf-8') as stderr:
        process = subprocess.Popen([*map(str, command), '--verify-deployment', str(report_path)],
                                   cwd=output, env=environment, stdout=stdout, stderr=stderr)
        try:
            code = process.wait(timeout=timeout)
        except subprocess.TimeoutExpired:
            timed_out = True
            process.kill()
            code = process.wait()
    try:
        report = json.loads(report_path.read_text(encoding='utf-8'))
    except (OSError, ValueError):
        report = {}
    return ProbeResult(code, timed_out, report if isinstance(report, dict) else {})


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
