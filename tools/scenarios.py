#!/usr/bin/env python3
"""Functional scenarios: scripted visits whose log must contain exact expected results.

Each tests/scenarios/<name>.json describes one scenario:

    {"description": "...",
     "script": "goto:pl-inspector;wait:20;press:Agent::Start;...;stats;quit",
     "expect": ["exact substring of a log line", ...],          # every one must occur
     "expectRegex": ["regular expression", ...],                 # optional
     "forbid": ["substring that must not occur", ...],           # optional
     "renderers": ["OPENGLES3"],                                 # optional, default: the build's default
     "requires": "MUSEUM_INSPECTOR",                             # optional: a CMake switch that must be ON in the build tree
     "env": {"MUSEUM_DATA_HOME": "{dir}/blocked/data"},             # optional: extra environment; {dir} is a fresh directory
     "touch": ["blocked"]}                                       # optional: empty regular files created in {dir} first

The museum always runs on a private virtual display (tools/run.sh). A scenario passes only when the museum exited
0, reached its stats step with no exhibit or script errors, and every expectation held. A scenario whose required
build switch is off is reported as SKIP (never as a pass).

  tools/scenarios.py [name ...]          exit 0 when no scenario failed
"""

from __future__ import annotations

import json
import os
import pathlib
import re
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
SCENARIOS = ROOT / "tests" / "scenarios"


def run_scenario(name: str, spec: dict, build_dir: str) -> tuple[str, list[str]]:
    out_dir = ROOT / build_dir / "scenarios"
    out_dir.mkdir(parents=True, exist_ok=True)
    problems: list[str] = []
    for renderer in spec.get("renderers", [None]):
        tag = f"{name}{'-' + renderer if renderer else ''}"
        log = out_dir / f"{tag}.log"
        env = dict(os.environ, BUILD_DIR=build_dir, MUSEUM_LOG=str(log), MUSEUM_TIMEOUT=str(spec.get("timeout", 240)))
        if "env" in spec or "touch" in spec:
            scratch = out_dir / f"{tag}.d"
            shutil.rmtree(scratch, ignore_errors=True)
            scratch.mkdir()
            for name_ in spec.get("touch", []):
                (scratch / name_).touch()
            env.update({key: value.replace("{dir}", str(scratch)) for key, value in spec.get("env", {}).items()})
        if renderer:
            env["CNA_GRAPHICS_RENDERER"] = renderer
        completed = subprocess.run([str(ROOT / "tools" / "run.sh"), spec["script"], "--no-audio"], cwd=ROOT, env=env,
                                   stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
        text = log.read_text(errors="replace") if log.is_file() else ""
        prefix = f"[{renderer}] " if renderer else ""
        if completed.returncode != 0:
            problems.append(f"{prefix}the museum exited with status {completed.returncode}")
        stats = re.findall(r"^\[museum\]\[stats\].*$", text, re.MULTILINE)
        if not stats:
            problems.append(f"{prefix}no stats line (the script did not finish)")
        elif not re.search(r"\bexhibit errors 0\b", stats[-1]) or re.search(r"\bscript errors [1-9]", stats[-1]):
            problems.append(f"{prefix}errors in the stats line: {stats[-1][-60:]}")
        for marker in ("[museum][exhibit-error]", "[museum][draw-error]", "[museum][script-error]"):
            if marker in text:
                first = next(line for line in text.splitlines() if marker in line)
                problems.append(f"{prefix}{first[:160]}")
        for expected in spec.get("expect", []):
            if expected not in text:
                problems.append(f"{prefix}missing: {expected}")
        for pattern in spec.get("expectRegex", []):
            if not re.search(pattern, text, re.MULTILINE):
                problems.append(f"{prefix}no line matches: {pattern}")
        for forbidden in spec.get("forbid", []):
            if forbidden in text:
                problems.append(f"{prefix}must not occur: {forbidden}")
    return ("FAIL" if problems else "PASS"), problems


def main() -> int:
    build_dir = os.environ.get("BUILD_DIR", "build")
    binary = ROOT / build_dir / "bin" / "cna-3d-museum"
    if not binary.is_file():
        print(f"error: {binary} is not built", file=sys.stderr)
        return 2
    cache = (ROOT / build_dir / "CMakeCache.txt").read_text(errors="replace")
    features = {match.group(1): match.group(2).upper() in ("ON", "1", "TRUE", "YES") for match in re.finditer(r"^([A-Z0-9_]+):BOOL=(\S*)$", cache, re.MULTILINE)}
    # String switches such as MUSEUM_VIDEO (OFF, AUTO, ON): AUTO counts as on (the room says so when the library is missing).
    features.update({match.group(1): match.group(2).upper() in ("ON", "AUTO") for match in re.finditer(r"^(MUSEUM_[A-Z0-9_]+):STRING=(\S*)$", cache, re.MULTILINE)})
    names = sys.argv[1:] or sorted(path.stem for path in SCENARIOS.glob("*.json"))
    counts = {"PASS": 0, "FAIL": 0, "SKIP": 0}
    for name in names:
        spec = json.loads((SCENARIOS / f"{name}.json").read_text(encoding="utf-8"))
        required = spec.get("requires")
        if required and not features.get(required, False):
            print(f"SKIP  {name}: {required} is off in this build")
            counts["SKIP"] += 1
            continue
        status, problems = run_scenario(name, spec, build_dir)
        counts[status] += 1
        print(f"{status:5} {name}: {spec.get('description', '')}")
        for problem in problems:
            print(f"        {problem}")
    print(f"summary: {counts['PASS']} passed, {counts['FAIL']} failed, {counts['SKIP']} skipped")
    if not sys.argv[1:]:   # only the whole set is a profile result
        subprocess.run([sys.executable, str(ROOT / "tools" / "record_profile.py"), "scenarios", "--passed", str(counts["PASS"]),
                        "--failed", str(counts["FAIL"]), "--skipped", str(counts["SKIP"])], check=False)
    return 1 if counts["FAIL"] else 0


if __name__ == "__main__":
    sys.exit(main())
