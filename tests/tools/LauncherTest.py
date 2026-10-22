"""Exercise native CLI/launcher argument rejection without showing windows or opening data."""
import os
import re
import shutil
import subprocess
import tempfile
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parents[2]
env = dict(os.environ, QT_QPA_PLATFORM="offscreen", QT_MAC_DISABLE_FOREGROUND_APPLICATION_TRANSFORM="1")
# The wrapper obtains plugin paths from the same configured Kit as the product.
with tempfile.TemporaryDirectory(prefix="yalahoo 匿名 参数 ") as temporary:
    env.update(TMPDIR=temporary, TMP=temporary, TEMP=temporary,
               QT_LOGGING_RULES="yalahoo.app.info=true", QT_MESSAGE_PATTERN="%{message}")

    def run(*arguments):
        result = subprocess.run(list(map(str, arguments)), cwd=temporary, env=env,
                                capture_output=True, timeout=15)
        logs = re.search(r'Logging started file=\s*"([^"]+)"', result.stderr.decode())
        assert logs, result.stderr.decode()
        path = Path(logs[1]).resolve()
        assert path.is_relative_to(Path(temporary).resolve()), path
        assert not path.parent.exists(), "Anonymous session logs were not cleaned up"
        return result

    # Exercise the development fallback independently of an existing production
    # package. Only the development Kit supplies the headless QPA test plugin;
    # production package isolation is checked by its deployed-product verifier.
    ROOT = Path(temporary) / "原生 开发"
    ROOT.mkdir()
    for name in ("yalahoo.sh", "yalahoo.command"):
        shutil.copy2(REPOSITORY / name, ROOT / name)
    shutil.copytree(REPOSITORY / "tools/launchers", ROOT / "tools/launchers")
    release = ROOT / "build/release"
    release.mkdir(parents=True)
    for name in ("bin", "CMakeCache.txt"):
        (release / name).symlink_to(REPOSITORY / "build/release" / name)
    target = Path(temporary) / "私人 库 #"
    launchers = [ROOT / "yalahoo.sh", ROOT / "yalahoo.command",
                 ROOT / "tools/launchers/demo.sh", ROOT / "tools/launchers/demo.command",
                 ROOT / "tools/launchers/lighting-lab.sh", ROOT / "tools/launchers/lighting-lab.command"]
    for launcher in launchers:
        result = run(launcher, "--help")
        assert result.returncode == 0, (launcher, result.stderr.decode())
        assert b"--demo" in result.stdout and b"--lighting-lab" in result.stdout
        args = [str(launcher), "--data-dir", str(target)]
        if launcher.parent == ROOT:
            args.append("--demo")
        result = run(*args)
        assert result.returncode == 7, (launcher, result.returncode, result.stderr.decode())
        assert b"cannot be combined" in result.stderr
        assert not target.exists(), "Invalid demo arguments touched a data root"
    for option in ["--music-root", "--import-legacy", "--import-preferences", "--verify-app"]:
        result = run(ROOT / "yalahoo.sh", "--lighting-lab", option, target)
        assert result.returncode == 7 and not target.exists(), (option, result.stderr.decode())
    for option in ("-h", "--help-all"):
        result = run(ROOT / "yalahoo.sh", option)
        assert result.returncode == 0 and b"--demo" in result.stdout, result.stderr.decode()
    result = run(ROOT / "yalahoo.sh", "--verify-app", "--unknown-fixture-option")
    assert result.returncode == 7 and b"unknown-fixture-option" in result.stderr, result.stderr.decode()
    result = run(ROOT / "yalahoo.sh", "--demo", "--rescan-library")
    assert result.returncode == 7 and not target.exists()
print("Native shell launchers preserve help/Unicode arguments and reject demo/data combinations before storage")
