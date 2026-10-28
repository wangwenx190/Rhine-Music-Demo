"""Verify committed branding and exercise clean-input export and atomic failure boundaries."""
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

executable, repository, evidence = map(Path, sys.argv[1:])
environment = dict(os.environ, QT_QPA_PLATFORM="offscreen",
                   QT_MAC_DISABLE_FOREGROUND_APPLICATION_TRANSFORM="1")
Path(evidence).mkdir(parents=True, exist_ok=True)
output = Path(tempfile.mkdtemp(prefix="branding-", dir=evidence))


def run(root, report, verify=False):
    arguments = [str(executable), str(root), "--report", str(report)]
    if verify:
        arguments.append("--verify-only")
    return subprocess.run(arguments, env=environment, capture_output=True, timeout=60)


def inventory(root):
    return {str(path.relative_to(root)): hashlib.sha256(path.read_bytes()).hexdigest()
            for path in root.rglob("*") if path.is_file()}


report = output / "report.json"
result = run(repository, report, verify=True)
assert result.returncode == 0, result.stderr.decode()
original = report.read_bytes()
result = run(repository, report, verify=True)
assert result.returncode != 0 and report.read_bytes() == original
with tempfile.TemporaryDirectory(prefix="branding clean inputs ") as temporary:
    root = Path(temporary)
    branding = root / "resources/branding"
    branding.mkdir(parents=True)
    for source in (repository / "resources/branding").glob("*.svg"):
        shutil.copy2(source, branding)
    # A missing generated asset is still a forbidden diagnostic destination.
    before = inventory(root)
    for forbidden in [branding / "reveal-map.json",
                      root / "resources/manifests/branding.json",
                      root / "resources/icons/yalahoo/new.png",
                      root / "docs/media/branding/new.png"]:
        result = run(root, forbidden)
        assert result.returncode != 0, result.stderr.decode()
        assert inventory(root) == before, "Rejected report path modified product files"
    # Resolve a report ancestor alias before checking protected destinations.
    alias = root / "brand-alias"
    alias.symlink_to(branding, target_is_directory=True)
    result = run(root, alias / "reveal-map.json")
    assert result.returncode != 0
    assert inventory(root) == before
    alias.unlink()
    # No ignored concepts.svg or existing icon derivatives are available in this checkout.
    result = run(root, output / "export.json")
    assert result.returncode == 0, result.stderr.decode()
    result = run(root, output / "export-verified.json", verify=True)
    assert result.returncode == 0, result.stderr.decode()
    before = inventory(root)
    # Stale generated files cannot survive a successful export with an incomplete manifest.
    obsolete = root / "resources/icons/yalahoo/obsolete.png"
    obsolete.write_bytes(b"old derivative")
    before = inventory(root)
    result = run(root, output / "obsolete.json")
    assert result.returncode != 0
    assert inventory(root) == before
    obsolete.unlink()
    blocker = output / "not-a-directory"
    blocker.write_bytes(b"preserve")
    before = inventory(root)
    result = run(root, blocker / "report.json")
    assert result.returncode != 0
    assert inventory(root) == before
    assert blocker.read_bytes() == b"preserve"
    # A late publication failure must preserve every earlier product asset.
    manifest = root / "resources/manifests/branding.json"
    manifest.unlink()
    manifest.mkdir()
    before = inventory(root)
    result = run(root, output / "blocked.json")
    assert result.returncode != 0
    assert json.loads((output / "blocked.json").read_text())["passed"] is False
    assert inventory(root) == before, "Failed export partially replaced product files"
print(f"Branding input/export/rollback and report preservation passed; evidence: {output}")
