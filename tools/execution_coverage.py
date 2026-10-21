#!/usr/bin/env python3
"""Execution evidence from gcov line counters: one museum process per room.

A tree configured with -DMUSEUM_COVERAGE=ON counts how often every line of the museum's own
code ran. Shared exhibit classes serve several rooms, so a single tour cannot say which room ran a
line; this tool therefore starts one process per room (enter the room, wait, operate every control,
wait, print stats, quit), collects the counters after each process and keeps the executed lines per
room. tools/audit_api_coverage.py then marks a reviewed implementation record as executed only when
every step of its path has an executed line inside its anchor, in the record's own room's process.

  tools/execution_coverage.py run --id RUN_ID [--build-dir build-probe] [--renderer OPENGLES3]
                                  [--frames 20] [room-key ...]

Writes audit/runs/RUN_ID.{script,log,lines.json.gz} and adds (or replaces) the run in
audit/execution_evidence.json. Rooms whose process fails, times out or logs an exhibit, draw or
script error are left out of the run's room list and reported under "failedRooms" with the reason;
they never count as executed.
"""

from __future__ import annotations

import argparse
import datetime
import gzip
import hashlib
import io
import json
import os
import pathlib
import platform
import re
import subprocess
import sys
from typing import Any

ROOT = pathlib.Path(__file__).resolve().parents[1]
LINES_SCHEMA = "museum-line-coverage/1"
OBJECT_DIRS = ("CMakeFiles/museum_core.dir", "CMakeFiles/cna-3d-museum.dir")


def sha256_bytes(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def gcda_files(build_dir: pathlib.Path) -> list[pathlib.Path]:
    found: list[pathlib.Path] = []
    for relative in OBJECT_DIRS:
        found.extend(sorted((build_dir / relative).rglob("*.gcda")))
    return found


def reset_counters(build_dir: pathlib.Path) -> None:
    for path in gcda_files(build_dir):
        path.unlink()


def ranges(lines: list[int]) -> str:
    parts = []
    start = previous = None
    for line in lines:
        if start is None:
            start = previous = line
        elif line == previous + 1:
            previous = line
        else:
            parts.append(f"{start}" if start == previous else f"{start}-{previous}")
            start = previous = line
    if start is not None:
        parts.append(f"{start}" if start == previous else f"{start}-{previous}")
    return ",".join(parts)


def parse_ranges(text: str) -> set[int]:
    lines: set[int] = set()
    for part in filter(None, text.split(",")):
        first, _, last = part.partition("-")
        lines.update(range(int(first), int(last or first) + 1))
    return lines


def collect_lines(build_dir: pathlib.Path) -> dict[str, list[int]]:
    """Executed lines of the museum's sources (repository-relative paths) from the current counters."""
    files = gcda_files(build_dir)
    if not files:
        raise RuntimeError(f"no gcov counters under {build_dir}: is it configured with -DMUSEUM_COVERAGE=ON?")
    executed: dict[str, set[int]] = {}
    for start in range(0, len(files), 64):
        batch = files[start : start + 64]
        completed = subprocess.run(
            ["gcov", "--json-format", "--stdout", *map(str, batch)],
            cwd=build_dir,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            check=False,
        )
        if completed.returncode != 0:
            raise RuntimeError("gcov failed: " + completed.stderr.decode(errors="replace")[:400])
        for document in completed.stdout.decode().splitlines():
            if not document.strip():
                continue
            payload = json.loads(document)
            cwd = pathlib.Path(payload.get("current_working_directory") or build_dir)
            for entry in payload.get("files", []):
                source = pathlib.Path(entry["file"])
                if not source.is_absolute():
                    source = cwd / source
                try:
                    relative = source.resolve().relative_to(ROOT).as_posix()
                except ValueError:
                    continue
                if not relative.startswith("src/"):
                    continue
                hit = {line["line_number"] for line in entry.get("lines", []) if line.get("count", 0) > 0}
                if hit:
                    executed.setdefault(relative, set()).update(hit)
    return {path: sorted(lines) for path, lines in sorted(executed.items())}


def load_scenarios() -> dict[str, str]:
    """Single-room scenarios from tests/scenarios: their scripts replace the sweep for that room, so the manual
    buttons a sweep never presses run too (each press must succeed, see room_failure)."""
    scripts: dict[str, str] = {}
    for path in sorted((ROOT / "tests" / "scenarios").glob("*.json")):
        script = json.loads(path.read_text(encoding="utf-8"))["script"]
        steps = [step for step in script.split(";") if step]
        rooms = [step[5:] for step in steps if step.startswith("goto:")]
        if len(rooms) == 1 and steps[0].startswith("goto:") and steps[-2:] == ["stats", "quit"] and rooms[0] not in scripts:
            # A scenario that only visits (its room checks on arrival) still sweeps the panels, as the default script does.
            if not any(step.startswith("press") for step in steps):
                script = ";".join(steps[:-2] + ["pressall", "wait:4", "stats", "quit"])
            scripts[rooms[0]] = script
    return scripts


def room_failure(status: int, log_text: str) -> str | None:
    if status == 124:
        return "timed out"
    if status != 0:
        return f"exit status {status}"
    stats = re.findall(r"^\[museum\]\[stats\].*$", log_text, re.MULTILINE)
    if len(stats) != 1:
        return "no final stats line"
    if not re.search(r"\bexhibit errors 0\b", stats[0]):
        return "exhibit errors: " + stats[0].split("exhibit errors", 1)[1].strip()
    script_errors = re.search(r"\bscript errors (\d+)\b", stats[0])
    if script_errors and script_errors.group(1) != "0":
        return f"script errors {script_errors.group(1)}"
    for marker in ("[museum][exhibit-error]", "[museum][draw-error]", "[museum][script-error]"):
        if marker in log_text:
            line = next(line for line in log_text.splitlines() if marker in line)
            return line[:200]
    # A room whose sweep finds no panel is a visit (recorded as such in roomsWithoutControls); otherwise a control must
    # have been operated.
    if not re.search(r"^\[museum\]\[script\] (pressall: [0-9]+ panel\(s\) exercised|press '[^']+': done)$", log_text, re.MULTILINE):
        return "no control was operated"
    return None


def without_controls(log_text: str) -> bool:
    return re.search(r"^\[museum\]\[script\] pressall: 0 panel\(s\) exercised$", log_text, re.MULTILINE) is not None


def source_hashes() -> dict[str, str]:
    """Every file the museum binary is built from, as it is now."""
    files = [path for path in (ROOT / "src").rglob("*") if path.is_file()] + [ROOT / "CMakeLists.txt"]
    return {path.relative_to(ROOT).as_posix(): sha256_bytes(path.read_bytes()) for path in sorted(files)}


def require_current_binary(build_dir: pathlib.Path) -> None:
    """The binary must be what the working tree builds now: the line numbers are read against these sources."""
    binary = build_dir / "bin" / "cna-3d-museum"
    if not binary.is_file():
        raise RuntimeError(f"{binary} is not built")
    # Ninja cannot answer this with a dry run here: the sources are globbed with CONFIGURE_DEPENDS, whose check is an
    # always-dirty edge a dry run cannot execute, so it always reports a CMake re-run and never looks at the objects.
    # The binary must instead be newer than every file it is built from (the same set source_hashes() records); CNA's
    # state is recorded separately by its binary hash.
    built = binary.stat().st_mtime
    newer = [path for path in [*(ROOT / "src").rglob("*"), ROOT / "CMakeLists.txt"] if path.is_file() and path.stat().st_mtime > built]
    if newer:
        raise RuntimeError(f"{binary} is older than the sources ({newer[0].relative_to(ROOT)} and {len(newer) - 1} more); rebuild it first")


def museum_lines(text: str) -> list[str]:
    return [line for line in text.splitlines() if line.startswith("[museum]")]


def git(*arguments: str, cwd: pathlib.Path = ROOT) -> str:
    completed = subprocess.run(["git", *arguments], cwd=cwd, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, check=False)
    return completed.stdout.strip() if completed.returncode == 0 else "unknown"


def cache_value(build_dir: pathlib.Path, name: str) -> str:
    cache = (build_dir / "CMakeCache.txt").read_text(errors="replace")
    match = re.search(rf"^{re.escape(name)}:[A-Z]+=(.*)$", cache, re.MULTILINE)
    return match.group(1) if match else ""


def run(arguments: argparse.Namespace) -> int:
    build_dir = (ROOT / arguments.build_dir).resolve()
    binary = build_dir / "bin" / "cna-3d-museum"
    library = build_dir / "cna" / "libcna.so"
    if cache_value(build_dir, "MUSEUM_COVERAGE") != "ON":
        raise RuntimeError(f"{arguments.build_dir} is not configured with -DMUSEUM_COVERAGE=ON")
    require_current_binary(build_dir)
    reset_counters(build_dir)   # before the binary runs at all: counters left by an older build have other checksums
    rooms = arguments.rooms or subprocess.run(
        [str(binary), "--list-rooms"], stdout=subprocess.PIPE, text=True, check=True
    ).stdout.split()
    # Provenance is taken before the first room runs and checked again at the end, so edits made during a long run
    # cannot be recorded as the sources of this binary.
    sources_at_start = source_hashes()
    museum_revision = git("rev-parse", "HEAD")
    museum_dirty = bool(git("status", "--porcelain", "--", "src", "CMakeLists.txt"))
    cna_root = pathlib.Path(cache_value(build_dir, "CNA_ROOT_DIR") or ROOT.parent / "cna")   # the CNA tree this binary was built from
    cna_revision = git("rev-parse", "HEAD", cwd=cna_root)
    museum_sha = sha256_bytes(binary.read_bytes())
    cna_sha = sha256_bytes(library.read_bytes())
    work = build_dir / "execution-coverage" / arguments.id
    work.mkdir(parents=True, exist_ok=True)
    per_room: dict[str, dict[str, str]] = {}
    passed: list[str] = []
    no_controls: list[str] = []
    failed: dict[str, str] = {}
    script_parts: list[str] = []
    log_lines: list[str] = []
    env = dict(os.environ, BUILD_DIR=arguments.build_dir, CNA_GRAPHICS_RENDERER=arguments.renderer,
               MUSEUM_TIMEOUT=str(arguments.timeout))
    scenarios = load_scenarios() if arguments.scenarios else {}
    for index, room in enumerate(rooms, 1):
        script = scenarios.get(room) or f"goto:{room};wait:{arguments.frames};pressall;wait:4;stats;quit"
        reset_counters(build_dir)
        room_log = work / f"{room}.log"
        env["MUSEUM_LOG"] = str(room_log)
        completed = subprocess.run(
            [str(ROOT / "tools" / "run.sh"), script, "--no-audio"],
            cwd=ROOT, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False,
        )
        text = room_log.read_text(errors="replace") if room_log.is_file() else ""
        reason = room_failure(completed.returncode, text)
        if reason is not None:
            failed[room] = reason
            print(f"[{index}/{len(rooms)}] {room}: FAILED ({reason})", flush=True)
            continue
        lines = collect_lines(build_dir)
        per_room[room] = {path: ranges(numbers) for path, numbers in lines.items()}
        passed.append(room)
        if without_controls(text):
            no_controls.append(room)
        script_parts.append(script)
        log_lines.extend(museum_lines(text))
        print(f"[{index}/{len(rooms)}] {room}: ok, {sum(len(v) for v in lines.values())} lines", flush=True)
    reset_counters(build_dir)
    if not passed:
        raise RuntimeError("no room completed cleanly")
    changed = sorted(path for path, digest in source_hashes().items() if sources_at_start.get(path) != digest)
    changed += sorted(path for path in sources_at_start if not (ROOT / path).is_file())
    if changed or sha256_bytes(binary.read_bytes()) != museum_sha or sha256_bytes(library.read_bytes()) != cna_sha:
        raise RuntimeError("the sources or binaries changed during the run (" + ", ".join(changed[:5] or ["binary"]) + "); nothing was recorded")
    sources = sorted({path for coverage in per_room.values() for path in coverage})
    lines_payload = {
        "schema": LINES_SCHEMA,
        "buildDir": arguments.build_dir,
        "renderer": arguments.renderer,
        "sources": {path: sources_at_start[path] for path in sources},
        "rooms": per_room,
    }
    runs_dir = ROOT / "audit" / "runs"
    runs_dir.mkdir(parents=True, exist_ok=True)
    script_path = runs_dir / f"{arguments.id}.script"
    log_path = runs_dir / f"{arguments.id}.log"
    lines_path = runs_dir / f"{arguments.id}.lines.json.gz"
    script_path.write_text(";".join(script_parts) + "\n", encoding="utf-8")
    log_path.write_text("\n".join(log_lines) + "\n", encoding="utf-8")
    raw = io.BytesIO()
    with gzip.GzipFile(fileobj=raw, mode="wb", mtime=0, compresslevel=9) as handle:
        handle.write((json.dumps(lines_payload, sort_keys=True, separators=(",", ":")) + "\n").encode())
    lines_path.write_bytes(raw.getvalue())
    entry = {
        "id": arguments.id,
        "recordedAt": datetime.datetime.now().astimezone().replace(microsecond=0).isoformat(),
        "renderer": arguments.renderer,
        "platform": f"{platform.system()} {platform.release()} {platform.machine()}, "
                    + ("physical GPU, no display (SDL offscreen)" if os.environ.get("MUSEUM_GPU") else "private virtual display"),
        "command": f"tools/execution_coverage.py run --id {arguments.id} --build-dir {arguments.build_dir} "
                   f"--renderer {arguments.renderer} --frames {arguments.frames}" + (" --scenarios" if arguments.scenarios else "")
                   + ("" if not arguments.rooms else " " + " ".join(arguments.rooms)),
        "configuration": {
            "CMAKE_BUILD_TYPE": cache_value(build_dir, "CMAKE_BUILD_TYPE"),
            "CNA_DIAGNOSTICS": cache_value(build_dir, "CNA_DIAGNOSTICS"),
            "CNA_GRAPHICS_RENDERERS": cache_value(build_dir, "CNA_GRAPHICS_RENDERERS"),
            "MUSEUM_COVERAGE": "ON",
            "MUSEUM_NETWORKING": cache_value(build_dir, "MUSEUM_NETWORKING"),
            "MUSEUM_PIPELINE": cache_value(build_dir, "MUSEUM_PIPELINE"),
            "audio": "disabled (--no-audio)",
        },
        "interaction": "per-room-coverage",
        "buildDir": arguments.build_dir,
        "museumRevision": museum_revision,
        "museumDirty": museum_dirty,
        "cnaRevision": cna_revision,
        "museumBinarySha256": museum_sha,
        "cnaBinarySha256": cna_sha,
        "rooms": passed,
        "roomsWithoutControls": no_controls,
        "scenarioRooms": sorted(room for room in passed if room in scenarios),
        "failedRooms": failed,
        "script": {"path": script_path.relative_to(ROOT).as_posix(), "sha256": sha256_bytes(script_path.read_bytes())},
        "log": {"path": log_path.relative_to(ROOT).as_posix(), "sha256": sha256_bytes(log_path.read_bytes())},
        "lineCoverage": {"path": lines_path.relative_to(ROOT).as_posix(), "sha256": sha256_bytes(lines_path.read_bytes())},
    }
    manifest_path = ROOT / "audit" / "execution_evidence.json"
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))
    manifest["runs"] = [run for run in manifest["runs"] if run.get("id") != arguments.id] + [entry]
    manifest_path.write_text(json.dumps(manifest, indent=2, ensure_ascii=False) + "\n", encoding="utf-8")
    print(f"{len(passed)} room(s) recorded, {len(failed)} failed; run {arguments.id} written to {manifest_path.relative_to(ROOT)}")
    for room, reason in failed.items():
        print(f"  failed: {room}: {reason}")
    return 0 if not failed else 3


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)
    run_parser = sub.add_parser("run")
    run_parser.add_argument("--id", required=True)
    run_parser.add_argument("--build-dir", default="build-probe")
    run_parser.add_argument("--renderer", default="OPENGLES3")
    run_parser.add_argument("--frames", type=int, default=20)
    run_parser.add_argument("--timeout", type=int, default=180)
    run_parser.add_argument("--scenarios", action="store_true", help="use tests/scenarios scripts for the rooms they cover")
    run_parser.add_argument("rooms", nargs="*")
    arguments = parser.parse_args()
    if not re.fullmatch(r"[A-Za-z0-9._-]+", arguments.id):
        parser.error("--id may contain letters, digits, '.', '_' and '-' only")
    try:
        return run(arguments)
    except RuntimeError as error:
        print(f"execution coverage error: {error}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    sys.exit(main())
