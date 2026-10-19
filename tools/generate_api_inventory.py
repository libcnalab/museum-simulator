#!/usr/bin/env python3
"""Generate the museum's complete public CNA declaration inventory.

The CNA C-API project already owns a Doxygen 1.9 parser with stable declaration
identities.  This tool deliberately reuses that parser, but applies the museum's
broader scope: every public C++ header in every CNA module, including build-time,
phone, platform, diagnostics and Inspector modules, plus every public C ABI header.
Internal/detail paths are recorded as exclusions and never enter the denominator.
"""

from __future__ import annotations

import argparse
import collections
import dataclasses
import hashlib
import importlib.util
import json
import pathlib
import re
import subprocess
import sys
import tempfile
import xml.etree.ElementTree as ET
from typing import Any, Iterable


SCHEMA = "museum-cna-public-api/1"
PUBLIC_ROOTS = ("Microsoft", "CNA")
INTERNAL_SEGMENTS = frozenset(("internal", "detail"))
# A physical detail header can still own declarations exported by a documented public facade.
# Keep this list narrow and reviewable; admitting every transitive detail header would turn
# implementation helpers into public API.
PUBLIC_DETAIL_EXPORTS = {
    pathlib.Path(
        "modules/content-pipeline/include/Microsoft/Xna/Framework/Content/Pipeline/"
        "Serialization/Intermediate/detail/IntermediateSerializerCore.hpp"
    ): pathlib.Path(
        "modules/content-pipeline/include/Microsoft/Xna/Framework/Content/Pipeline/"
        "Serialization/Intermediate/IntermediateSerializer.hpp"
    ),
}


@dataclasses.dataclass(frozen=True)
class HeaderInventory:
    included: tuple[pathlib.Path, ...]
    excluded: tuple[pathlib.Path, ...]
    exported_detail: tuple[pathlib.Path, ...]


def repository_root() -> pathlib.Path:
    return pathlib.Path(__file__).resolve().parents[1]


def module_name(header: pathlib.Path) -> str:
    parts = header.parts
    if not parts or parts[0] != "modules" or "include" not in parts:
        raise ValueError(f"not a CNA module header: {header}")
    include_index = parts.index("include")
    if include_index <= 1:
        raise ValueError(f"header has no module component: {header}")
    return "/".join(parts[1:include_index])


def language_name(header: pathlib.Path) -> str:
    return "c" if module_name(header) == "c-api" and header.suffix == ".h" else "cpp"


def is_internal(relative_to_include: pathlib.Path) -> bool:
    return any(part.lower() in INTERNAL_SEGMENTS for part in relative_to_include.parts)


def is_internal_qualified_name(name: str) -> bool:
    return any(part.lower() in INTERNAL_SEGMENTS for part in name.split("::"))


def mentions_internal_qualified_name(value: str) -> bool:
    return re.search(r"(?:^|::)(?:internal|detail)(?:::|$)", value, re.IGNORECASE) is not None


def discover_headers(cna_root: pathlib.Path) -> HeaderInventory:
    modules_root = cna_root / "modules"
    if not modules_root.is_dir():
        raise RuntimeError(f"CNA modules directory does not exist: {modules_root}")

    included: set[pathlib.Path] = set()
    excluded: set[pathlib.Path] = set()
    exported_detail: set[pathlib.Path] = set()
    for include_root in sorted(path for path in modules_root.rglob("include") if path.is_dir()):
        module = include_root.parent.relative_to(modules_root).as_posix()
        for public_root in PUBLIC_ROOTS:
            base = include_root / public_root
            if not base.is_dir():
                continue
            for header in sorted(base.rglob("*")):
                if not header.is_file():
                    continue
                suffixes = {".hpp"}
                if module == "c-api":
                    suffixes.add(".h")
                if header.suffix not in suffixes:
                    continue
                relative = header.relative_to(include_root)
                destination = header.relative_to(cna_root)
                if destination in PUBLIC_DETAIL_EXPORTS:
                    included.add(destination)
                    exported_detail.add(destination)
                else:
                    (excluded if is_internal(relative) else included).add(destination)

    if not included:
        raise RuntimeError("no public CNA headers were discovered")
    overlap = included & excluded
    if overlap:
        raise RuntimeError(f"headers are both included and excluded: {sorted(overlap)}")
    return HeaderInventory(
        tuple(sorted(included)),
        tuple(sorted(excluded)),
        tuple(sorted(exported_detail)),
    )


def load_cna_scanner(cna_root: pathlib.Path) -> Any:
    path = cna_root / "tools" / "c-api" / "generate_coverage_inventory.py"
    if not path.is_file():
        raise RuntimeError(f"canonical CNA Doxygen scanner is missing: {path}")
    spec = importlib.util.spec_from_file_location("cna_coverage_inventory", path)
    if spec is None or spec.loader is None:
        raise RuntimeError(f"cannot load canonical CNA Doxygen scanner: {path}")
    module = importlib.util.module_from_spec(spec)
    sys.modules[spec.name] = module
    spec.loader.exec_module(module)
    required = (
        "COMPOUND_KINDS",
        "MEMBER_COMPOUND_KINDS",
        "PUBLIC_ACCESS",
        "Symbol",
        "discover_include_paths",
        "enum_value_symbols",
        "normalize_location",
        "normalize_space",
        "resolve_doxygen",
        "symbol_from_member",
        "template_signature",
        "xml_text",
    )
    missing = [name for name in required if not hasattr(module, name)]
    if missing:
        raise RuntimeError("canonical CNA scanner lacks: " + ", ".join(missing))
    return module


def run_doxygen(
    cna_root: pathlib.Path,
    headers: tuple[pathlib.Path, ...],
    output_directory: pathlib.Path,
    scanner: Any,
) -> pathlib.Path:
    doxygen = scanner.resolve_doxygen()
    include_paths = scanner.discover_include_paths(cna_root)
    config = (
        "PROJECT_NAME = CNA_Museum_Public_API",
        f"OUTPUT_DIRECTORY = {output_directory}",
        "INPUT = " + " ".join(path.as_posix() for path in headers),
        "FILE_PATTERNS = *.hpp *.h",
        "RECURSIVE = NO",
        "GENERATE_XML = YES",
        "GENERATE_HTML = NO",
        "GENERATE_LATEX = NO",
        "XML_PROGRAMLISTING = NO",
        "EXTRACT_ALL = YES",
        "EXTRACT_PRIVATE = NO",
        "EXTRACT_PACKAGE = YES",
        "EXTRACT_STATIC = YES",
        "EXTRACT_LOCAL_CLASSES = NO",
        "HIDE_UNDOC_MEMBERS = NO",
        "ENABLE_PREPROCESSING = YES",
        "MACRO_EXPANSION = YES",
        "EXPAND_ONLY_PREDEF = NO",
        "SEARCH_INCLUDES = YES",
        "INCLUDE_PATH = " + " ".join(path.as_posix() for path in include_paths),
        "INCLUDE_FILE_PATTERNS = *.h *.hpp",
        "PREDEFINED = CNAEXT= NOXNA= SOUND_ENABLED XNA5 CNA_CNAEXT CNA_DEVICES "
        "CNA_RENDERER_HEADLESS CNA_C_API= CNA_C_API_STATIC",
        "SKIP_FUNCTION_MACROS = NO",
        "HAVE_DOT = NO",
        "SHOW_FILES = NO",
        "SHOW_NAMESPACES = NO",
        "QUIET = YES",
        "WARNINGS = NO",
    )
    completed = subprocess.run(
        [doxygen, "-"],
        cwd=cna_root,
        input="\n".join(config) + "\n",
        text=True,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
    )
    xml_directory = output_directory / "xml"
    if completed.returncode != 0 or not (xml_directory / "index.xml").is_file():
        diagnostics = (completed.stdout + completed.stderr).strip()
        raise RuntimeError(
            f"Doxygen inventory parse failed with exit code {completed.returncode}:\n{diagnostics}"
        )
    return xml_directory


def _compound_symbol(
    cna_root: pathlib.Path,
    compound: ET.Element,
    compound_kind: str,
    compound_name: str,
    scanner: Any,
) -> Any | None:
    access = compound.get("prot")
    if compound_kind not in scanner.COMPOUND_KINDS or access not in scanner.PUBLIC_ACCESS:
        return None
    location = scanner.normalize_location(cna_root, compound.find("location"))
    if location is None:
        return None
    template = scanner.template_signature(compound)
    return scanner.Symbol(
        header=location[0],
        line=location[1],
        kind=compound_kind,
        qualified_name=compound_name,
        signature=scanner.normalize_space(template),
        display=scanner.normalize_space(template + compound_name),
        access=access or "public",
        type_text=compound_kind,
    )


def parse_symbols(
    cna_root: pathlib.Path,
    xml_directory: pathlib.Path,
    headers: tuple[pathlib.Path, ...],
    scanner: Any,
) -> tuple[list[Any], list[Any]]:
    header_names = {path.as_posix() for path in headers}
    c_headers = {path.as_posix() for path in headers if language_name(path) == "c"}
    symbols: list[Any] = []
    for xml_file in sorted(xml_directory.glob("*.xml")):
        try:
            compound = ET.parse(xml_file).getroot().find("compounddef")
        except ET.ParseError as error:
            raise RuntimeError(f"invalid Doxygen XML {xml_file}: {error}") from error
        if compound is None:
            continue
        compound_kind = compound.get("kind", "")
        compound_name = scanner.xml_text(compound.find("compoundname"))
        compound_access = compound.get("prot")

        declaration = _compound_symbol(
            cna_root, compound, compound_kind, compound_name, scanner
        )
        if declaration is not None and declaration.header in header_names:
            symbols.append(declaration)

        if (
            compound_kind not in scanner.MEMBER_COMPOUND_KINDS
            or compound_access == "private"
        ):
            continue
        for member in compound.findall("./sectiondef/memberdef"):
            # The canonical scanner ignores non-macro members of C++ file compounds because their
            # namespace/group compounds carry the declarations. C has no namespace compound, so
            # its public functions and typedefs must be admitted from the file compound.
            location = scanner.normalize_location(cna_root, member.find("location"))
            member_header = location[0] if location is not None else ""
            if (
                compound_kind == "file"
                and member.get("kind") != "define"
                and member_header not in c_headers
            ):
                continue
            symbol = scanner.symbol_from_member(
                cna_root, compound_kind, compound_name, member
            )
            if symbol is None or symbol.header not in header_names:
                continue
            if symbol.header in c_headers and member.get("kind") == "function":
                symbol = dataclasses.replace(symbol, kind="function")
            symbols.append(symbol)
            if symbol.kind == "enum":
                symbols.extend(scanner.enum_value_symbols(cna_root, symbol, member))

    namespace_excluded = [
        symbol for symbol in symbols if is_internal_qualified_name(symbol.qualified_name)
    ]
    symbols = [
        symbol for symbol in symbols if not is_internal_qualified_name(symbol.qualified_name)
    ]
    unique: dict[str, Any] = {}
    for symbol in sorted(symbols, key=lambda item: (item.header, item.line, item.identity)):
        existing = unique.get(symbol.identity)
        if existing is None or symbol.line < existing.line:
            unique[symbol.identity] = symbol
    ids: dict[str, str] = {}
    for symbol in unique.values():
        previous = ids.setdefault(symbol.stable_id, symbol.identity)
        if previous != symbol.identity:
            raise RuntimeError(
                f"stable symbol ID collision {symbol.stable_id}: {previous!r} vs {symbol.identity!r}"
            )
    return (
        sorted(unique.values(), key=lambda item: (item.header, item.line, item.identity)),
        sorted(
            namespace_excluded,
            key=lambda item: (item.header, item.line, item.identity),
        ),
    )


def public_header_sha256(cna_root: pathlib.Path, headers: Iterable[pathlib.Path]) -> str:
    digest = hashlib.sha256()
    for header in headers:
        raw_path = header.as_posix().encode("utf-8")
        body = (cna_root / header).read_bytes()
        digest.update(len(raw_path).to_bytes(8, "big"))
        digest.update(raw_path)
        digest.update(len(body).to_bytes(8, "big"))
        digest.update(body)
    return digest.hexdigest()


def git_text(cna_root: pathlib.Path, *arguments: str) -> str:
    completed = subprocess.run(
        ["git", "-C", str(cna_root), *arguments],
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
        check=False,
    )
    return completed.stdout.strip() if completed.returncode == 0 else "unknown"


def build_payload(
    cna_root: pathlib.Path,
    inventory: HeaderInventory,
    symbols: list[Any],
    doxygen_version: str,
    header_sha256: str,
    namespace_excluded: list[Any],
) -> dict[str, Any]:
    header_counts: collections.Counter[str] = collections.Counter()
    excluded_header_counts: collections.Counter[str] = collections.Counter()
    symbol_counts: collections.Counter[str] = collections.Counter()
    kind_counts: collections.Counter[str] = collections.Counter()
    language_counts: collections.Counter[str] = collections.Counter()
    for header in inventory.included:
        header_counts[module_name(header)] += 1
    for header in inventory.excluded:
        excluded_header_counts[module_name(header)] += 1
    rows = []
    internal_reference_rows = []
    for symbol in symbols:
        header = pathlib.Path(symbol.header)
        module = module_name(header)
        language = language_name(header)
        symbol_counts[module] += 1
        language_counts[language] += 1
        kind_counts[f"{language}:{symbol.kind}"] += 1
        row = {
                "id": symbol.stable_id,
                "language": language,
                "module": module,
                "header": symbol.header,
                "line": symbol.line,
                "access": symbol.access,
                "kind": symbol.kind,
                "qualifiedName": symbol.qualified_name,
                "signature": symbol.signature,
                "display": symbol.display,
                "type": symbol.type_text,
            }
        rows.append(row)
        if mentions_internal_qualified_name(
            " ".join((symbol.signature, symbol.type_text, symbol.display))
        ):
            internal_reference_rows.append(
                {
                    "id": symbol.stable_id,
                    "header": symbol.header,
                    "qualifiedName": symbol.qualified_name,
                    "signature": symbol.signature,
                    "type": symbol.type_text,
                }
            )
    if not language_counts["cpp"] or not language_counts["c"]:
        raise RuntimeError("inventory must contain both the public C++ API and the C ABI")
    if kind_counts["c:method"]:
        raise RuntimeError("C file-scope functions were incorrectly classified as methods")
    if not kind_counts["c:function"]:
        raise RuntimeError("the C ABI inventory contains no callable functions")
    module_names = sorted(set(header_counts) | set(excluded_header_counts))
    modules = [
        {
            "name": name,
            "headers": header_counts[name],
            "excludedHeaders": excluded_header_counts[name],
            "symbols": symbol_counts[name],
        }
        for name in module_names
    ]
    return {
        "schema": SCHEMA,
        "source": {
            "revision": git_text(cna_root, "rev-parse", "HEAD"),
            "description": git_text(cna_root, "describe", "--always", "--dirty"),
            "publicHeaderSha256": header_sha256,
            "doxygenVersion": doxygen_version,
        },
        "policy": {
            "roots": list(PUBLIC_ROOTS),
            "excludedPathSegmentsCaseInsensitive": sorted(INTERNAL_SEGMENTS),
            "excludedNamespaceSegmentsCaseInsensitive": sorted(INTERNAL_SEGMENTS),
            "cpp": "all public/protected declarations in every module's public .hpp headers",
            "c": "all declarations and macros in modules/c-api public .h headers",
            "exportedDetailHeaders": [
                {
                    "header": header.as_posix(),
                    "publicFacade": PUBLIC_DETAIL_EXPORTS[header].as_posix(),
                }
                for header in inventory.exported_detail
            ],
        },
        "counts": {
            "headers": len(inventory.included),
            "excludedHeaders": len(inventory.excluded),
            "excludedNamespaceDeclarations": len(namespace_excluded),
            "publicDeclarationsReferencingInternalTypes": len(internal_reference_rows),
            "symbols": len(symbols),
            "languages": dict(sorted(language_counts.items())),
            "kinds": dict(sorted(kind_counts.items())),
        },
        "modules": modules,
        "excludedHeaders": [path.as_posix() for path in inventory.excluded],
        "excludedNamespaceDeclarations": [
            {
                "header": symbol.header,
                "line": symbol.line,
                "kind": symbol.kind,
                "qualifiedName": symbol.qualified_name,
                "signature": symbol.signature,
            }
            for symbol in namespace_excluded
        ],
        "publicDeclarationsReferencingInternalTypes": internal_reference_rows,
        "symbols": rows,
    }


def render_summary(payload: dict[str, Any]) -> str:
    source = payload["source"]
    counts = payload["counts"]
    languages = counts["languages"]
    lines = [
        "# CNA public API inventory for the museum",
        "",
        "<!-- Generated by tools/generate_api_inventory.py; do not edit by hand. -->",
        "",
        "This is the audit denominator, not a coverage claim. A declaration counts as demonstrated",
        "only after the separate evidence matrix links it to an implemented and executed scenario.",
        "The inventory includes every CNA module, including platform, phone, diagnostics, Inspector",
        "and build-time content pipeline APIs, plus the native C ABI. Paths containing `Internal` or",
        "`Detail` (case-insensitive) are excluded unless a reviewed public facade exports declarations",
        "physically defined there.",
        "",
        f"CNA revision: `{source['revision']}` (`{source['description']}`)",
        "",
        f"Public-header SHA-256: `{source['publicHeaderSha256']}`",
        "",
        f"Doxygen: `{source['doxygenVersion']}`",
        "",
        f"Snapshot: **{counts['headers']} headers**, **{counts['symbols']} declarations** "
        f"(**{languages.get('cpp', 0)} C++**, **{languages.get('c', 0)} C**), "
        f"**{counts['excludedHeaders']} internal/detail headers excluded**, and "
        f"**{counts['excludedNamespaceDeclarations']} declarations in internal/detail namespaces "
        "excluded from otherwise public headers**.",
        f"The snapshot also records **{counts['publicDeclarationsReferencingInternalTypes']} public "
        "declarations whose signatures still reference internal/detail types**; these declarations "
        "remain in the denominator, while the referenced internal types do not.",
        "One reviewed detail header is included through its public `IntermediateSerializer.hpp` facade.",
        "",
        "The complete deterministic JSON is generated in",
        "`build/api-audit/public-api.json` and is intentionally not committed.",
        "",
        "| Module | Public headers | Internal/detail headers | Declarations |",
        "|---|---:|---:|---:|",
    ]
    for module in payload["modules"]:
        lines.append(
            f"| `{module['name']}` | {module['headers']} | {module['excludedHeaders']} | "
            f"{module['symbols']} |"
        )
    lines.extend(
        (
            "",
            "Regenerate or verify with:",
            "",
            "```bash",
            "python3 tools/generate_api_inventory.py --write",
            "python3 tools/generate_api_inventory.py --check",
            "```",
            "",
        )
    )
    return "\n".join(lines)


def declaration_delta(
    previous: dict[str, Any], current: dict[str, Any]
) -> tuple[list[dict[str, Any]], list[dict[str, Any]]]:
    previous_by_id = {symbol["id"]: symbol for symbol in previous.get("symbols", [])}
    current_by_id = {symbol["id"]: symbol for symbol in current.get("symbols", [])}
    added = [current_by_id[key] for key in sorted(current_by_id.keys() - previous_by_id.keys())]
    removed = [previous_by_id[key] for key in sorted(previous_by_id.keys() - current_by_id.keys())]
    return added, removed


def print_declaration_delta(previous: dict[str, Any], current: dict[str, Any]) -> None:
    added, removed = declaration_delta(previous, current)
    print(
        f"declaration delta: +{len(added)} added, -{len(removed)} removed",
        file=sys.stderr,
    )
    for label, rows in (("added", added), ("removed", removed)):
        for symbol in rows[:20]:
            print(
                f"  {label}: {symbol['id']} {symbol['qualifiedName']} {symbol['signature']}",
                file=sys.stderr,
            )
        if len(rows) > 20:
            print(f"  {label}: ... and {len(rows) - 20} more", file=sys.stderr)


def parse_arguments() -> argparse.Namespace:
    root = repository_root()
    parser = argparse.ArgumentParser(description=__doc__)
    mode = parser.add_mutually_exclusive_group(required=True)
    mode.add_argument("--write", action="store_true")
    mode.add_argument("--check", action="store_true")
    parser.add_argument("--cna-root", type=pathlib.Path, default=root.parent / "cna")
    parser.add_argument(
        "--json-output",
        type=pathlib.Path,
        default=root / "build" / "api-audit" / "public-api.json",
    )
    parser.add_argument(
        "--summary-output",
        type=pathlib.Path,
        default=root / "docs" / "CNA_API_INVENTORY.md",
    )
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()
    cna_root = arguments.cna_root.resolve()
    scanner = load_cna_scanner(cna_root)
    inventory = discover_headers(cna_root)
    header_sha256 = public_header_sha256(cna_root, inventory.included)
    temporary_root = repository_root() / "build-probe"
    temporary_root.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="museum-api-inventory-", dir=temporary_root) as tmp:
        xml = run_doxygen(cna_root, inventory.included, pathlib.Path(tmp), scanner)
        symbols, namespace_excluded = parse_symbols(
            cna_root, xml, inventory.included, scanner
        )
    final_inventory = discover_headers(cna_root)
    final_header_sha256 = public_header_sha256(cna_root, final_inventory.included)
    if final_inventory != inventory or final_header_sha256 != header_sha256:
        raise RuntimeError("public CNA headers changed while the inventory was being generated")
    doxygen_path = scanner.resolve_doxygen()
    doxygen_version = subprocess.run(
        [doxygen_path, "--version"],
        stdout=subprocess.PIPE,
        stderr=subprocess.DEVNULL,
        text=True,
        check=True,
    ).stdout.strip()
    payload = build_payload(
        cna_root,
        inventory,
        symbols,
        doxygen_version,
        header_sha256,
        namespace_excluded,
    )
    summary = render_summary(payload)
    rendered_json = json.dumps(payload, indent=2, ensure_ascii=False) + "\n"

    if arguments.write:
        arguments.json_output.parent.mkdir(parents=True, exist_ok=True)
        arguments.summary_output.parent.mkdir(parents=True, exist_ok=True)
        arguments.json_output.write_text(rendered_json, encoding="utf-8")
        arguments.summary_output.write_text(summary, encoding="utf-8")
        print(
            f"CNA public API inventory: {payload['counts']['headers']} headers, "
            f"{payload['counts']['symbols']} declarations; wrote "
            f"{arguments.json_output} and {arguments.summary_output}"
        )
        return 0

    if not arguments.summary_output.is_file():
        print(f"inventory summary is missing: {arguments.summary_output}", file=sys.stderr)
        return 1
    if arguments.summary_output.read_text(encoding="utf-8") != summary:
        print(
            "inventory summary is stale: run tools/generate_api_inventory.py --write",
            file=sys.stderr,
        )
        if arguments.json_output.is_file():
            try:
                previous = json.loads(arguments.json_output.read_text(encoding="utf-8"))
                print_declaration_delta(previous, payload)
            except (OSError, json.JSONDecodeError, KeyError, TypeError):
                print("existing inventory JSON cannot be used for a declaration delta", file=sys.stderr)
        return 1
    if not arguments.json_output.is_file():
        arguments.json_output.parent.mkdir(parents=True, exist_ok=True)
        arguments.json_output.write_text(rendered_json, encoding="utf-8")
        print(f"created build cache {arguments.json_output}")
    if arguments.json_output.read_text(encoding="utf-8") != rendered_json:
        print(
            "inventory JSON is stale: run tools/generate_api_inventory.py --write",
            file=sys.stderr,
        )
        try:
            previous = json.loads(arguments.json_output.read_text(encoding="utf-8"))
            print_declaration_delta(previous, payload)
        except (OSError, json.JSONDecodeError, KeyError, TypeError):
            print("existing inventory JSON cannot be used for a declaration delta", file=sys.stderr)
        return 1
    print(
        f"CNA public API inventory is current: {payload['counts']['headers']} headers, "
        f"{payload['counts']['symbols']} declarations"
    )
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, RuntimeError, subprocess.SubprocessError, json.JSONDecodeError) as error:
        print(f"CNA public API inventory error: {error}", file=sys.stderr)
        raise SystemExit(2) from error
