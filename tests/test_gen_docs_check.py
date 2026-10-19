#!/usr/bin/env python3

import hashlib
import os
import pathlib
import shutil
import subprocess
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]


class GenDocsCheckTests(unittest.TestCase):
    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.root = pathlib.Path(self.temporary.name)
        (self.root / "tools").mkdir()
        (self.root / "docs").mkdir()
        shutil.copy2(ROOT / "tools" / "gen_docs.sh", self.root / "tools" / "gen_docs.sh")
        (self.root / "README.md").write_text(
            "before\n<!-- BEGIN COUNTS -->\nold\n<!-- END COUNTS -->\nafter\n"
        )
        (self.root / "docs" / "MUSEUM_MAP.md").write_text(
            "<!-- BEGIN MAP -->\nold\n<!-- END MAP -->\n"
        )
        (self.root / "docs" / "CNA_COVERAGE.md").write_text(
            "<!-- BEGIN COVERAGE -->\nold\n<!-- END COVERAGE -->\n"
        )
        (self.root / "docs" / "REFERENCE_VIEWS.md").write_text("old\n")
        self.binary = self.root / "fake-museum"
        self.binary.write_text(
            "#!/bin/sh\n"
            "case \"$1\" in\n"
            "  --validate) echo 'summary' ;;\n"
            "  --map-markdown) echo 'map' ;;\n"
            "  --coverage-markdown) echo 'coverage' ;;\n"
            "  --list-views) printf 'lobby\\tinfo\\tThe lobby\\n' ;;\n"
            "  *) exit 9 ;;\n"
            "esac\n"
        )
        self.binary.chmod(0o755)
        self.environment = {
            **os.environ,
            "MUSEUM_BIN": str(self.binary),
            "BUILD_DIR": "build",
        }

    def tearDown(self):
        self.temporary.cleanup()

    def run_script(self, *arguments):
        return subprocess.run(
            [str(self.root / "tools" / "gen_docs.sh"), *arguments],
            cwd=self.root,
            env=self.environment,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=False,
        )

    def source_digest(self):
        digest = hashlib.sha256()
        paths = (self.root / "README.md", *sorted((self.root / "docs").iterdir()))
        for path in paths:
            digest.update(path.read_bytes())
        return digest.hexdigest()

    def test_write_then_check_is_non_mutating(self):
        self.assertEqual(self.run_script().returncode, 0)
        before = self.source_digest()
        checked = self.run_script("--check")
        self.assertEqual(checked.returncode, 0, checked.stderr)
        self.assertEqual(self.source_digest(), before)

    def test_check_rejects_stale_output_without_rewriting_it(self):
        self.assertEqual(self.run_script().returncode, 0)
        readme = self.root / "README.md"
        readme.write_text(readme.read_text().replace("summary", "stale"))
        before = self.source_digest()
        checked = self.run_script("--check")
        self.assertNotEqual(checked.returncode, 0)
        self.assertIn("stale", checked.stderr)
        self.assertEqual(self.source_digest(), before)

    def test_missing_marker_and_binary_are_errors(self):
        (self.root / "docs" / "MUSEUM_MAP.md").write_text("no markers\n")
        self.assertNotEqual(self.run_script("--check").returncode, 0)
        self.environment["MUSEUM_BIN"] = str(self.root / "missing")
        self.assertEqual(self.run_script("--check").returncode, 2)


if __name__ == "__main__":
    unittest.main()
