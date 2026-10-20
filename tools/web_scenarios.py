#!/usr/bin/env python3
"""The functional scenarios (tests/scenarios/*.json) in the browser edition: each script runs in headless Chrome through
tools/web_tour.sh's script mode and its console lines must satisfy the same checks as tools/scenarios.py.

  tools/web_scenarios.py [name ...]      exit 0 when no scenario failed; needs tools/build_web.sh first

MUSEUM_WEB_RENDERER (default WEBGL2) picks the renderer of the two-renderer browser build. A scenario is SKIP, never a pass,
when its build switch is off in build-consumer, when it needs extra environment or files (a page has no environment), or
when it names native renderers. A scenario's "web" entry may replace native expectations that need an optional library the
browser build lacks (Zstandard, the FFmpeg media decoder) with the exact line the room prints instead.
"""

from __future__ import annotations

import json
import os
import pathlib
import re
import subprocess
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
from scenarios import SCENARIOS, check_log  # noqa: E402

ROOT = pathlib.Path(__file__).resolve().parents[1]
BUILD = ROOT / "build-consumer"


def main() -> int:
    page = BUILD / "bin" / "cna-3d-museum.html"
    if not page.is_file():
        print(f"error: {page} is not built (tools/build_web.sh)", file=sys.stderr)
        return 2
    cache = (BUILD / "CMakeCache.txt").read_text(errors="replace")
    features = {m.group(1): m.group(2).upper() in ("ON", "1", "TRUE", "YES") for m in re.finditer(r"^([A-Z0-9_]+):BOOL=(\S*)$", cache, re.MULTILINE)}
    features.update({m.group(1): m.group(2).upper() in ("ON", "AUTO") for m in re.finditer(r"^(MUSEUM_[A-Z0-9_]+):STRING=(\S*)$", cache, re.MULTILINE)})
    renderer = os.environ.get("MUSEUM_WEB_RENDERER", "WEBGL2")
    names = sys.argv[1:] or sorted(path.stem for path in SCENARIOS.glob("*.json"))
    counts = {"PASS": 0, "FAIL": 0, "SKIP": 0}
    for name in names:
        spec = json.loads((SCENARIOS / f"{name}.json").read_text(encoding="utf-8"))
        required = spec.get("requires")
        reason = (f"{required} is off in the browser build" if required and not features.get(required, False) else
                  "needs extra environment or files, which a page does not have" if ("env" in spec or "touch" in spec) and not spec.get("web", {}).get("ignoreEnv") else
                  "names native renderers" if "renderers" in spec else None)
        if reason:
            print(f"SKIP  {name}: {reason}")
            counts["SKIP"] += 1
            continue
        # "web": {"replace": {native expectation: browser expectation}} where the browser build lacks an optional library and
        # the room says so with its own exact line (the replacement is checked just as strictly); "replaceRegex" likewise;
        # "ignoreEnv": the environment only isolated the native run (a browser run starts from a fresh profile anyway).
        for native, browser in spec.get("web", {}).get("replace", {}).items():
            spec["expect"] = [browser if e == native else e for e in spec.get("expect", [])]
        for native, browser in spec.get("web", {}).get("replaceRegex", {}).items():
            spec["expectRegex"] = [browser if e == native else e for e in spec.get("expectRegex", [])]
        out = BUILD / "web-scenarios" / renderer / name
        env = dict(os.environ, MUSEUM_WEB_SCRIPT=spec["script"], MUSEUM_WEB_OUT=str(out), MUSEUM_WEB_RENDERER=renderer,
                   MUSEUM_WEB_TIMEOUT=str(max(600, 3 * int(spec.get("timeout", 240)))))
        subprocess.run([str(ROOT / "tools" / "web_tour.sh")], cwd=ROOT, env=env, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, check=False)
        log = out / "museum.log"
        raw = log.read_text(errors="replace") if log.is_file() else ""
        text = re.sub(r"^MUSEUM_(OUT|ERR) ", "", raw, flags=re.MULTILINE)   # the console prefixes each line with its stream
        problems = check_log(spec, text)
        if "MUSEUM_EXIT 0" not in raw:
            exits = re.findall(r"MUSEUM_(?:EXIT|ABORT).*", raw)
            problems.insert(0, f"the page did not exit 0 ({exits[-1] if exits else 'no exit reported'})")
        for marker in ("MUSEUM_ABORT", "MUSEUM_PAGE_ERROR"):
            if marker in raw:
                problems.append(next(line for line in raw.splitlines() if marker in line)[:160])
        status = "FAIL" if problems else "PASS"
        counts[status] += 1
        print(f"{status:5} {name}: {spec.get('description', '')}")
        for problem in problems:
            print(f"        {problem}")
    print(f"summary ({renderer} in the browser): {counts['PASS']} passed, {counts['FAIL']} failed, {counts['SKIP']} skipped")
    if not sys.argv[1:]:
        subprocess.run([sys.executable, str(ROOT / "tools" / "record_profile.py"), f"web-{renderer.lower()}-scenarios", "--passed", str(counts["PASS"]),
                        "--failed", str(counts["FAIL"]), "--skipped", str(counts["SKIP"]), "--binary", str(BUILD / "bin" / "cna-3d-museum.wasm"),
                        "--library", str(BUILD / "bin" / "cna-3d-museum.wasm")], env=dict(os.environ, BUILD_DIR="build-consumer"), check=False)
    return 1 if counts["FAIL"] else 0


if __name__ == "__main__":
    sys.exit(main())
