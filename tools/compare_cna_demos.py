#!/usr/bin/env python3
"""Compare the museum's coverage with the sibling cna-demos catalogue and its CNA issue matrix.

cna-demos is a different repository with its own runs; nothing here counts as a museum run. The report
answers two questions for the audit (backlog A10):

  * which CNA declarations the demos name that no museum room claims, grouped by demo category and
    subcategory, so that no whole area silently disappears from the museum;
  * which declarations are touched by a known open or unverified CNA issue, and what the museum
    claims about them: a catalogue link or an executed path is never a PASS, and a correctness oracle
    over such a declaration is listed for review.

  tools/compare_cna_demos.py [--demos-exe PATH | --catalog FILE] [--matrix FILE] [--output FILE]

The demo API names and issue demo lists are resolved with the same conservative resolver as the
museum's own catalogue (tools/audit_api_coverage.py), against build/api-audit/public-api.json.
Both source revisions and the catalogue's SHA-256 are written into the report.
"""

from __future__ import annotations

import argparse
import collections
import hashlib
import importlib.util
import json
import pathlib
import subprocess
import sys
from typing import Any

ROOT = pathlib.Path(__file__).resolve().parents[1]
OPEN_STATUSES = {"CONFIRMED_OPEN", "PENDING_VERIFICATION", "DEMO_EXPECTATION_BUG"}


def load_audit_module():
    spec = importlib.util.spec_from_file_location("audit_api_coverage", ROOT / "tools" / "audit_api_coverage.py")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    return module


def git_revision(path: pathlib.Path) -> str:
    completed = subprocess.run(["git", "rev-parse", "HEAD"], cwd=path, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, text=True, check=False)
    return completed.stdout.strip() if completed.returncode == 0 else "unknown"


def normalise(name: str) -> str:
    # Demo catalogues spell members C++-style (Type::Member, Ns::Type::Member); the resolver takes dots.
    name = name.split("(")[0].strip()
    if name.startswith("Microsoft::Xna::Framework::"):
        name = name[len("Microsoft::Xna::Framework::"):]
    for prefix in ("Graphics::", "Audio::", "Input::", "Content::", "Media::", "Net::", "GamerServices::", "Storage::"):
        if name.startswith(prefix) and name.count("::") >= 2:
            name = name[len(prefix):]
    return name.replace("::", ".")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--demos-exe", type=pathlib.Path, default=ROOT.parent / "cna-demos" / "build" / "linux-full" / "bin" / "cna-demos")
    parser.add_argument("--catalog", type=pathlib.Path, help="a saved `cna-demos --catalog-json` instead of running the executable")
    parser.add_argument("--matrix", type=pathlib.Path, default=ROOT.parent / "cna" / "CNA_DEMOS_REPAIR_MATRIX.json")
    parser.add_argument("--output", type=pathlib.Path, default=ROOT / "docs" / "CNA_DEMOS_COMPARISON.md")
    arguments = parser.parse_args()

    if arguments.catalog:
        raw = arguments.catalog.read_bytes()
    else:
        raw = subprocess.run([str(arguments.demos_exe), "--catalog-json"], check=True, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL, timeout=120).stdout
    catalog = json.loads(raw)
    catalog_sha = hashlib.sha256(raw).hexdigest()
    matrix_raw = arguments.matrix.read_bytes()
    matrix = json.loads(matrix_raw)
    audit = load_audit_module()
    inventory = json.loads((ROOT / "build" / "api-audit" / "public-api.json").read_text(encoding="utf-8"))
    evidence = json.loads((ROOT / "build" / "api-audit" / "coverage-evidence.json").read_text(encoding="utf-8"))
    symbols = inventory["symbols"]
    by_id = {symbol["id"]: symbol for symbol in symbols}
    index = audit.build_symbol_suffix_index(symbols)
    museum = {symbol["id"]: symbol for symbol in evidence["symbols"]}

    def level(symbol_id: str) -> str:
        row = museum.get(symbol_id)
        if row is None or not row["catalogLinked"]:
            return "none"
        if row["correctnessEvidence"]["status"] != "unverified":
            return "correctness"
        if row["executionEvidence"]["status"] != "unverified":
            return "executed"
        if row["implementationEvidence"]["status"] != "unverified":
            return "implemented"
        return "catalogue"

    resolved_names: dict[str, list[str]] = {}

    def resolve(name: str) -> list[str]:
        if name not in resolved_names:
            resolution, ids = audit.resolve_claim(normalise(name), symbols, index)
            resolved_names[name] = ids if resolution in ("exact", "family") else []
        return resolved_names[name]

    demo_symbols: dict[str, set[str]] = {}
    unresolved_names: collections.Counter[str] = collections.Counter()
    for demo in catalog["demos"]:
        ids: set[str] = set()
        for name in demo.get("apis", []):
            found = resolve(name)
            if found:
                ids.update(found)
            else:
                unresolved_names[demo["category"]] += 1
        demo_symbols[demo["id"]] = ids

    categories: dict[str, dict[str, Any]] = collections.OrderedDict()
    for demo in sorted(catalog["demos"], key=lambda item: (item["category"], item["subcategory"], item["id"])):
        category = categories.setdefault(demo["category"], {"demos": 0, "symbols": set(), "subcategories": collections.defaultdict(set)})
        category["demos"] += 1
        category["symbols"].update(demo_symbols[demo["id"]])
        category["subcategories"][demo["subcategory"]].update(demo_symbols[demo["id"]])

    lines = [
        "# The museum next to cna-demos",
        "",
        "<!-- Generated by tools/compare_cna_demos.py; do not edit by hand. -->",
        "",
        "cna-demos is a separate repository with its own runs: nothing here is a museum run or a museum PASS.",
        "Demo API names and the issue matrix are resolved against the museum's public API inventory with the",
        "museum's own conservative resolver; a demo name that does not resolve to a declaration is counted, not guessed.",
        "",
        f"- cna-demos revision `{git_revision(ROOT.parent / 'cna-demos')}`, catalogue {catalog.get('demoCount', len(catalog['demos']))} demos, "
        f"`--catalog-json` SHA-256 `{catalog_sha}`",
        f"- issue matrix `{arguments.matrix.name}` SHA-256 `{hashlib.sha256(matrix_raw).hexdigest()}`, baseline CNA "
        f"`{matrix.get('baseline', {}).get('cna_revision', '?')}`, demos `{matrix.get('baseline', {}).get('demos_revision', '?')}`",
        f"- museum public API inventory: CNA `{inventory['source']['revision']}`",
        "",
        "Museum evidence levels: none (no room claims it), catalogue, implemented (reviewed call path),",
        "executed (a current recorded run ran the path), correctness (an exact recorded oracle).",
        "",
        "## Declarations named by demos, per category",
        "",
        "| Category | Demos | Declarations named | Museum: claimed | implemented | executed | Unresolved demo API names |",
        "|---|---:|---:|---:|---:|---:|---:|",
    ]
    missing_areas: list[tuple[str, str, int]] = []
    for name, category in categories.items():
        ids = category["symbols"]
        levels = collections.Counter(level(symbol_id) for symbol_id in ids)
        claimed = len(ids) - levels["none"]
        implemented = levels["implemented"] + levels["executed"] + levels["correctness"]
        executed = levels["executed"] + levels["correctness"]
        lines.append(f"| {name} | {category['demos']} | {len(ids)} | {claimed} | {implemented} | {executed} | {unresolved_names[name]} |")
        for subcategory, sub_ids in sorted(category["subcategories"].items()):
            if sub_ids and all(level(symbol_id) == "none" for symbol_id in sub_ids):
                missing_areas.append((name, subcategory, len(sub_ids)))

    area_rules = evidence.get("areas", {}).get("rules", [])
    header_status: dict[str, str] = {}
    import fnmatch
    for symbol in symbols:
        header = symbol["header"]
        if header in header_status:
            continue
        for rule in area_rules:
            if fnmatch.fnmatchcase(header, rule["match"]):
                header_status[header] = f"{rule['status']}{' ' + rule['backlog'] if rule['backlog'] else ''}"
                break

    lines.extend(
        (
            "",
            "## Demo areas with no museum claim at all",
            "",
            "Each subcategory below names only declarations that no museum room claims. The last column is the",
            "museum's own area classification of their headers (`audit/area_classification.json`), so every such",
            "area is either an open museum task or an explicitly reasoned non-goal.",
            "",
            "| Category | Subcategory | Declarations | Museum area status of their headers |",
            "|---|---|---:|---|",
        )
    )
    for category_name, subcategory, count in missing_areas:
        statuses = sorted({header_status.get(by_id[symbol_id]["header"], "claimed elsewhere in the header")
                           for symbol_id in categories[category_name]["subcategories"][subcategory]})
        lines.append(f"| {category_name} | {subcategory} | {count} | {'; '.join(statuses)} |")
    if not missing_areas:
        lines.append("| - | - | 0 | - |")

    lines.extend(
        (
            "",
            "## Known CNA issues and what the museum says about the same declarations",
            "",
            "Only issues that are not resolved in the matrix are listed. \"Museum levels\" counts the declarations the",
            "issue's demos name by the museum's evidence level; a correctness oracle over such a declaration would",
            "need a second look, everything else is only a claim that the path exists or ran, never a PASS.",
            "",
            "| Issue | Status | Subsystem | Title | Declarations | Museum levels |",
            "|---|---|---|---|---:|---|",
        )
    )
    open_issues = [issue for issue in matrix["issues"] if issue.get("status") in OPEN_STATUSES]
    review = []
    for issue in open_issues:
        ids: set[str] = set()
        for demo_id in issue.get("demos", []):
            ids.update(demo_symbols.get(demo_id, set()))
        levels = collections.Counter(level(symbol_id) for symbol_id in ids)
        if levels["correctness"]:
            review.append(issue["id"])
        summary = ", ".join(f"{key} {value}" for key, value in sorted(levels.items())) or "-"
        title = issue["title"].replace("|", "\\|")
        lines.append(f"| {issue['id']} | {issue['status']} | {issue['subsystem']} | {title} | {len(ids)} | {summary} |")
    lines.extend(
        (
            "",
            f"Open or unverified issues: **{len(open_issues)}** of {len(matrix['issues'])}; issues whose declarations carry a museum "
            f"correctness oracle: **{len(review)}**{' (' + ', '.join(review) + ')' if review else ''}.",
            "",
            "Regenerate with `tools/compare_cna_demos.py` (needs a built cna-demos or a saved `--catalog-json`).",
            "",
        )
    )
    arguments.output.write_text("\n".join(lines), encoding="utf-8")
    print(f"wrote {arguments.output.relative_to(ROOT)}: {len(categories)} categories, {len(missing_areas)} areas with no museum claim, "
          f"{len(open_issues)} open issues, {len(review)} need review")
    return 0


if __name__ == "__main__":
    sys.exit(main())
