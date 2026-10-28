"""Run the real CMake policy and CTest presets against harmless process fixtures."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[2]
FLAGS = ("VISIBLE", "FOREGROUND", "WINDOW_LIFECYCLE")


class TestPolicyTest(unittest.TestCase):
    def test_presets_isolate_opt_ins_and_select_only_the_declared_scope(self):
        for target in ("macos", "windows", "linux"):
            with self.subTest(target=target):
                self.check_presets(target)

    def check_presets(self, target):
        with tempfile.TemporaryDirectory(prefix="yalahoo ctest space-") as directory:
            root = Path(directory)
            shutil.copyfile(ROOT / "CMakePresets.json", root / "CMakePresets.json")
            fixture = root / "fixture.py"
            fixture.write_text("import json, os, pathlib, sys\n"
                               "pathlib.Path(sys.argv[1]).write_text(json.dumps({k: v for k, v in os.environ.items() "
                               "if k.startswith('YALAHOO_VERIFY_')}))\n", encoding="utf-8")
            for folder in ("tests/unit", "tests/integration", "tests/render"):
                (root / folder).mkdir(parents=True)
                (root / folder / "CMakeLists.txt").write_text("", encoding="utf-8")
            declarations = ["cmake_minimum_required(VERSION 3.25)", "project(PolicyFixture NONE)",
                            "enable_testing()", f"set(APPLE {'TRUE' if target == 'macos' else 'FALSE'})",
                            f"set(WIN32 {'TRUE' if target == 'windows' else 'FALSE'})",
                            "set(YALAHOO_MACOS_VISIBLE_TESTS OFF)"]
            names = ("unit-probe", "startup-qml", "runtime-detail", "rhi-bokeh-gpu",
                     "playback-device", "audio-ambience-device", "settings-qml", "playback-qml")
            for name in names:
                declarations.append(f'add_test(NAME {name} COMMAND "{Path(sys.executable).as_posix()}" '
                                    f'"{fixture.as_posix()}" "{(root / (name + ".json")).as_posix()}")')
            declarations += [*[f"add_subdirectory({folder})" for folder in ("tests/unit", "tests/integration", "tests/render")],
                             f'include("{(ROOT / "tests/cmake/TestPolicy.cmake").as_posix()}")']
            (root / "CMakeLists.txt").write_text("\n".join(declarations) + "\n", encoding="utf-8")
            subprocess.run(["cmake", "--preset", "qt-release"], cwd=root, check=True, capture_output=True, timeout=20)
            environment = {**os.environ, **{f"YALAHOO_VERIFY_{flag}": "1" for flag in FLAGS}}
            for preset, expected in (("qt-release", {"unit-probe"}),
                                     ("qt-release-full", {"unit-probe", "rhi-bokeh-gpu"}),
                                     ("qt-release-render", {"rhi-bokeh-gpu"}),
                                     ("qt-release-media", {"playback-device", "audio-ambience-device"}
                                      | ({"startup-qml", "settings-qml", "playback-qml"}
                                         if target == "macos" else set()))):
                with self.subTest(preset=preset):
                    for name in names:
                        (root / (name + ".json")).unlink(missing_ok=True)
                    result = subprocess.run(["ctest", "--preset", preset], cwd=root, env=environment,
                                            capture_output=True, text=True, timeout=20)
                    self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
                    self.assertEqual({name for name in names if (root / (name + ".json")).exists()}, expected)
                    for name in expected:
                        actual = json.loads((root / (name + ".json")).read_text())
                        for flag in FLAGS:
                            wanted = "0"
                            self.assertEqual(actual[f"YALAHOO_VERIFY_{flag}"], wanted)


if __name__ == "__main__":
    unittest.main()
