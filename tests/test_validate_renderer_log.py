#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "validate_renderer_log", ROOT / "tools" / "validate_renderer_log.py"
)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


def complete_log(*, second_status: str = "SKIP") -> str:
    return "\n".join(
        (
            "[museum][script] goto rv-geometry",
            "[museum][validate] rv-geometry triangle coverage: PASS (expected red, got red)",
            "[museum][script] goto rv-effects",
            f"[museum][validate] rv-effects compiled effect: {second_status} (unavailable)",
            "[museum][stats] built 2 loaded 2 exhibit errors 0",
        )
    )


class ValidateRendererLogTests(unittest.TestCase):
    rooms = ["rv-geometry", "rv-effects"]

    def test_complete_run_passes_and_preserves_all_results(self):
        result = MODULE.validate_log(self.rooms, complete_log())
        self.assertEqual(result.errors, ())
        self.assertEqual((result.pass_count, result.skip_count), (1, 1))
        self.assertEqual(len(result.check_lines), 2)

    def test_empty_room_inventory_is_never_green(self):
        result = MODULE.validate_log([], "[museum][stats] done")
        self.assertIn("the museum listed no renderer-validation rooms", result.errors)
        self.assertIn("the log contains no validation results", result.errors)

    def test_missing_room_result_is_never_green(self):
        text = complete_log().replace(
            "[museum][validate] rv-effects compiled effect: SKIP (unavailable)\n", ""
        )
        result = MODULE.validate_log(self.rooms, text)
        self.assertIn("rooms with no validation result: rv-effects", result.errors)

    def test_missing_visit_is_never_green(self):
        text = complete_log().replace("[museum][script] goto rv-effects\n", "")
        result = MODULE.validate_log(self.rooms, text)
        self.assertIn("rooms not visited: rv-effects", result.errors)

    def test_missing_completion_marker_is_never_green(self):
        text = complete_log().replace("[museum][stats] built 2 loaded 2 exhibit errors 0", "")
        result = MODULE.validate_log(self.rooms, text)
        self.assertIn("the scripted run did not reach its final stats marker", result.errors)

    def test_nonzero_exhibit_error_count_is_never_green(self):
        text = complete_log().replace("exhibit errors 0", "exhibit errors 2")
        result = MODULE.validate_log(self.rooms, text)
        self.assertIn("the final stats report 2 exhibit error(s)", result.errors)

    def test_fail_result_is_never_green(self):
        result = MODULE.validate_log(self.rooms, complete_log(second_status="FAIL"))
        self.assertIn("1 validation check(s) failed", result.errors)

    def test_exhibit_or_draw_error_is_never_green(self):
        for marker in ("[museum][exhibit-error]", "[museum][draw-error]"):
            with self.subTest(marker=marker):
                result = MODULE.validate_log(self.rooms, complete_log() + "\n" + marker)
                self.assertTrue(any(marker in error for error in result.errors))

    def test_aborted_validation_line_is_never_green(self):
        result = MODULE.validate_log(
            self.rooms, complete_log() + "\n[museum][validate] rv-effects aborted: exploded"
        )
        self.assertTrue(any("malformed or aborted" in error for error in result.errors))

    def test_unexpected_validation_room_is_never_green(self):
        result = MODULE.validate_log(
            self.rooms,
            complete_log()
            + "\n[museum][validate] rv-surprise something: PASS (expected yes, got yes)",
        )
        self.assertIn("unexpected validation result from room rv-surprise", result.errors)


if __name__ == "__main__":
    unittest.main()
