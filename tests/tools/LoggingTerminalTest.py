#!/usr/bin/env python3
"""Verify actual terminal colors and file redirection without creating a window."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

binary = sys.argv[1]
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
    if os.name != 'nt':
        import pty
        master, slave = pty.openpty()
        try:
            process = subprocess.Popen([binary, '--emit', str(root / 'tty-logs'), 'normal'],
                                       stderr=slave, env=environment)
            os.close(slave)
            slave = -1
            captured = bytearray()
            while True:
                try:
                    chunk = os.read(master, 16384)
                except OSError:
                    break
                if not chunk:
                    break
                captured.extend(chunk)
            assert process.wait(timeout=10) == 0
            assert b'\x1b[33m' in captured and b'\x1b[31m' in captured
            assert b'\x1b[0m' in captured
            for line in captured.splitlines():
                if b'info-marker' in line:
                    assert b'\x1b[' not in line
            files = list((root / 'tty-logs').glob('*.log'))
            assert len(files) == 1 and b'\x1b[' not in files[0].read_bytes()
        finally:
            os.close(master)
            if slave >= 0:
                os.close(slave)
