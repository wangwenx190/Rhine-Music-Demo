"""Exercise native CLI/launcher argument rejection without showing windows or opening data."""
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

REPOSITORY = Path(__file__).resolve().parents[2]
env = dict(os.environ, QT_QPA_PLATFORM="offscreen", QT_MAC_DISABLE_FOREGROUND_APPLICATION_TRANSFORM="1")
# The wrapper obtains plugin paths from the same configured Kit as the product.
with tempfile.TemporaryDirectory(prefix="yalahoo 匿名 参数 ") as temporary:
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
        result = subprocess.run([str(launcher), "--help"], cwd=temporary, env=env, capture_output=True, timeout=15)
        assert result.returncode == 0, (launcher, result.stderr.decode())
        assert b"--demo" in result.stdout and b"--lighting-lab" in result.stdout
        args = [str(launcher), "--data-dir", str(target)]
        if launcher.parent == ROOT:
            args.append("--demo")
        result = subprocess.run(args, cwd=temporary, env=env, capture_output=True, timeout=15)
        assert result.returncode == 7, (launcher, result.returncode, result.stderr.decode())
        assert b"cannot be combined" in result.stderr
        assert not target.exists(), "Invalid demo arguments touched a data root"
    for option in ["--music-root", "--import-legacy", "--import-preferences", "--verify-runtime"]:
        result = subprocess.run([str(ROOT / "yalahoo.sh"), "--lighting-lab", option, str(target)],
                                cwd=temporary, env=env, capture_output=True, timeout=15)
        assert result.returncode == 7 and not target.exists(), (option, result.stderr.decode())
    result = subprocess.run([str(ROOT / "yalahoo.sh"), "--demo", "--rescan-library"],
                            cwd=temporary, env=env, capture_output=True, timeout=15)
    assert result.returncode == 7 and not target.exists()
print("Native shell launchers preserve help/Unicode arguments and reject demo/data combinations before storage")
