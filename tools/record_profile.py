#!/usr/bin/env python3
"""Records one test profile's result for tools/profile_summary.py.

    tools/record_profile.py NAME --passed N --failed N [--skipped N] [--unavailable N] [--not-run N] [--note TEXT]

Writes ${BUILD_DIR:-build}/profiles/NAME.json with the counts, the date and the SHA-256 of the museum binary and of
libcna the run used, so the summary can tell a current result from one that belongs to an older build. The runners
(check_all.sh, scenarios.py, renderer_check.sh, devices/audio/voice/net/online checks, web_smoke.sh) call it as their
last step; nothing else should.
"""

from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import os
import pathlib
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]


def sha256(path: pathlib.Path) -> str | None:
    if not path.is_file():
        return None
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1 << 20), b""):
            digest.update(block)
    return digest.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("name")
    parser.add_argument("--passed", type=int, required=True)
    parser.add_argument("--failed", type=int, required=True)
    parser.add_argument("--skipped", type=int, default=0)
    parser.add_argument("--unavailable", type=int, default=0)
    parser.add_argument("--not-run", type=int, default=0)
    parser.add_argument("--note", default="")
    parser.add_argument("--binary", default="", help="the program the run used, when it is not <build dir>/bin/cna-3d-museum")
    parser.add_argument("--library", default="", help="the CNA library it used, when not next to the binary's build tree")
    arguments = parser.parse_args()
    build_dir = ROOT / os.environ.get("BUILD_DIR", "build")
    binary = pathlib.Path(arguments.binary or os.environ.get("MUSEUM_BIN") or build_dir / "bin" / "cna-3d-museum")
    binary = binary if binary.is_absolute() else ROOT / binary
    library = pathlib.Path(arguments.library) if arguments.library else binary.parent.parent / "cna" / "libcna.so"
    library = library if library.is_absolute() else ROOT / library
    record = {
        "profile": arguments.name,
        "recordedAt": datetime.datetime.now().astimezone().replace(microsecond=0).isoformat(),
        "buildDir": os.environ.get("BUILD_DIR", "build"),
        "binary": binary.relative_to(ROOT).as_posix() if binary.is_relative_to(ROOT) else str(binary),
        "library": library.relative_to(ROOT).as_posix() if library.is_relative_to(ROOT) else str(library),
        "museumBinarySha256": sha256(binary),
        "cnaBinarySha256": sha256(library),
        "passed": arguments.passed,
        "failed": arguments.failed,
        "skipped": arguments.skipped,
        "unavailable": arguments.unavailable,
        "notRun": arguments.not_run,
        "note": arguments.note,
    }
    out = build_dir / "profiles"
    out.mkdir(parents=True, exist_ok=True)
    (out / f"{arguments.name}.json").write_text(json.dumps(record, indent=1) + "\n", encoding="utf-8")
    return 0


if __name__ == "__main__":
    sys.exit(main())
