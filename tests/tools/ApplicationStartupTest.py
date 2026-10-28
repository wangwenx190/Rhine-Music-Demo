"""Checks early command handling in the actual product executable, without a GUI platform."""
import os
import subprocess
import sys
import unittest


EXECUTABLE = sys.argv.pop(1)


class ApplicationStartupTest(unittest.TestCase):
    def test_help_and_errors_precede_graphics_initialization(self):
        environment = dict(os.environ, QSG_RHI_BACKEND="invalid-review-backend",
                           QT_QPA_PLATFORM="unavailable-test-platform")
        for arguments, expected in [(["--help"], 0), (["--help-all"], 0), (["-h"], 0),
                                    (["--unknown-option"], 7),
                                    (["--data-dir=relative"], 7)]:
            with self.subTest(arguments=arguments):
                result = subprocess.run([EXECUTABLE, *arguments], env=environment,
                                        capture_output=True, timeout=10)
                self.assertEqual(result.returncode, expected, result.stderr.decode())
                if expected == 0:
                    self.assertIn(b"--music-root", result.stdout)
                else:
                    self.assertTrue(result.stderr)
                self.assertNotIn(b"Application starting", result.stderr)
                self.assertNotIn(b"platform plugin", result.stderr)


if __name__ == "__main__":
    unittest.main()
