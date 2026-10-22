#!/usr/bin/env python3
"""Verify actual terminal colors and file redirection without creating a window."""
import os
from pathlib import Path
import selectors
import subprocess
import sys
import tempfile
import time


def read_terminal(master, process, timeout):
    deadline = time.monotonic() + timeout
    captured = bytearray()
    with selectors.DefaultSelector() as selector:
        selector.register(master, selectors.EVENT_READ)
        while True:
            remaining = deadline - time.monotonic()
            if remaining <= 0 or not selector.select(remaining):
                raise subprocess.TimeoutExpired(process.args, timeout)
            try:
                chunk = os.read(master, 16384)
            except OSError:
                break
            if not chunk:
                break
            captured.extend(chunk)
    if process.wait(timeout=max(0, deadline - time.monotonic())) != 0:
        raise AssertionError("Terminal log process failed")
    return captured


def main(binary):
    environment = dict(os.environ)
    environment.pop('QT_MESSAGE_PATTERN', None)
    environment.pop('QT_LOGGING_RULES', None)
    with tempfile.TemporaryDirectory() as directory:
        root = Path(directory)
        with (root / 'redirected.txt').open('wb') as stream:
            subprocess.run([binary, '--emit', str(root / 'file-logs'), 'normal'],
                           stderr=stream, env=environment, check=True, timeout=10)
        redirected = (root / 'redirected.txt').read_bytes()
        assert b'warning-marker' in redirected and b'\x1b[' not in redirected
        if os.name == 'nt':
            return
        import pty
        master, slave = pty.openpty()
        process = None
        try:
            process = subprocess.Popen([binary, '--emit', str(root / 'tty-logs'), 'normal'],
                                       stderr=slave, env=environment)
            os.close(slave)
            slave = -1
            captured = read_terminal(master, process, 10)
            assert b'\x1b[33m' in captured and b'\x1b[31m' in captured
            assert b'\x1b[0m' in captured
            for line in captured.splitlines():
                if b'info-marker' in line:
                    assert b'\x1b[' not in line
            files = list((root / 'tty-logs').glob('*.log'))
            assert len(files) == 1 and b'\x1b[' not in files[0].read_bytes()
        finally:
            if process is not None and process.poll() is None:
                process.kill()
                process.wait(timeout=5)
            os.close(master)
            if slave >= 0:
                os.close(slave)


if __name__ == '__main__':
    main(sys.argv[1])
