#!/usr/bin/env python3
"""The step accounting of tools/check_all.sh: a skip is never reported or counted as a pass."""

import pathlib
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


def run_steps(body: str) -> subprocess.CompletedProcess:
    with tempfile.TemporaryDirectory() as build:
        script = f"set -u\nBUILD_DIR={build}\nsource {ROOT / 'tools' / '_steps.sh'}\n{body}\nsummary\nexit $failed\n"
        return subprocess.run(["bash", "-c", script], capture_output=True, text=True, check=False)


class CheckStepsTests(unittest.TestCase):
    def test_pass_fail_and_skip_are_distinct(self):
        result = run_steps(
            'step "good" true\n'
            'step "bad" false\n'
            "step \"absent\" bash -c 'echo \"skipped: xdotool is not installed\"; exit 77'\n"
        )
        lines = result.stdout.splitlines()
        self.assertEqual(lines[0], "ok    good")
        self.assertTrue(lines[1].startswith("FAIL  bad"))
        self.assertTrue(lines[2].startswith("skip  absent  (skipped: xdotool is not installed"))
        self.assertEqual(lines[-1], "summary: 1 passed, 1 failed, 1 skipped, 0 not run")
        self.assertEqual(result.returncode, 1)

    def test_skip_alone_does_not_fail_but_is_not_a_pass(self):
        result = run_steps("step \"absent\" bash -c 'exit 77'\n")
        self.assertEqual(result.returncode, 0)
        self.assertIn("summary: 0 passed, 0 failed, 1 skipped, 0 not run", result.stdout)
        self.assertNotIn("ok ", result.stdout)

    def test_required_skip_is_a_failure(self):
        result = run_steps(
            "step \"online\" bash -c 'echo skipped: the service is not built; exit 77'\n"
            '[ $? = 77 ] && required_skip "online"\n'
        )
        self.assertEqual(result.returncode, 1)
        self.assertIn("FAIL  online: the profile requires it, but it was skipped", result.stdout)
        self.assertIn("summary: 0 passed, 1 failed, 0 skipped, 0 not run", result.stdout)

    def test_not_run_parts_are_listed(self):
        result = run_steps('step "good" true\nnotrun+=("online service (ONLINE=1)")\nnotrun+=("slow renderers")\n')
        self.assertIn(
            "summary: 1 passed, 0 failed, 0 skipped, 2 not run (online service (ONLINE=1);slow renderers)",
            result.stdout,
        )
        self.assertEqual(result.returncode, 0)


if __name__ == "__main__":
    unittest.main()
