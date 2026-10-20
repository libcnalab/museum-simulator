#!/usr/bin/env python3
"""Resolve museum catalogue claims against the generated CNA public API inventory.

This is deliberately conservative. A catalogue name linked to a declaration is still
only a catalogue link: implementation, execution and correctness remain UNVERIFIED
until later evidence records prove each stage.
"""

from __future__ import annotations

import argparse
import collections
import dataclasses
import fnmatch
import gzip
import hashlib
import json
import pathlib
import re
import subprocess
import sys
from typing import Any


SCHEMA = "museum-cna-coverage-evidence/1"
IMPLEMENTATION_SCHEMA = "museum-cna-implementation-evidence/1"
EXECUTION_SCHEMA = "museum-cna-execution-evidence/1"
AREA_SCHEMA = "museum-cna-area-classification/1"
LINE_COVERAGE_SCHEMA = "museum-line-coverage/1"
NON_DECLARATION_CLAIMS = {
    "ApiOrigin": "museum catalogue type, not CNA API",
    "CNAEXT": "compile-time API family label, not a declaration",
    "XNA 4.0": "compatibility family label, not a declaration",
    "cna-content": "command-line tool identity, not a public declaration",
}
EXTERNAL_DEPENDENCY_PREFIXES = ("System.", "System::")
TYPE_KINDS = {"class", "struct", "union", "enum", "alias"}


@dataclasses.dataclass(frozen=True)
class ExhibitSource:
    factory: str
    source: str


def repository_root() -> pathlib.Path:
    return pathlib.Path(__file__).resolve().parents[1]


def load_claims(binary: pathlib.Path) -> tuple[dict[str, Any], str]:
    completed = subprocess.run(
        [str(binary), "--coverage-json"],
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        text=True,
        check=False,
    )
    if completed.returncode != 0:
        raise RuntimeError(
            f"{binary} --coverage-json exited {completed.returncode}: {completed.stderr.strip()}"
        )
    payload = json.loads(completed.stdout)
    if payload.get("schema") != "museum-api-claims/1":
        raise RuntimeError("museum returned an unsupported coverage-claims schema")
    return payload, hashlib.sha256(completed.stdout.encode("utf-8")).hexdigest()


def discover_exhibit_sources(root: pathlib.Path) -> dict[str, ExhibitSource]:
    registry_path = root / "src" / "Exhibits" / "ExhibitRegistry.cpp"
    if not registry_path.is_file():
        raise RuntimeError(f"exhibit registry is missing: {registry_path}")
    registry_text = registry_path.read_text(encoding="utf-8")
    entries = re.findall(r'\{\s*"([^"]+)"\s*,\s*(Make[A-Za-z0-9_]+)\s*\}', registry_text)
    if not entries:
        raise RuntimeError("exhibit registry contains no factory entries")
    by_factory: collections.defaultdict[str, list[str]] = collections.defaultdict(list)
    definition_pattern = re.compile(
        r"\bstd::unique_ptr\s*<\s*Exhibit\s*>\s+(Make[A-Za-z0-9_]+)\s*\("
    )
    for path in sorted((root / "src" / "Exhibits").rglob("*.cpp")):
        relative = path.relative_to(root).as_posix()
        for factory in definition_pattern.findall(path.read_text(encoding="utf-8")):
            by_factory[factory].append(relative)

    result: dict[str, ExhibitSource] = {}
    for exhibit, factory in entries:
        if exhibit in result:
            raise RuntimeError(f"duplicate exhibit registry entry: {exhibit}")
        definitions = by_factory.get(factory, [])
        if len(definitions) != 1:
            raise RuntimeError(
                f"factory {factory} for exhibit {exhibit} has {len(definitions)} definitions"
            )
        result[exhibit] = ExhibitSource(factory, definitions[0])
    return result


def exhibit_source_sha256(
    root: pathlib.Path, sources: dict[str, ExhibitSource]
) -> str:
    paths = {"src/Exhibits/ExhibitRegistry.cpp"}
    paths.update(source.source for source in sources.values())
    digest = hashlib.sha256()
    for relative in sorted(paths):
        encoded = relative.encode("utf-8")
        body = (root / relative).read_bytes()
        digest.update(len(encoded).to_bytes(8, "big"))
        digest.update(encoded)
        digest.update(len(body).to_bytes(8, "big"))
        digest.update(body)
    return digest.hexdigest()


def load_implementation_manifest(path: pathlib.Path) -> tuple[dict[str, Any], str]:
    if not path.is_file():
        raise RuntimeError(f"implementation evidence manifest is missing: {path}")
    raw = path.read_bytes()
    payload = json.loads(raw.decode("utf-8"))
    if payload.get("schema") != IMPLEMENTATION_SCHEMA:
        raise RuntimeError("unsupported implementation evidence schema")
    if not isinstance(payload.get("records"), list):
        raise RuntimeError("implementation evidence records must be a list")
    return payload, hashlib.sha256(raw).hexdigest()


def sha256_file(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def load_execution_manifest(path: pathlib.Path) -> tuple[dict[str, Any], str]:
    if not path.is_file():
        raise RuntimeError(f"execution evidence manifest is missing: {path}")
    raw = path.read_bytes()
    payload = json.loads(raw.decode("utf-8"))
    if payload.get("schema") != EXECUTION_SCHEMA:
        raise RuntimeError("unsupported execution evidence schema")
    if (
        not isinstance(payload.get("runs"), list)
        or not isinstance(payload.get("records"), list)
        or not isinstance(payload.get("oracles", []), list)
    ):
        raise RuntimeError("execution evidence runs, records and oracles must be lists")
    return payload, hashlib.sha256(raw).hexdigest()


def _repository_file(root: pathlib.Path, relative: str, description: str) -> pathlib.Path:
    path = (root / relative).resolve()
    try:
        path.relative_to(root.resolve())
    except ValueError as error:
        raise RuntimeError(f"{description} escapes the repository") from error
    if not path.is_file():
        raise RuntimeError(f"{description} is missing: {relative}")
    return path


def _optional_sha256(path: pathlib.Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest() if path.is_file() else "missing"


def _parse_line_ranges(text: str) -> set[int]:
    lines: set[int] = set()
    for part in filter(None, text.split(",")):
        first, _, last = part.partition("-")
        lines.update(range(int(first), int(last or first) + 1))
    return lines


def executed_by_line_coverage(
    implementation_records: list[dict[str, Any]], rooms_coverage: dict[str, dict[str, str]]
) -> list[str]:
    """Reviewed records whose every path step ran, judged in the process of the record's own room.

    A step ran when at least one line its anchor spans has a nonzero gcov count. Anchors on lines
    without executable code (a member declaration, an include) can therefore never count as
    executed through this route.
    """
    executed = []
    for record in implementation_records:
        coverage = rooms_coverage.get(record["room"])
        if coverage is None:
            continue
        ran = True
        for step in record["path"]:
            hit = _parse_line_ranges(coverage.get(step["source"], ""))
            span = range(step["line"], step["line"] + step["anchor"].count("\n") + 1)
            if not any(line in hit for line in span):
                ran = False
                break
        if ran:
            executed.append(record["id"])
    return sorted(executed)


def resolve_execution_evidence(
    root: pathlib.Path,
    manifest: dict[str, Any],
    implementation_records: list[dict[str, Any]],
    current_museum_binary_sha256: str,
    current_cna_binary_sha256: str,
) -> tuple[list[dict[str, Any]], list[dict[str, Any]], list[dict[str, Any]], str]:
    implementation_by_id = {record["id"]: record for record in implementation_records}
    coverage_by_run: dict[str, dict[str, dict[str, str]]] = {}
    seen_runs: set[str] = set()
    resolved_runs = []
    digest = hashlib.sha256()
    for raw in manifest["runs"]:
        run_id = raw.get("id")
        if not isinstance(run_id, str) or not run_id or run_id in seen_runs:
            raise RuntimeError(f"invalid or duplicate execution run id: {run_id!r}")
        seen_runs.add(run_id)
        rooms = raw.get("rooms")
        if not isinstance(rooms, list) or not rooms or len(set(rooms)) != len(rooms):
            raise RuntimeError(f"execution run {run_id} has invalid rooms")
        artifacts = {}
        bodies = {}
        for kind in ("script", "log"):
            entry = raw.get(kind)
            if not isinstance(entry, dict):
                raise RuntimeError(f"execution run {run_id} has no {kind} artifact")
            relative = entry.get("path")
            expected_sha = entry.get("sha256")
            if not isinstance(relative, str) or not isinstance(expected_sha, str):
                raise RuntimeError(f"execution run {run_id} has invalid {kind} metadata")
            path = _repository_file(root, relative, f"execution run {run_id} {kind}")
            body = path.read_bytes()
            actual_sha = hashlib.sha256(body).hexdigest()
            if actual_sha != expected_sha:
                raise RuntimeError(f"execution run {run_id} {kind} SHA-256 mismatch")
            artifacts[kind] = {"path": relative, "sha256": actual_sha}
            bodies[kind] = body
            digest.update(kind.encode("ascii") + b"\0" + relative.encode("utf-8") + b"\0" + body)
        script = bodies["script"].decode("utf-8").strip()
        script_tokens = [token.strip() for token in script.split(";") if token.strip()]
        scripted_rooms = [token[5:] for token in script_tokens if token.startswith("goto:")]
        if scripted_rooms != rooms:
            raise RuntimeError(f"execution run {run_id} script rooms do not match metadata")
        interaction = raw.get("interaction", "pressall")
        if interaction == "per-room-coverage":
            # One process per room: each room's steps operate its controls, by the sweep or by named presses
            # (a scenario); a press that finds no control is a script error, which the log check refuses.
            segments: list[list[str]] = []
            for token in script_tokens:
                if token.startswith("goto:"):
                    segments.append([])
                elif segments:
                    segments[-1].append(token)
            if any(not any(t == "pressall" or t.startswith("press:") for t in segment) for segment in segments):
                raise RuntimeError(f"execution run {run_id} has a room whose controls are never operated")
        elif interaction == "pressall":
            if script_tokens.count("pressall") != len(rooms):
                raise RuntimeError(f"execution run {run_id} does not press all controls in every room")
        elif interaction == "scripted-controls":
            scripted_controls = [
                token[6:].rsplit("=", 1)[0]
                for token in script_tokens
                if token.startswith("press:")
            ]
            if not scripted_controls or scripted_controls != raw.get("controls"):
                raise RuntimeError(f"execution run {run_id} scripted controls do not match metadata")
        else:
            raise RuntimeError(f"execution run {run_id} has invalid interaction mode {interaction!r}")
        log = bodies["log"].decode("utf-8")
        logged_rooms = re.findall(r"^\[museum\]\[script\] goto (\S+)$", log, re.MULTILINE)
        if logged_rooms != rooms:
            raise RuntimeError(f"execution run {run_id} log rooms do not match metadata")
        if interaction == "per-room-coverage":
            swept = sum(1 for segment in segments if "pressall" in segment)
            # Each swept room's sweep is logged once; a room whose sweep found no panel at all is only a visit and must
            # be declared as such in roomsWithoutControls.
            sweeps: list[tuple[str, int]] = []
            current_room = None
            for line in log.splitlines():
                goto = re.match(r"^\[museum\]\[script\] goto (\S+)$", line)
                if goto:
                    current_room = goto.group(1)
                swept_line = re.match(r"^\[museum\]\[script\] pressall: ([0-9]+) panel\(s\) exercised$", line)
                if swept_line:
                    sweeps.append((current_room, int(swept_line.group(1))))
            if len(sweeps) != swept:
                raise RuntimeError(f"execution run {run_id} lacks a pressall result for every swept room")
            if sorted(room for room, panels in sweeps if panels == 0) != sorted(raw.get("roomsWithoutControls", [])):
                raise RuntimeError(f"execution run {run_id} roomsWithoutControls does not match the rooms whose sweep found no panel")
        elif interaction == "pressall":
            pressed = re.findall(
                r"^\[museum\]\[script\] pressall: ([1-9][0-9]*) panel\(s\) exercised$",
                log,
                re.MULTILINE,
            )
            if len(pressed) != len(rooms):
                raise RuntimeError(f"execution run {run_id} lacks a successful pressall for every room")
        else:
            logged_controls = re.findall(
                r"^\[museum\]\[script\] press '([^']+)': done$", log, re.MULTILINE
            )
            if logged_controls != scripted_controls:
                raise RuntimeError(f"execution run {run_id} lacks its scripted control actions")
        stats = re.findall(r"^\[museum\]\[stats\].*$", log, re.MULTILINE)
        # A per-room-coverage run is one process per room, so it has one stats line per room.
        expected_stats = len(rooms) if interaction == "per-room-coverage" else 1
        if len(stats) != expected_stats or not all(re.search(r"\bexhibit errors 0\b", line) for line in stats):
            raise RuntimeError(f"execution run {run_id} lacks clean final stats")
        for line in stats:
            script_errors = re.search(r"\bscript errors (\d+)\b", line)
            if script_errors and script_errors.group(1) != "0":
                raise RuntimeError(f"execution run {run_id} reports script errors")
        forbidden = (
            "[museum][exhibit-error]",
            "[museum][draw-error]",
            "[museum][script-error]",
            "pressall: exception",
        )
        if any(marker in log for marker in forbidden):
            raise RuntimeError(f"execution run {run_id} contains an execution error")
        museum_sha = raw.get("museumBinarySha256")
        cna_sha = raw.get("cnaBinarySha256")
        if not isinstance(museum_sha, str) or not isinstance(cna_sha, str):
            raise RuntimeError(f"execution run {run_id} lacks binary fingerprints")
        build_dir = raw.get("buildDir")
        current_museum, current_cna = current_museum_binary_sha256, current_cna_binary_sha256
        if isinstance(build_dir, str) and build_dir not in ("", "build"):
            current_museum = _optional_sha256(root / build_dir / "bin" / "cna-3d-museum")
            current_cna = _optional_sha256(root / build_dir / "cna" / "libcna.so")
        line_coverage = None
        sources_current = True
        if interaction == "per-room-coverage":
            entry = raw.get("lineCoverage")
            if not isinstance(entry, dict) or not isinstance(entry.get("path"), str):
                raise RuntimeError(f"execution run {run_id} has no line-coverage artifact")
            path = _repository_file(root, entry["path"], f"execution run {run_id} line coverage")
            body = path.read_bytes()
            if hashlib.sha256(body).hexdigest() != entry.get("sha256"):
                raise RuntimeError(f"execution run {run_id} line coverage SHA-256 mismatch")
            digest.update(b"lines\0" + entry["path"].encode("utf-8") + b"\0" + body)
            line_coverage = json.loads(gzip.decompress(body).decode("utf-8"))
            if line_coverage.get("schema") != LINE_COVERAGE_SCHEMA or sorted(line_coverage.get("rooms", {})) != sorted(rooms):
                raise RuntimeError(f"execution run {run_id} line coverage does not match its rooms")
            # Line numbers only mean something against the sources the binary was built from.
            for source, expected in line_coverage.get("sources", {}).items():
                if _optional_sha256(root / source) != expected:
                    sources_current = False
                    break
        resolved_runs.append(
            {
                "id": run_id,
                "recordedAt": raw.get("recordedAt"),
                "renderer": raw.get("renderer"),
                "platform": raw.get("platform"),
                "command": raw.get("command"),
                "configuration": raw.get("configuration", {}),
                "interaction": interaction,
                "controls": raw.get("controls", []),
                "museumRevision": raw.get("museumRevision"),
                "cnaRevision": raw.get("cnaRevision"),
                "museumBinarySha256": museum_sha,
                "cnaBinarySha256": cna_sha,
                "currentBuild": (
                    museum_sha == current_museum
                    and cna_sha == current_cna
                    and sources_current
                ),
                "buildDir": build_dir or "build",
                "rooms": rooms,
                "failedRooms": raw.get("failedRooms", {}),
                "script": artifacts["script"],
                "log": artifacts["log"],
                "finalStats": stats[-1],
            }
        )
        if line_coverage is not None:
            coverage_by_run[run_id] = line_coverage["rooms"]
    runs_by_id = {run["id"]: run for run in resolved_runs}
    seen_records: set[str] = set()
    resolved_records = []
    for raw in manifest["records"]:
        record_id = raw.get("id")
        if not isinstance(record_id, str) or not record_id or record_id in seen_records:
            raise RuntimeError(f"invalid or duplicate execution evidence id: {record_id!r}")
        seen_records.add(record_id)
        run = runs_by_id.get(raw.get("run"))
        if run is None:
            raise RuntimeError(f"execution evidence {record_id} names an unknown run")
        implementation_ids = raw.get("implementationRecords")
        if (
            not isinstance(implementation_ids, list)
            or not implementation_ids
            or len(set(implementation_ids)) != len(implementation_ids)
        ):
            raise RuntimeError(f"execution evidence {record_id} has invalid implementation records")
        unknown = sorted(set(implementation_ids) - implementation_by_id.keys())
        if unknown:
            raise RuntimeError(
                f"execution evidence {record_id} names unknown implementation records: "
                + ", ".join(unknown)
            )
        outside_run = sorted(
            identity
            for identity in implementation_ids
            if implementation_by_id[identity]["room"] not in run["rooms"]
        )
        if outside_run:
            raise RuntimeError(
                f"execution evidence {record_id} links records outside the run rooms: "
                + ", ".join(outside_run)
            )
        note = raw.get("note")
        if not isinstance(note, str) or not note.strip():
            raise RuntimeError(f"execution evidence {record_id} needs a review note")
        resolved_records.append(
            {
                "id": record_id,
                "run": run["id"],
                "currentBuild": run["currentBuild"],
                "implementationRecords": implementation_ids,
                "note": note,
            }
        )
    for run_id, rooms_coverage in coverage_by_run.items():
        executed = executed_by_line_coverage(implementation_records, rooms_coverage)
        if executed:
            run = runs_by_id[run_id]
            resolved_records.append(
                {
                    "id": f"{run_id}-line-coverage",
                    "run": run_id,
                    "currentBuild": run["currentBuild"],
                    "implementationRecords": executed,
                    "note": "Derived from gcov line counters: every step of each listed path has an executed "
                    "line inside its anchor, in the process that visited the record's own room.",
                }
            )
    executed_implementation_ids = {
        implementation_id
        for record in resolved_records
        for implementation_id in record["implementationRecords"]
    }
    seen_oracles: set[str] = set()
    resolved_oracles = []
    for raw in manifest.get("oracles", []):
        oracle_id = raw.get("id")
        if not isinstance(oracle_id, str) or not oracle_id or oracle_id in seen_oracles:
            raise RuntimeError(f"invalid or duplicate correctness oracle id: {oracle_id!r}")
        seen_oracles.add(oracle_id)
        run = runs_by_id.get(raw.get("run"))
        if run is None:
            raise RuntimeError(f"correctness oracle {oracle_id} names an unknown run")
        implementation_ids = raw.get("implementationRecords")
        if (
            not isinstance(implementation_ids, list)
            or not implementation_ids
            or len(set(implementation_ids)) != len(implementation_ids)
        ):
            raise RuntimeError(f"correctness oracle {oracle_id} has invalid implementation records")
        unknown = sorted(set(implementation_ids) - implementation_by_id.keys())
        if unknown:
            raise RuntimeError(
                f"correctness oracle {oracle_id} names unknown implementation records: "
                + ", ".join(unknown)
            )
        unexecuted = sorted(set(implementation_ids) - executed_implementation_ids)
        if unexecuted:
            raise RuntimeError(
                f"correctness oracle {oracle_id} lacks execution evidence for: "
                + ", ".join(unexecuted)
            )
        witness = raw.get("witness")
        expectation = raw.get("expectation")
        note = raw.get("note")
        if not all(isinstance(value, str) and value.strip() for value in (witness, expectation, note)):
            raise RuntimeError(f"correctness oracle {oracle_id} has invalid text fields")
        log = (root / run["log"]["path"]).read_text(encoding="utf-8")
        occurrences = log.count(witness)
        if occurrences != 1:
            raise RuntimeError(
                f"correctness oracle {oracle_id} witness occurs {occurrences} times"
            )
        resolved_oracles.append(
            {
                "id": oracle_id,
                "run": run["id"],
                "currentBuild": run["currentBuild"],
                "implementationRecords": implementation_ids,
                "witness": witness,
                "expectation": expectation,
                "note": note,
            }
        )
    return (
        sorted(resolved_runs, key=lambda item: item["id"]),
        sorted(resolved_records, key=lambda item: item["id"]),
        sorted(resolved_oracles, key=lambda item: item["id"]),
        digest.hexdigest(),
    )


def implementation_evidence_inputs_sha256(
    root: pathlib.Path,
    manifest_sha256: str,
    records: list[dict[str, Any]],
) -> str:
    """Hash the manifest and every source file on a reviewed path.

    Anchors make moved or deleted call sites fail loudly.  This additional hash also
    makes edits elsewhere in an evidence-bearing source visible in generated output.
    """
    digest = hashlib.sha256()
    digest.update(manifest_sha256.encode("ascii"))
    paths = sorted({step["source"] for record in records for step in record["path"]})
    for relative in paths:
        encoded = relative.encode("utf-8")
        body = (root / relative).read_bytes()
        digest.update(len(encoded).to_bytes(8, "big"))
        digest.update(encoded)
        digest.update(len(body).to_bytes(8, "big"))
        digest.update(body)
    return digest.hexdigest()


def resolve_implementation_records(
    root: pathlib.Path,
    manifest: dict[str, Any],
    claim_rows: list[dict[str, Any]],
    exhibit_sources: dict[str, ExhibitSource],
) -> list[dict[str, Any]]:
    claims = {row["claim"]: row for row in claim_rows}
    seen_ids: set[str] = set()
    resolved = []
    allowed_kinds = {
        "direct-call",
        "direct-read",
        "direct-use",
        "indirect-framework",
        "indirect-runtime",
    }
    for raw in manifest["records"]:
        record_id = raw.get("id")
        if not isinstance(record_id, str) or not record_id or record_id in seen_ids:
            raise RuntimeError(f"invalid or duplicate implementation evidence id: {record_id!r}")
        seen_ids.add(record_id)
        claim_name = raw.get("claim")
        claim = claims.get(claim_name)
        if claim is None:
            raise RuntimeError(f"implementation evidence {record_id} names unknown claim {claim_name!r}")
        room_key = raw.get("room")
        room = next((item for item in claim["rooms"] if item["key"] == room_key), None)
        if room is None:
            raise RuntimeError(
                f"implementation evidence {record_id} names room {room_key!r} that does not claim {claim_name!r}"
            )
        kind = raw.get("kind")
        if kind not in allowed_kinds:
            raise RuntimeError(f"implementation evidence {record_id} has invalid kind {kind!r}")
        symbol_ids = raw.get("symbols")
        if (
            not isinstance(symbol_ids, list)
            or len(set(symbol_ids)) != len(symbol_ids)
            or (not symbol_ids and claim["resolution"] != "external-dependency")
        ):
            raise RuntimeError(f"implementation evidence {record_id} has invalid symbol IDs")
        unknown_symbols = sorted(set(symbol_ids) - set(claim["symbols"]))
        if unknown_symbols:
            raise RuntimeError(
                f"implementation evidence {record_id} links symbols outside claim {claim_name}: "
                + ", ".join(unknown_symbols)
            )
        conditions = raw.get("conditions")
        if not isinstance(conditions, list) or any(not isinstance(item, str) for item in conditions):
            raise RuntimeError(f"implementation evidence {record_id} has invalid conditions")
        note = raw.get("note")
        if not isinstance(note, str) or not note.strip():
            raise RuntimeError(f"implementation evidence {record_id} needs a review note")
        raw_path = raw.get("path")
        if not isinstance(raw_path, list) or not raw_path:
            raise RuntimeError(f"implementation evidence {record_id} has no source path")
        path = []
        for step in raw_path:
            source = step.get("source") if isinstance(step, dict) else None
            anchor = step.get("anchor") if isinstance(step, dict) else None
            if not isinstance(source, str) or not isinstance(anchor, str) or not anchor:
                raise RuntimeError(f"implementation evidence {record_id} has an invalid path step")
            source_path = (root / source).resolve()
            try:
                source_path.relative_to(root.resolve())
            except ValueError as error:
                raise RuntimeError(f"implementation evidence {record_id} escapes the repository") from error
            if not source_path.is_file():
                raise RuntimeError(f"implementation evidence {record_id} source is missing: {source}")
            text = source_path.read_text(encoding="utf-8")
            occurrences = text.count(anchor)
            if occurrences != 1:
                raise RuntimeError(
                    f"implementation evidence {record_id} anchor occurs {occurrences} times in {source}"
                )
            line = text[: text.index(anchor)].count("\n") + 1
            path.append({"source": source, "line": line, "anchor": anchor})
        exhibit_source = exhibit_sources[room["exhibit"]].source
        path_sources = {step["source"] for step in path}
        if exhibit_source not in path_sources:
            raise RuntimeError(
                f"implementation evidence {record_id} never reaches room source {exhibit_source}"
            )
        if not kind.startswith("indirect-") and len(path) != 1:
            raise RuntimeError(f"direct implementation evidence {record_id} must have one path step")
        resolved.append(
            {
                "id": record_id,
                "claim": claim_name,
                "room": room_key,
                "kind": kind,
                "symbols": symbol_ids,
                "path": path,
                "conditions": conditions,
                "note": note,
            }
        )
    return sorted(resolved, key=lambda item: item["id"])


def _strip_generic_arguments(value: str) -> str:
    previous = None
    while previous != value:
        previous = value
        value = re.sub(r"<[^<>]*>", "", value)
    return value


def split_claim(claim: str) -> tuple[str, list[str]]:
    call = re.fullmatch(r"(.+?)\((.*)\)", claim)
    if call is None:
        return _strip_generic_arguments(claim), []
    arguments = [part.strip() for part in call.group(2).split(",") if part.strip()]
    return _strip_generic_arguments(call.group(1)), arguments


def claim_suffixes(claim: str) -> tuple[list[str], list[str]]:
    base, arguments = split_claim(claim)
    qualified = base.replace(".", "::")
    suffixes = [qualified]
    if "." in base:
        owner, member = base.rsplit(".", 1)
        owner = owner.replace(".", "::")
        suffixes.extend(
            (
                f"{owner}::get{member}Property",
                f"{owner}::set{member}Property",
                f"{owner}::get{member}",
                f"{owner}::set{member}",
                f"{owner}::is{member}",
            )
        )
    else:
        # A constructor is represented as Namespace::Type::Type by Doxygen.
        unqualified = qualified.rsplit("::", 1)[-1]
        suffixes.append(f"{qualified}::{unqualified}")
    return list(dict.fromkeys(suffixes)), arguments


def _ends_with_qualified(name: str, suffix: str) -> bool:
    return name == suffix or name.endswith("::" + suffix)


def _argument_tokens(arguments: list[str]) -> list[str]:
    tokens = []
    for argument in arguments:
        cleaned = argument.replace("[]", "")
        tokens.extend(re.findall(r"[A-Za-z_][A-Za-z0-9_]*", cleaned))
    ignored = {"const", "unsigned", "signed", "byte"}
    return [token for token in tokens if token not in ignored]


def build_symbol_suffix_index(
    symbols: list[dict[str, Any]],
) -> dict[str, list[dict[str, Any]]]:
    index: collections.defaultdict[str, list[dict[str, Any]]] = collections.defaultdict(list)
    for symbol in symbols:
        parts = symbol["qualifiedName"].split("::")
        for start in range(len(parts)):
            index["::".join(parts[start:])].append(symbol)
    return dict(index)


def resolve_claim(
    claim: str,
    symbols: list[dict[str, Any]],
    suffix_index: dict[str, list[dict[str, Any]]] | None = None,
) -> tuple[str, list[str]]:
    if claim in NON_DECLARATION_CLAIMS:
        return "non-declaration", []
    if claim.startswith(EXTERNAL_DEPENDENCY_PREFIXES):
        return "external-dependency", []
    suffixes, arguments = claim_suffixes(claim)
    if suffix_index is None:
        candidates = [
            symbol
            for symbol in symbols
            if any(_ends_with_qualified(symbol["qualifiedName"], suffix) for suffix in suffixes)
        ]
    else:
        candidates = [symbol for suffix in suffixes for symbol in suffix_index.get(suffix, [])]
    if not claim.startswith(("cna_", "CNA_")):
        cpp = [symbol for symbol in candidates if symbol["language"] == "cpp"]
        if cpp:
            candidates = cpp

    tokens = _argument_tokens(arguments)
    if tokens and len(candidates) > 1:
        filtered = [
            symbol
            for symbol in candidates
            if all(token in symbol["signature"] or token in symbol["display"] for token in tokens)
        ]
        # An argument-qualified claim names particular overloads: when none matches, it is unresolved,
        # never silently widened to the whole member family.
        candidates = filtered

    unique = {symbol["id"]: symbol for symbol in candidates}
    candidates = sorted(unique.values(), key=lambda item: item["id"])
    if not candidates:
        return "unresolved", []

    base, _ = split_claim(claim)
    if "." not in base:
        typed = [symbol for symbol in candidates if symbol["kind"] in TYPE_KINDS]
        if typed:
            candidates = typed
    names = {symbol["qualifiedName"] for symbol in candidates}
    owners = {name.rsplit("::", 1)[0] if "::" in name else "" for name in names}
    if len(candidates) == 1:
        resolution = "exact"
    elif len(owners) == 1 or len(names) == 1:
        resolution = "family"
    else:
        resolution = "ambiguous"
    return resolution, [symbol["id"] for symbol in candidates]


def build_evidence(
    root: pathlib.Path,
    inventory: dict[str, Any],
    claims_payload: dict[str, Any],
    claims_sha256: str,
    exhibit_sources: dict[str, ExhibitSource],
    implementation_sha256: str,
    implementation_manifest: dict[str, Any],
    implementation_manifest_sha256: str,
    execution_manifest: dict[str, Any] | None = None,
    execution_manifest_sha256: str = "",
    current_museum_binary_sha256: str = "",
    current_cna_binary_sha256: str = "",
) -> dict[str, Any]:
    symbols = inventory["symbols"]
    suffix_index = build_symbol_suffix_index(symbols)
    by_id = {symbol["id"]: symbol for symbol in symbols}
    if len(by_id) != len(symbols):
        raise RuntimeError("public API inventory contains duplicate declaration IDs")
    rooms_by_key = {room["key"]: room for room in claims_payload["rooms"]}
    if len(rooms_by_key) != len(claims_payload["rooms"]):
        raise RuntimeError("museum coverage JSON contains duplicate room keys")
    missing_exhibits = sorted(
        {room["exhibit"] for room in claims_payload["rooms"]} - exhibit_sources.keys()
    )
    if missing_exhibits:
        raise RuntimeError(
            "coverage rooms use exhibits without a source mapping: "
            + ", ".join(missing_exhibits)
        )

    claim_rows = []
    links: collections.defaultdict[str, list[str]] = collections.defaultdict(list)
    resolution_counts: collections.Counter[str] = collections.Counter()
    for claim in claims_payload["claims"]:
        name = claim["api"]
        resolution, symbol_ids = resolve_claim(name, symbols, suffix_index)
        resolution_counts[resolution] += 1
        for symbol_id in symbol_ids:
            links[symbol_id].append(name)
        room_records = []
        for key in claim["rooms"]:
            if key not in rooms_by_key:
                raise RuntimeError(f"claim {name} refers to unknown room {key}")
            room = rooms_by_key[key]
            room_records.append(
                {
                    "key": key,
                    "exhibit": room["exhibit"],
                    "origin": room["origin"],
                    "implementationCandidate": {
                        "factory": exhibit_sources[room["exhibit"]].factory,
                        "source": exhibit_sources[room["exhibit"]].source,
                        "status": "source-located-not-reviewed",
                    },
                }
            )
        row = {
            "claim": name,
            "resolution": resolution,
            "symbols": symbol_ids,
            "rooms": room_records,
            "implementationEvidence": {
                "status": "unverified",
                "verifiedSymbols": [],
                "reviewedRooms": [],
                "verifiedRooms": [],
                "roomStatus": [],
                "records": [],
            },
            "executionEvidence": {
                "status": "unverified",
                "executedSymbols": [],
                "executedRooms": [],
                "verifiedRooms": [],
                "roomStatus": [],
                "records": [],
                "runs": [],
            },
            "correctnessEvidence": {
                "status": "unverified",
                "verifiedSymbols": [],
                "verifiedRooms": [],
                "roomStatus": [],
                "oracles": [],
                "runs": [],
            },
            "conditions": [],
        }
        if resolution == "non-declaration":
            row["note"] = NON_DECLARATION_CLAIMS[name]
        elif resolution == "external-dependency":
            row["note"] = "SharpRuntime/System dependency API; outside the CNA declaration denominator"
        claim_rows.append(row)

    implementation_records = resolve_implementation_records(
        root, implementation_manifest, claim_rows, exhibit_sources
    )
    implementation_inputs_sha256 = implementation_evidence_inputs_sha256(
        root, implementation_manifest_sha256, implementation_records
    )
    execution_runs: list[dict[str, Any]] = []
    execution_records: list[dict[str, Any]] = []
    correctness_oracles: list[dict[str, Any]] = []
    execution_inputs_sha256 = ""
    if execution_manifest is not None:
        (
            execution_runs,
            execution_records,
            correctness_oracles,
            execution_inputs_sha256,
        ) = resolve_execution_evidence(
            root,
            execution_manifest,
            implementation_records,
            current_museum_binary_sha256,
            current_cna_binary_sha256,
        )
    execution_by_implementation: collections.defaultdict[str, list[dict[str, Any]]] = (
        collections.defaultdict(list)
    )
    for record in execution_records:
        if not record["currentBuild"]:
            continue
        for implementation_id in record["implementationRecords"]:
            execution_by_implementation[implementation_id].append(record)
    correctness_by_implementation: collections.defaultdict[str, list[dict[str, Any]]] = (
        collections.defaultdict(list)
    )
    for oracle in correctness_oracles:
        if not oracle["currentBuild"]:
            continue
        for implementation_id in oracle["implementationRecords"]:
            correctness_by_implementation[implementation_id].append(oracle)
    implementation_by_id = {record["id"]: record for record in implementation_records}
    for record in implementation_records:
        executions = execution_by_implementation.get(record["id"], [])
        record["executionEvidence"] = {
            "status": "verified" if executions else "unverified",
            "records": [item["id"] for item in executions],
            "runs": sorted({item["run"] for item in executions}),
        }
        oracles = correctness_by_implementation.get(record["id"], [])
        record["correctnessEvidence"] = {
            "status": "verified" if oracles else "unverified",
            "oracles": [item["id"] for item in oracles],
            "runs": sorted({item["run"] for item in oracles}),
        }
    records_by_claim: collections.defaultdict[str, list[dict[str, Any]]] = collections.defaultdict(list)
    records_by_symbol: collections.defaultdict[str, list[dict[str, Any]]] = collections.defaultdict(list)
    for record in implementation_records:
        records_by_claim[record["claim"]].append(record)
        for symbol_id in record["symbols"]:
            records_by_symbol[symbol_id].append(record)
    for row in claim_rows:
        records = records_by_claim[row["claim"]]
        if not records:
            continue
        verified_symbols = sorted({symbol for record in records for symbol in record["symbols"]})
        reviewed_rooms = sorted({record["room"] for record in records})
        all_rooms = sorted(room["key"] for room in row["rooms"])
        room_status = []
        for room in all_rooms:
            room_records = [record for record in records if record["room"] == room]
            room_symbols = sorted(
                {symbol for record in room_records for symbol in record["symbols"]}
            )
            room_status.append(
                {
                    "room": room,
                    "status": (
                        "verified"
                        if room_records and room_symbols == sorted(row["symbols"])
                        else "partial" if room_records else "unverified"
                    ),
                    "verifiedSymbols": room_symbols,
                    "records": [record["id"] for record in room_records],
                }
            )
        verified_rooms = [
            item["room"] for item in room_status if item["status"] == "verified"
        ]
        status = "verified" if verified_rooms == all_rooms else "partial"
        row["implementationEvidence"] = {
            "status": status,
            "verifiedSymbols": verified_symbols,
            "reviewedRooms": reviewed_rooms,
            "verifiedRooms": verified_rooms,
            "roomStatus": room_status,
            "records": [record["id"] for record in records],
        }

    execution_records_by_claim: collections.defaultdict[str, list[dict[str, Any]]] = (
        collections.defaultdict(list)
    )
    execution_records_by_symbol: collections.defaultdict[str, list[dict[str, Any]]] = (
        collections.defaultdict(list)
    )
    for implementation_id, executions in execution_by_implementation.items():
        implementation = implementation_by_id[implementation_id]
        execution_records_by_claim[implementation["claim"]].extend(executions)
        for symbol_id in implementation["symbols"]:
            execution_records_by_symbol[symbol_id].extend(executions)
    for row in claim_rows:
        implementation_records_for_claim = records_by_claim[row["claim"]]
        executed_implementation_records = [
            record
            for record in implementation_records_for_claim
            if execution_by_implementation.get(record["id"])
        ]
        if not executed_implementation_records:
            continue
        all_rooms = sorted(room["key"] for room in row["rooms"])
        room_status = []
        for room in all_rooms:
            room_records = [
                record for record in executed_implementation_records if record["room"] == room
            ]
            room_symbols = sorted(
                {symbol for record in room_records for symbol in record["symbols"]}
            )
            room_status.append(
                {
                    "room": room,
                    "status": (
                        "verified"
                        if room_records and room_symbols == sorted(row["symbols"])
                        else "partial" if room_records else "unverified"
                    ),
                    "executedSymbols": room_symbols,
                    "implementationRecords": [record["id"] for record in room_records],
                }
            )
        executed_symbols = sorted(
            {symbol for record in executed_implementation_records for symbol in record["symbols"]}
        )
        executed_rooms = sorted({record["room"] for record in executed_implementation_records})
        verified_rooms = [
            item["room"] for item in room_status if item["status"] == "verified"
        ]
        related_execution_records = execution_records_by_claim[row["claim"]]
        row["executionEvidence"] = {
            "status": "verified" if verified_rooms == all_rooms else "partial",
            "executedSymbols": executed_symbols,
            "executedRooms": executed_rooms,
            "verifiedRooms": verified_rooms,
            "roomStatus": room_status,
            "records": sorted({record["id"] for record in related_execution_records}),
            "runs": sorted({record["run"] for record in related_execution_records}),
        }

    correctness_oracles_by_symbol: collections.defaultdict[str, list[dict[str, Any]]] = (
        collections.defaultdict(list)
    )
    for implementation_id, oracles in correctness_by_implementation.items():
        implementation = implementation_by_id[implementation_id]
        for symbol_id in implementation["symbols"]:
            correctness_oracles_by_symbol[symbol_id].extend(oracles)
    for row in claim_rows:
        implementation_records_for_claim = records_by_claim[row["claim"]]
        verified_implementation_records = [
            record
            for record in implementation_records_for_claim
            if correctness_by_implementation.get(record["id"])
        ]
        if not verified_implementation_records:
            continue
        all_rooms = sorted(room["key"] for room in row["rooms"])
        room_status = []
        for room in all_rooms:
            room_records = [
                record for record in verified_implementation_records if record["room"] == room
            ]
            room_symbols = sorted(
                {symbol for record in room_records for symbol in record["symbols"]}
            )
            room_status.append(
                {
                    "room": room,
                    "status": (
                        "verified"
                        if room_records and room_symbols == sorted(row["symbols"])
                        else "partial" if room_records else "unverified"
                    ),
                    "verifiedSymbols": room_symbols,
                    "implementationRecords": [record["id"] for record in room_records],
                }
            )
        verified_symbols = sorted(
            {symbol for record in verified_implementation_records for symbol in record["symbols"]}
        )
        verified_rooms = [
            item["room"] for item in room_status if item["status"] == "verified"
        ]
        related_oracles = [
            oracle
            for record in verified_implementation_records
            for oracle in correctness_by_implementation[record["id"]]
        ]
        row["correctnessEvidence"] = {
            "status": "verified" if verified_rooms == all_rooms else "partial",
            "verifiedSymbols": verified_symbols,
            "verifiedRooms": verified_rooms,
            "roomStatus": room_status,
            "oracles": sorted({oracle["id"] for oracle in related_oracles}),
            "runs": sorted({oracle["run"] for oracle in related_oracles}),
        }

    symbol_rows = []
    module_counts: dict[str, collections.Counter[str]] = collections.defaultdict(
        collections.Counter
    )
    for symbol in symbols:
        claimed_by = sorted(set(links.get(symbol["id"], [])))
        module_counts[symbol["module"]]["symbols"] += 1
        if claimed_by:
            module_counts[symbol["module"]]["catalogLinked"] += 1
        symbol_records = records_by_symbol.get(symbol["id"], [])
        symbol_execution_records = execution_records_by_symbol.get(symbol["id"], [])
        symbol_correctness_oracles = correctness_oracles_by_symbol.get(symbol["id"], [])
        symbol_rows.append(
            {
                "id": symbol["id"],
                "module": symbol["module"],
                "language": symbol["language"],
                "kind": symbol["kind"],
                "qualifiedName": symbol["qualifiedName"],
                "catalogClaims": claimed_by,
                "catalogLinked": bool(claimed_by),
                "implementationEvidence": {
                    "status": "verified" if symbol_records else "unverified",
                    "records": [record["id"] for record in symbol_records],
                },
                "executionEvidence": {
                    "status": "verified" if symbol_execution_records else "unverified",
                    "records": sorted(
                        {record["id"] for record in symbol_execution_records}
                    ),
                    "runs": sorted({record["run"] for record in symbol_execution_records}),
                },
                "correctnessEvidence": {
                    "status": "verified" if symbol_correctness_oracles else "unverified",
                    "oracles": sorted(
                        {oracle["id"] for oracle in symbol_correctness_oracles}
                    ),
                    "runs": sorted({oracle["run"] for oracle in symbol_correctness_oracles}),
                },
                "status": "unverified",
            }
        )

    modules = []
    for module in inventory["modules"]:
        counts = module_counts[module["name"]]
        modules.append(
            {
                **module,
                "catalogLinked": counts["catalogLinked"],
                "notCatalogLinked": counts["symbols"] - counts["catalogLinked"],
            }
        )
    return {
        "schema": SCHEMA,
        "source": {
            "museumClaimsSha256": claims_sha256,
            "museumImplementationSourcesSha256": implementation_sha256,
            "implementationEvidenceManifestSha256": implementation_manifest_sha256,
            "implementationEvidenceInputsSha256": implementation_inputs_sha256,
            "executionEvidenceManifestSha256": execution_manifest_sha256,
            "executionEvidenceInputsSha256": execution_inputs_sha256,
            "currentMuseumBinarySha256": current_museum_binary_sha256,
            "currentCnaBinarySha256": current_cna_binary_sha256,
            "exhibitRegistry": "src/Exhibits/ExhibitRegistry.cpp",
            "cna": inventory["source"],
        },
        "definitions": {
            "catalogLinked": "a catalogue string resolved to this declaration; not proof of a call",
            "implementationEvidence": "a reviewed source/trace path reaches the declaration",
            "executionEvidence": "a recorded scenario executed that path under named conditions",
            "correctnessEvidence": "an oracle checked an expected result, refusal or state transition",
            "unverified": "no qualifying evidence has been recorded yet",
            "externalDependency": "a dependency API intentionally outside the CNA declaration denominator",
            "implementationCandidate": "the registry locates the exhibit factory source; this is not proof that the room reaches a claimed declaration",
            "currentBuildExecution": "the run artifacts are valid and their museum and CNA binary SHA-256 values match the binaries used by this audit",
        },
        "counts": {
            "claims": len(claim_rows),
            "claimRoomPairs": sum(len(row["rooms"]) for row in claim_rows),
            "resolutions": dict(sorted(resolution_counts.items())),
            "declarations": len(symbol_rows),
            "catalogLinkedDeclarations": sum(bool(row["catalogClaims"]) for row in symbol_rows),
            "executedDeclarations": sum(
                row["executionEvidence"]["status"] == "verified" for row in symbol_rows
            ),
            "correctnessVerifiedDeclarations": sum(
                row["correctnessEvidence"]["status"] == "verified" for row in symbol_rows
            ),
            "roomsWithImplementationSource": len(rooms_by_key),
            "implementationEvidenceRecords": len(implementation_records),
            "implementedDeclarations": len(records_by_symbol),
            "implementationVerifiedClaims": sum(
                row["implementationEvidence"]["status"] == "verified" for row in claim_rows
            ),
            "implementationReviewedClaimRoomPairs": sum(
                len(row["implementationEvidence"]["reviewedRooms"]) for row in claim_rows
            ),
            "implementationVerifiedClaimRoomPairs": sum(
                len(row["implementationEvidence"]["verifiedRooms"]) for row in claim_rows
            ),
            "executionRuns": len(execution_runs),
            "currentExecutionRuns": sum(run["currentBuild"] for run in execution_runs),
            "executionEvidenceRecords": len(execution_records),
            "executionVerifiedClaims": sum(
                row["executionEvidence"]["status"] == "verified" for row in claim_rows
            ),
            "executionVerifiedClaimRoomPairs": sum(
                len(row["executionEvidence"]["verifiedRooms"]) for row in claim_rows
            ),
            "correctnessOracles": len(correctness_oracles),
            "correctnessVerifiedClaims": sum(
                row["correctnessEvidence"]["status"] == "verified" for row in claim_rows
            ),
            "correctnessVerifiedClaimRoomPairs": sum(
                len(row["correctnessEvidence"]["verifiedRooms"]) for row in claim_rows
            ),
        },
        "modules": modules,
        "claims": claim_rows,
        "implementationRecords": implementation_records,
        "executionRuns": execution_runs,
        "executionRecords": execution_records,
        "correctnessOracles": correctness_oracles,
        "symbols": symbol_rows,
    }


def classify_areas(
    inventory: dict[str, Any], evidence_symbols: list[dict[str, Any]], manifest: dict[str, Any]
) -> dict[str, Any]:
    """Give every public header a status: claimed by a room, or classified by a reviewed rule.

    Headers with at least one catalogue-linked declaration are demonstrated (at whatever evidence
    level the matrix shows). Every other header must match a rule (first match wins) whose status is
    gap (with a backlog task), supporting or not-applicable; a rule that matches no unclaimed header
    is stale, so the classification has to follow the catalogue.
    """
    if manifest.get("schema") != AREA_SCHEMA:
        raise RuntimeError("unsupported area classification schema")
    statuses = manifest.get("statuses")
    rules = manifest.get("rules")
    if not isinstance(statuses, dict) or not isinstance(rules, list) or not rules:
        raise RuntimeError("area classification needs statuses and rules")
    for index, rule in enumerate(rules):
        if not isinstance(rule, dict) or not all(
            isinstance(rule.get(key), str) for key in ("match", "area", "status", "backlog", "reason")
        ):
            raise RuntimeError(f"area rule {index} is malformed")
        if rule["status"] not in statuses:
            raise RuntimeError(f"area rule {rule['match']} has unknown status {rule['status']!r}")
        if (rule["status"] == "gap") != bool(rule["backlog"].strip()):
            raise RuntimeError(f"area rule {rule['match']}: a gap names its backlog task, other statuses do not")
        if not rule["reason"].strip():
            raise RuntimeError(f"area rule {rule['match']} needs a reason")
    linked = {symbol["id"] for symbol in evidence_symbols if symbol["catalogLinked"]}
    headers: collections.defaultdict[str, list[int]] = collections.defaultdict(lambda: [0, 0])
    for symbol in inventory["symbols"]:
        row = headers[symbol["header"]]
        row[0] += 1
        row[1] += symbol["id"] in linked
    rule_rows = [dict(rule, headers=0, declarations=0) for rule in rules]
    unmatched = []
    demonstrated_headers = demonstrated_declarations = 0
    for header, (declarations, claimed) in sorted(headers.items()):
        if claimed:
            demonstrated_headers += 1
            demonstrated_declarations += declarations
            continue
        for row in rule_rows:
            if fnmatch.fnmatchcase(header, row["match"]):
                row["headers"] += 1
                row["declarations"] += declarations
                break
        else:
            unmatched.append(header)
    if unmatched:
        raise RuntimeError(
            f"{len(unmatched)} unclaimed public header(s) have no area classification, first: "
            + ", ".join(unmatched[:5])
        )
    stale = [row["match"] for row in rule_rows if row["headers"] == 0]
    if stale:
        raise RuntimeError("area rules that match no unclaimed header: " + ", ".join(stale[:5]))
    totals: dict[str, dict[str, int]] = {}
    for row in rule_rows:
        total = totals.setdefault(row["status"], {"headers": 0, "declarations": 0})
        total["headers"] += row["headers"]
        total["declarations"] += row["declarations"]
    return {
        "demonstratedHeaders": demonstrated_headers,
        "demonstratedHeaderDeclarations": demonstrated_declarations,
        "classified": totals,
        "statuses": statuses,
        "rules": rule_rows,
    }


def render_areas(areas: dict[str, Any]) -> list[str]:
    lines = [
        "",
        "## Public API areas",
        "",
        f"**{areas['demonstratedHeaders']}** public headers ({areas['demonstratedHeaderDeclarations']} declarations) "
        "have at least one declaration claimed by a room. Every other header is classified by a reviewed rule in",
        "`audit/area_classification.json` (first match wins; the check fails for an unclassified header or a stale rule):",
        "",
        "| Status | Headers | Declarations | Meaning |",
        "|---|---:|---:|---|",
    ]
    for status, meaning in areas["statuses"].items():
        total = areas["classified"].get(status, {"headers": 0, "declarations": 0})
        lines.append(f"| {status} | {total['headers']} | {total['declarations']} | {meaning} |")
    lines.extend(("", "| Area | Status | Backlog | Headers | Declarations | Reason |", "|---|---|---|---:|---:|---|"))
    merged: dict[tuple[str, str, str], list[Any]] = {}
    for row in areas["rules"]:
        key = (row["area"], row["status"], row["backlog"])
        entry = merged.setdefault(key, [0, 0, []])
        entry[0] += row["headers"]
        entry[1] += row["declarations"]
        if row["reason"] not in entry[2]:
            entry[2].append(row["reason"])
    order = {"gap": 0, "supporting": 1, "not-applicable": 2}
    for (area, status, backlog), (count, declarations, reasons) in sorted(
        merged.items(), key=lambda item: (order.get(item[0][1], 9), item[0][0])
    ):
        reason = reasons[0] if len(reasons) == 1 else reasons[0] + f" (+{len(reasons) - 1} more rule(s))"
        lines.append(f"| {area} | {status} | {backlog or '-'} | {count} | {declarations} | {reason} |")
    return lines


def render_summary(evidence: dict[str, Any]) -> str:
    counts = evidence["counts"]
    resolutions = counts["resolutions"]
    problem_claims = [
        claim
        for claim in evidence["claims"]
        if claim["resolution"] in {"unresolved", "ambiguous", "non-declaration"}
    ]
    external_claims = [
        claim for claim in evidence["claims"] if claim["resolution"] == "external-dependency"
    ]
    room_progress: collections.defaultdict[str, collections.Counter[str]] = collections.defaultdict(
        collections.Counter
    )
    for claim in evidence["claims"]:
        implementation_statuses = {
            item["room"]: item["status"]
            for item in claim["implementationEvidence"]["roomStatus"]
        }
        execution_statuses = {
            item["room"]: item["status"]
            for item in claim["executionEvidence"]["roomStatus"]
        }
        correctness_statuses = {
            item["room"]: item["status"]
            for item in claim["correctnessEvidence"]["roomStatus"]
        }
        for room in claim["rooms"]:
            progress = room_progress[room["key"]]
            progress["claims"] += 1
            implementation_status = implementation_statuses.get(room["key"], "unverified")
            execution_status = execution_statuses.get(room["key"], "unverified")
            correctness_status = correctness_statuses.get(room["key"], "unverified")
            progress[implementation_status] += 1
            progress[f"executed_{execution_status}"] += 1
            progress[f"correctness_{correctness_status}"] += 1
    lines = [
        "# CNA museum coverage audit",
        "",
        "<!-- Generated by tools/audit_api_coverage.py; do not edit by hand. -->",
        "",
        "A catalogue link is not proof that the museum calls, executes or validates a declaration.",
        "Implementation, execution and correctness counts rise only from recorded evidence at each stage.",
        "The complete per-claim and per-declaration matrix",
        "is generated in `build/api-audit/coverage-evidence.json`.",
        "",
        # Content hashes, not the git revision: committing the generated summary must not make it stale.
        f"Museum catalogue SHA-256: `{evidence['source']['museumClaimsSha256']}`; exhibit sources SHA-256: "
        f"`{evidence['source']['museumImplementationSourcesSha256']}`",
        "",
        f"CNA revision: `{evidence['source']['cna']['revision']}` "
        f"(`{evidence['source']['cna']['description']}`)",
        "",
        f"Claims: **{counts['claims']}**; exact declaration matches: "
        f"**{resolutions.get('exact', 0)}**; member/overload families: "
        f"**{resolutions.get('family', 0)}**; ambiguous: **{resolutions.get('ambiguous', 0)}**; "
        f"unresolved: **{resolutions.get('unresolved', 0)}**; non-declaration labels: "
        f"**{resolutions.get('non-declaration', 0)}**; external dependency claims: "
        f"**{resolutions.get('external-dependency', 0)}**.",
        "",
        f"Public declarations: **{counts['declarations']}**; linked by at least one catalogue name: "
        f"**{counts['catalogLinkedDeclarations']}**; implementation-verified: "
        f"**{counts['implementedDeclarations']}**; executed: **{counts['executedDeclarations']}**; "
        f"correctness-verified: **{counts['correctnessVerifiedDeclarations']}**.",
        "",
        f"Implementation source located through the exhibit registry for "
        f"**{counts['roomsWithImplementationSource']} rooms**. Registry locations are candidates; "
        "reviewed call paths are listed separately below.",
        f"Reviewed implementation records: **{counts['implementationEvidenceRecords']}**; "
        f"fully implementation-verified claims: **{counts['implementationVerifiedClaims']}**. "
        f"Reviewed claim-room pairs: **{counts['implementationReviewedClaimRoomPairs']}** of "
        f"**{counts['claimRoomPairs']}**; fully verified pairs: "
        f"**{counts['implementationVerifiedClaimRoomPairs']}**.",
        f"Execution runs: **{counts['currentExecutionRuns']} current** of "
        f"**{counts['executionRuns']} recorded**; execution records: "
        f"**{counts['executionEvidenceRecords']}**; fully executed claims: "
        f"**{counts['executionVerifiedClaims']}**; executed claim-room pairs: "
        f"**{counts['executionVerifiedClaimRoomPairs']}**.",
        f"Correctness oracles: **{counts['correctnessOracles']}**; fully correctness-verified "
        f"claims: **{counts['correctnessVerifiedClaims']}**; correctness-verified "
        f"claim-room pairs: **{counts['correctnessVerifiedClaimRoomPairs']}**.",
        "",
        "| Module | Declarations | Catalogue-linked | Not catalogue-linked |",
        "|---|---:|---:|---:|",
    ]
    for module in evidence["modules"]:
        lines.append(
            f"| `{module['name']}` | {module['symbols']} | {module['catalogLinked']} | "
            f"{module['notCatalogLinked']} |"
        )
    lines.extend(("", "## Implementation review progress", ""))
    reviewed_rooms = [
        (room, progress)
        for room, progress in sorted(room_progress.items())
        if progress["verified"] or progress["partial"]
    ]
    if not reviewed_rooms:
        lines.append("No claim-room pair has been reviewed yet.")
    else:
        lines.extend(
            (
                "Only rooms with at least one reviewed pair are listed.",
                "",
                "| Room | Implementation verified | Implementation partial | Executed | Execution partial | Correctness verified | Total claims |",
                "|---|---:|---:|---:|---:|---:|---:|",
            )
        )
        for room, progress in reviewed_rooms:
            lines.append(
                f"| `{room}` | {progress['verified']} | {progress['partial']} | "
                f"{progress['executed_verified']} | {progress['executed_partial']} | "
                f"{progress['correctness_verified']} | "
                f"{progress['claims']} |"
            )
    lines.extend(("", "## Claims requiring review", ""))
    if not problem_claims:
        lines.append("None.")
    else:
        lines.extend(("| Claim | Resolution | Rooms |", "|---|---|---|"))
        for claim in problem_claims:
            rooms = ", ".join(f"`{room['key']}`" for room in claim["rooms"])
            lines.append(
                f"| `{claim['claim']}` | {claim['resolution']} | {rooms} |"
            )
    lines.extend(
        (
            "",
            "## External dependency claims",
            "",
            "These are demonstrated by museum rooms but are not counted as CNA declarations:",
            "",
        )
    )
    if not external_claims:
        lines.append("None.")
    else:
        lines.extend(("| Claim | Rooms |", "|---|---|"))
        for claim in external_claims:
            rooms = ", ".join(f"`{room['key']}`" for room in claim["rooms"])
            lines.append(f"| `{claim['claim']}` | {rooms} |")
    if "areas" in evidence:
        lines.extend(render_areas(evidence["areas"]))
    lines.extend(
        (
            "",
            "Regenerate or verify with:",
            "",
            "```bash",
            "python3 tools/generate_api_inventory.py --write",
            "python3 tools/audit_api_coverage.py --write",
            "python3 tools/audit_api_coverage.py --check",
            "```",
            "",
        )
    )
    return "\n".join(lines)


def parse_arguments() -> argparse.Namespace:
    root = repository_root()
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    parser.add_argument(
        "--inventory",
        type=pathlib.Path,
        default=root / "build" / "api-audit" / "public-api.json",
    )
    parser.add_argument(
        "--museum-bin", type=pathlib.Path, default=root / "build" / "bin" / "cna-3d-museum"
    )
    parser.add_argument(
        "--json-output",
        type=pathlib.Path,
        default=root / "build" / "api-audit" / "coverage-evidence.json",
    )
    parser.add_argument(
        "--summary-output",
        type=pathlib.Path,
        default=root / "docs" / "CNA_COVERAGE_AUDIT.md",
    )
    parser.add_argument(
        "--implementation-evidence",
        type=pathlib.Path,
        default=root / "audit" / "implementation_evidence.json",
    )
    parser.add_argument(
        "--execution-evidence",
        type=pathlib.Path,
        default=root / "audit" / "execution_evidence.json",
    )
    parser.add_argument(
        "--area-classification",
        type=pathlib.Path,
        default=root / "audit" / "area_classification.json",
    )
    parser.add_argument(
        "--cna-bin",
        type=pathlib.Path,
        default=root / "build" / "cna" / "libcna.so",
    )
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    root = repository_root()
    if not arguments.inventory.is_file():
        raise RuntimeError(
            f"API inventory is missing: run tools/generate_api_inventory.py --write"
        )
    if not arguments.museum_bin.is_file():
        raise RuntimeError(f"museum binary is missing: {arguments.museum_bin}")
    inventory = json.loads(arguments.inventory.read_text(encoding="utf-8"))
    if inventory.get("schema") != "museum-cna-public-api/1":
        raise RuntimeError("unsupported public API inventory schema")
    claims, claims_sha256 = load_claims(arguments.museum_bin.resolve())
    sources = discover_exhibit_sources(root)
    implementation_sha256 = exhibit_source_sha256(root, sources)
    implementation_manifest, implementation_manifest_sha256 = load_implementation_manifest(
        arguments.implementation_evidence
    )
    execution_manifest, execution_manifest_sha256 = load_execution_manifest(
        arguments.execution_evidence
    )
    current_museum_binary_sha256 = sha256_file(arguments.museum_bin)
    current_cna_binary_sha256 = (
        sha256_file(arguments.cna_bin) if arguments.cna_bin.is_file() else "missing"
    )
    evidence = build_evidence(
        root,
        inventory,
        claims,
        claims_sha256,
        sources,
        implementation_sha256,
        implementation_manifest,
        implementation_manifest_sha256,
        execution_manifest,
        execution_manifest_sha256,
        current_museum_binary_sha256,
        current_cna_binary_sha256,
    )
    area_manifest = json.loads(arguments.area_classification.read_text(encoding="utf-8"))
    evidence["areas"] = classify_areas(inventory, evidence["symbols"], area_manifest)
    rendered_json = json.dumps(evidence, indent=2, ensure_ascii=False) + "\n"
    summary = render_summary(evidence)

    if arguments.write:
        arguments.json_output.parent.mkdir(parents=True, exist_ok=True)
        arguments.summary_output.parent.mkdir(parents=True, exist_ok=True)
        arguments.json_output.write_text(rendered_json, encoding="utf-8")
        arguments.summary_output.write_text(summary, encoding="utf-8")
        print(
            f"museum coverage audit: {evidence['counts']['claims']} claims, "
            f"{evidence['counts']['catalogLinkedDeclarations']} declaration links; wrote "
            f"{arguments.json_output} and {arguments.summary_output}"
        )
        return 0

    if not arguments.summary_output.is_file():
        print("coverage audit summary is missing: run tools/audit_api_coverage.py --write", file=sys.stderr)
        return 1
    if arguments.summary_output.read_text(encoding="utf-8") != summary:
        print("coverage audit summary is stale: run tools/audit_api_coverage.py --write", file=sys.stderr)
        return 1
    if not arguments.json_output.is_file():
        arguments.json_output.parent.mkdir(parents=True, exist_ok=True)
        arguments.json_output.write_text(rendered_json, encoding="utf-8")
        print(f"created build cache {arguments.json_output}")
    if arguments.json_output.read_text(encoding="utf-8") != rendered_json:
        # The JSON is an untracked build cache derived from the same inputs as the summary checked above (which must
        # match); after a rebuild of the binary it is refreshed, as when it is missing, instead of failing the check.
        arguments.json_output.write_text(rendered_json, encoding="utf-8")
        print(f"refreshed build cache {arguments.json_output}")
    print(
        f"museum coverage audit is current: {evidence['counts']['claims']} claims, "
        f"{evidence['counts']['catalogLinkedDeclarations']} declaration links"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.SubprocessError, json.JSONDecodeError) as error:
        print(f"museum coverage audit error: {error}", file=sys.stderr)
        raise SystemExit(2) from error
