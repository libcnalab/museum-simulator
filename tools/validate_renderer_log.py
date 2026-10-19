#!/usr/bin/env python3
"""Validate that a renderer-check log is complete before reporting it as green."""

from __future__ import annotations

import argparse
import dataclasses
import pathlib
import re
import sys


RESULT = re.compile(
    r"^\[museum\]\[validate\] (?P<room>\S+) .+: "
    r"(?P<status>PASS|FAIL|INFO|SKIP)(?: \(|$)"
)
GOTO = re.compile(r"^\[museum\]\[script\] goto (?P<room>\S+)$")
STATS = re.compile(r"^\[museum\]\[stats\].*\bexhibit errors (?P<errors>\d+)\b")
ERROR_MARKERS = (
    "[museum][exhibit-error]",
    "[museum][draw-error]",
    "[museum][script] goto: unknown room",
    "[museum][script] pressall: exception",
)


@dataclasses.dataclass(frozen=True)
class Validation:
    pass_count: int
    fail_count: int
    info_count: int
    skip_count: int
    check_lines: tuple[str, ...]
    errors: tuple[str, ...]


def validate_log(expected_rooms: list[str], text: str) -> Validation:
    errors: list[str] = []
    if not expected_rooms:
        errors.append("the museum listed no renderer-validation rooms")
    if len(expected_rooms) != len(set(expected_rooms)):
        errors.append("the expected room list contains duplicates")

    expected = set(expected_rooms)
    visited: set[str] = set()
    seen: dict[str, int] = {room: 0 for room in expected_rooms}
    counts = {"PASS": 0, "FAIL": 0, "INFO": 0, "SKIP": 0}
    check_lines: list[str] = []
    malformed: list[str] = []
    stats_errors: list[int] = []

    for line in text.splitlines():
        goto = GOTO.match(line)
        if goto:
            visited.add(goto.group("room"))

        stats = STATS.match(line)
        if stats:
            stats_errors.append(int(stats.group("errors")))

        if "[museum][validate]" in line:
            result = RESULT.match(line)
            if result is None:
                malformed.append(line)
                continue
            room = result.group("room")
            status = result.group("status")
            check_lines.append(line.removeprefix("[museum][validate] "))
            counts[status] += 1
            if room in seen:
                seen[room] += 1
            else:
                errors.append(f"unexpected validation result from room {room}")

    missing_visits = [room for room in expected_rooms if room not in visited]
    if missing_visits:
        errors.append("rooms not visited: " + ", ".join(missing_visits))
    missing_results = [room for room in expected_rooms if seen.get(room, 0) == 0]
    if missing_results:
        errors.append("rooms with no validation result: " + ", ".join(missing_results))
    if not check_lines:
        errors.append("the log contains no validation results")
    if malformed:
        errors.append(
            f"{len(malformed)} malformed or aborted validation line(s), first: "
            + malformed[0][:180]
        )
    if not stats_errors:
        errors.append("the scripted run did not reach its final stats marker")
    elif stats_errors[-1]:
        errors.append(
            f"the final stats report {stats_errors[-1]} exhibit error(s)"
        )
    for marker in ERROR_MARKERS:
        occurrences = text.count(marker)
        if occurrences:
            errors.append(f"the log contains {occurrences} occurrence(s) of {marker}")
    if counts["FAIL"]:
        errors.append(f"{counts['FAIL']} validation check(s) failed")

    return Validation(
        pass_count=counts["PASS"],
        fail_count=counts["FAIL"],
        info_count=counts["INFO"],
        skip_count=counts["SKIP"],
        check_lines=tuple(check_lines),
        errors=tuple(dict.fromkeys(errors)),
    )


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--rooms", type=pathlib.Path, required=True)
    parser.add_argument("--log", type=pathlib.Path, required=True)
    parser.add_argument("--checks", type=pathlib.Path, required=True)
    return parser.parse_args()


def main() -> int:
    args = parse_args()
    try:
        rooms = [line.strip() for line in args.rooms.read_text().splitlines() if line.strip()]
        log_text = args.log.read_text(errors="replace")
    except OSError as error:
        print(f"renderer log validation failed: {error}", file=sys.stderr)
        return 2

    result = validate_log(rooms, log_text)
    try:
        args.checks.write_text("\n".join(result.check_lines) + ("\n" if result.check_lines else ""))
    except OSError as error:
        print(f"renderer log validation failed: {error}", file=sys.stderr)
        return 2

    print(
        f"{result.pass_count:3d} pass  {result.fail_count:3d} fail  "
        f"{result.info_count:2d} info  {result.skip_count:2d} skip"
    )
    for error in result.errors:
        print(f"  incomplete: {error}", file=sys.stderr)
    return 1 if result.errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
