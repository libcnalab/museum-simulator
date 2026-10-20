#!/usr/bin/env python3

import gzip
import importlib.util
import json
import pathlib
import sys
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "audit_api_coverage", ROOT / "tools" / "audit_api_coverage.py"
)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


def symbol(identity, name, kind="method", signature="()", language="cpp"):
    return {
        "id": identity,
        "qualifiedName": name,
        "kind": kind,
        "signature": signature,
        "display": f"{name}{signature}",
        "language": language,
    }


class AuditApiCoverageTests(unittest.TestCase):
    def test_exhibit_registry_maps_each_factory_to_its_definition(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            exhibits = root / "src" / "Exhibits"
            exhibits.mkdir(parents=True)
            (exhibits / "ExhibitRegistry.cpp").write_text(
                'constexpr Entry entries[] = {{"lab", MakeLab}};\n'
            )
            (exhibits / "Lab.cpp").write_text(
                "std::unique_ptr<Exhibit> MakeLab(const RoomDef&, Services&) { return {}; }\n"
            )
            sources = MODULE.discover_exhibit_sources(root)
            self.assertEqual(sources["lab"].factory, "MakeLab")
            self.assertEqual(sources["lab"].source, "src/Exhibits/Lab.cpp")
            self.assertEqual(len(MODULE.exhibit_source_sha256(root, sources)), 64)

    def test_property_claim_resolves_getter_and_setter_family(self):
        symbols = [
            symbol("get", "N::BasicEffect::getAlphaProperty"),
            symbol("set", "N::BasicEffect::setAlphaProperty", signature="(float value)"),
        ]
        self.assertEqual(
            MODULE.resolve_claim("BasicEffect.Alpha", symbols),
            ("family", ["get", "set"]),
        )

    def test_ext_accessor_without_property_suffix_resolves(self):
        symbols = [symbol("texture", "N::SpriteFont::getTextureEXT")]
        self.assertEqual(
            MODULE.resolve_claim("SpriteFont.TextureEXT", symbols),
            ("exact", ["texture"]),
        )

    def test_qualified_type_prefers_type_over_constructor(self):
        symbols = [
            symbol("type", "CNA::Diagnostics::CounterHandle", kind="class"),
            symbol("ctor", "CNA::Diagnostics::CounterHandle::CounterHandle"),
        ]
        self.assertEqual(
            MODULE.resolve_claim("CNA::Diagnostics::CounterHandle", symbols),
            ("exact", ["type"]),
        )

    def test_overloads_are_a_single_member_family(self):
        symbols = [
            symbol("a", "N::SpriteBatch::Draw", signature="(Texture2D texture)"),
            symbol("b", "N::SpriteBatch::Draw", signature="(Texture2D texture, Color color)"),
        ]
        self.assertEqual(
            MODULE.resolve_claim("SpriteBatch.Draw", symbols),
            ("family", ["a", "b"]),
        )
        self.assertEqual(
            MODULE.resolve_claim(
                "SpriteBatch.Draw", symbols, MODULE.build_symbol_suffix_index(symbols)
            ),
            ("family", ["a", "b"]),
        )

    def test_argument_qualified_claim_never_widens_to_the_family(self):
        symbols = [
            symbol("a", "N::Texture3D::GetData", signature="(T *data, int elementCount)"),
            symbol("b", "N::Texture3D::GetData", signature="(int level, int left, int front, T *data)"),
        ]
        self.assertEqual(
            MODULE.resolve_claim("Texture3D.GetData(level, left)", symbols), ("exact", ["b"])
        )
        self.assertEqual(
            MODULE.resolve_claim("Texture3D.GetData(level, box)", symbols), ("unresolved", [])
        )

    def test_unresolved_and_non_declaration_stay_explicit(self):
        self.assertEqual(MODULE.resolve_claim("Missing.Api", []), ("unresolved", []))
        self.assertEqual(
            MODULE.resolve_claim("CNAEXT", []), ("non-declaration", [])
        )
        self.assertEqual(
            MODULE.resolve_claim("System.IO.MemoryStream", []),
            ("external-dependency", []),
        )

    def test_new_unclaimed_declaration_stays_unverified(self):
        inventory = {
            "source": {
                "revision": "cna",
                "description": "cna",
                "publicHeaderSha256": "headers",
            },
            "modules": [
                {"name": "m", "headers": 1, "excludedHeaders": 0, "symbols": 2}
            ],
            "symbols": [
                {**symbol("used", "N::Used", kind="class"), "module": "m"},
                {**symbol("new", "N::New", kind="class"), "module": "m"},
            ],
        }
        claims = {
            "rooms": [
                {"key": "room", "exhibit": "lab", "origin": "xna", "apis": ["Used"]}
            ],
            "claims": [{"api": "Used", "rooms": ["room"]}],
        }
        sources = {"lab": MODULE.ExhibitSource("MakeLab", "src/Exhibits/Lab.cpp")}
        evidence = MODULE.build_evidence(
            ROOT,
            inventory,
            claims,
            "claims",
            sources,
            "implementation",
            {"schema": MODULE.IMPLEMENTATION_SCHEMA, "records": []},
            "manifest",
        )
        rows = {row["id"]: row for row in evidence["symbols"]}
        self.assertTrue(rows["used"]["catalogLinked"])
        self.assertFalse(rows["new"]["catalogLinked"])
        self.assertEqual(rows["new"]["status"], "unverified")
        self.assertEqual(evidence["counts"]["implementedDeclarations"], 0)

    def test_reviewed_path_verifies_only_its_recorded_declaration(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            source = root / "src" / "Exhibits" / "Lab.cpp"
            source.parent.mkdir(parents=True)
            source.write_text("void Draw() { widget.Call(); }\n", encoding="utf-8")
            inventory = {
                "source": {"revision": "cna", "description": "cna"},
                "modules": [{"name": "m", "headers": 1, "excludedHeaders": 0, "symbols": 2}],
                "symbols": [
                    {**symbol("call", "N::Widget::Call"), "module": "m"},
                    {**symbol("other", "N::Widget::Other"), "module": "m"},
                ],
            }
            claims = {
                "rooms": [{"key": "room", "exhibit": "lab", "origin": "xna", "apis": ["Widget.Call"]}],
                "claims": [{"api": "Widget.Call", "rooms": ["room"]}],
            }
            manifest = {
                "schema": MODULE.IMPLEMENTATION_SCHEMA,
                "records": [{
                    "id": "room-widget-call",
                    "claim": "Widget.Call",
                    "room": "room",
                    "kind": "direct-call",
                    "symbols": ["call"],
                    "path": [{"source": "src/Exhibits/Lab.cpp", "anchor": "widget.Call();"}],
                    "conditions": [],
                    "note": "The room calls the public method directly.",
                }],
            }
            evidence = MODULE.build_evidence(
                root,
                inventory,
                claims,
                "claims",
                {"lab": MODULE.ExhibitSource("MakeLab", "src/Exhibits/Lab.cpp")},
                "implementation",
                manifest,
                "manifest",
            )
            rows = {row["id"]: row for row in evidence["symbols"]}
            self.assertEqual(evidence["counts"]["implementedDeclarations"], 1)
            self.assertEqual(evidence["counts"]["implementationVerifiedClaims"], 1)
            self.assertEqual(evidence["claims"][0]["implementationEvidence"]["status"], "verified")
            self.assertEqual(evidence["implementationRecords"][0]["path"][0]["line"], 1)
            self.assertEqual(rows["call"]["implementationEvidence"]["status"], "verified")
            self.assertEqual(rows["other"]["implementationEvidence"]["status"], "unverified")

            first_hash = evidence["source"]["implementationEvidenceInputsSha256"]
            source.write_text("// review-context changed\nvoid Draw() { widget.Call(); }\n", encoding="utf-8")
            changed = MODULE.build_evidence(
                root,
                inventory,
                claims,
                "claims",
                {"lab": MODULE.ExhibitSource("MakeLab", "src/Exhibits/Lab.cpp")},
                "implementation",
                manifest,
                "manifest",
            )
            self.assertNotEqual(first_hash, changed["source"]["implementationEvidenceInputsSha256"])
            self.assertEqual(changed["implementationRecords"][0]["path"][0]["line"], 2)

    def test_reviewed_path_rejects_non_unique_anchor(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            source = root / "src" / "Exhibits" / "Lab.cpp"
            source.parent.mkdir(parents=True)
            source.write_text("widget.Call(); widget.Call();\n", encoding="utf-8")
            claim_rows = [{
                "claim": "Widget.Call",
                "symbols": ["call"],
                "rooms": [{"key": "room", "exhibit": "lab"}],
            }]
            manifest = {
                "records": [{
                    "id": "duplicate-anchor",
                    "claim": "Widget.Call",
                    "room": "room",
                    "kind": "direct-call",
                    "symbols": ["call"],
                    "path": [{"source": "src/Exhibits/Lab.cpp", "anchor": "widget.Call();"}],
                    "conditions": [],
                    "note": "Test fixture.",
                }]
            }
            with self.assertRaisesRegex(RuntimeError, "anchor occurs 2 times"):
                MODULE.resolve_implementation_records(
                    root,
                    manifest,
                    claim_rows,
                    {"lab": MODULE.ExhibitSource("MakeLab", "src/Exhibits/Lab.cpp")},
                )

    def test_symbols_from_different_rooms_do_not_combine_into_verified_claim(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            source = root / "src" / "Exhibits" / "Lab.cpp"
            source.parent.mkdir(parents=True)
            source.write_text("widget.Action(1); widget.Action(2.0f);\n", encoding="utf-8")
            inventory = {
                "source": {"revision": "cna", "description": "cna"},
                "modules": [{"name": "m", "headers": 1, "excludedHeaders": 0, "symbols": 2}],
                "symbols": [
                    {**symbol("int", "N::Widget::Action", signature="(int)"), "module": "m"},
                    {**symbol("float", "N::Widget::Action", signature="(float)"), "module": "m"},
                ],
            }
            claims = {
                "rooms": [
                    {"key": "one", "exhibit": "lab", "origin": "xna", "apis": ["Widget.Action"]},
                    {"key": "two", "exhibit": "lab", "origin": "xna", "apis": ["Widget.Action"]},
                ],
                "claims": [{"api": "Widget.Action", "rooms": ["one", "two"]}],
            }
            records = []
            for identity, room, anchor in (
                ("int", "one", "widget.Action(1);"),
                ("float", "two", "widget.Action(2.0f);"),
            ):
                records.append({
                    "id": f"{room}-action",
                    "claim": "Widget.Action",
                    "room": room,
                    "kind": "direct-call",
                    "symbols": [identity],
                    "path": [{"source": "src/Exhibits/Lab.cpp", "anchor": anchor}],
                    "conditions": [],
                    "note": "Only one overload is used in this room.",
                })
            evidence = MODULE.build_evidence(
                root,
                inventory,
                claims,
                "claims",
                {"lab": MODULE.ExhibitSource("MakeLab", "src/Exhibits/Lab.cpp")},
                "implementation",
                {"schema": MODULE.IMPLEMENTATION_SCHEMA, "records": records},
                "manifest",
            )
            implementation = evidence["claims"][0]["implementationEvidence"]
            self.assertEqual(implementation["status"], "partial")
            self.assertEqual(implementation["verifiedSymbols"], ["float", "int"])
            self.assertEqual(implementation["reviewedRooms"], ["one", "two"])
            self.assertEqual(implementation["verifiedRooms"], [])
            self.assertEqual(evidence["counts"]["implementationVerifiedClaims"], 0)
            self.assertEqual(evidence["counts"]["implementationVerifiedClaimRoomPairs"], 0)

    def test_external_dependency_can_have_evidence_without_cna_symbol(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            source = root / "src" / "Exhibits" / "Lab.cpp"
            source.parent.mkdir(parents=True)
            source.write_text("System::IO::MemoryStream stream;\n", encoding="utf-8")
            inventory = {
                "source": {"revision": "cna", "description": "cna"},
                "modules": [{"name": "m", "headers": 0, "excludedHeaders": 0, "symbols": 0}],
                "symbols": [],
            }
            claims = {
                "rooms": [{
                    "key": "room",
                    "exhibit": "lab",
                    "origin": "xna",
                    "apis": ["System.IO.MemoryStream"],
                }],
                "claims": [{"api": "System.IO.MemoryStream", "rooms": ["room"]}],
            }
            manifest = {
                "schema": MODULE.IMPLEMENTATION_SCHEMA,
                "records": [{
                    "id": "room-memory-stream",
                    "claim": "System.IO.MemoryStream",
                    "room": "room",
                    "kind": "direct-use",
                    "symbols": [],
                    "path": [{
                        "source": "src/Exhibits/Lab.cpp",
                        "anchor": "System::IO::MemoryStream stream;",
                    }],
                    "conditions": [],
                    "note": "The room constructs the external dependency type directly.",
                }],
            }
            evidence = MODULE.build_evidence(
                root,
                inventory,
                claims,
                "claims",
                {"lab": MODULE.ExhibitSource("MakeLab", "src/Exhibits/Lab.cpp")},
                "implementation",
                manifest,
                "manifest",
            )
            self.assertEqual(evidence["claims"][0]["implementationEvidence"]["status"], "verified")
            self.assertEqual(evidence["counts"]["implementedDeclarations"], 0)
            self.assertEqual(evidence["counts"]["implementationVerifiedClaims"], 1)
            self.assertEqual(evidence["counts"]["implementationVerifiedClaimRoomPairs"], 1)

    def test_execution_run_requires_hashed_clean_complete_artifacts(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            script_path = root / "audit" / "run.script"
            log_path = root / "audit" / "run.log"
            script_path.parent.mkdir(parents=True)
            script_path.write_text(
                "goto:room;wait:2;pressall;wait:2;stats;quit\n", encoding="utf-8"
            )
            log_path.write_text(
                "[museum][script] goto room\n"
                "[museum][script] pressall: 1 panel(s) exercised\n"
                "[museum][stats] visited 1 exhibit errors 0\n",
                encoding="utf-8",
            )
            implementation = [{
                "id": "room-call",
                "claim": "Widget.Call",
                "room": "room",
                "symbols": ["call"],
            }]
            manifest = {
                "runs": [{
                    "id": "run",
                    "rooms": ["room"],
                    "museumBinarySha256": "museum",
                    "cnaBinarySha256": "cna",
                    "script": {
                        "path": "audit/run.script",
                        "sha256": MODULE.sha256_file(script_path),
                    },
                    "log": {
                        "path": "audit/run.log",
                        "sha256": MODULE.sha256_file(log_path),
                    },
                }],
                "records": [{
                    "id": "execution",
                    "run": "run",
                    "implementationRecords": ["room-call"],
                    "note": "The focused run executes the reviewed room path.",
                }],
                "oracles": [{
                    "id": "oracle",
                    "run": "run",
                    "implementationRecords": ["room-call"],
                    "witness": "[museum][stats] visited 1 exhibit errors 0",
                    "expectation": "The run completes with zero exhibit errors.",
                    "note": "The exact final stats line is retained in the hashed log.",
                }],
            }
            runs, records, oracles, inputs_sha = MODULE.resolve_execution_evidence(
                root, manifest, implementation, "museum", "cna"
            )
            self.assertTrue(runs[0]["currentBuild"])
            self.assertTrue(records[0]["currentBuild"])
            self.assertTrue(oracles[0]["currentBuild"])
            self.assertEqual(len(inputs_sha), 64)
            stale_runs, _, _, _ = MODULE.resolve_execution_evidence(
                root, manifest, implementation, "different", "cna"
            )
            self.assertFalse(stale_runs[0]["currentBuild"])

            log_path.write_text(
                "[museum][script] goto room\n[museum][stats] exhibit errors 0\n",
                encoding="utf-8",
            )
            manifest["runs"][0]["log"]["sha256"] = MODULE.sha256_file(log_path)
            with self.assertRaisesRegex(RuntimeError, "successful pressall"):
                MODULE.resolve_execution_evidence(
                    root, manifest, implementation, "museum", "cna"
                )

            for bad_log, message in (
                (
                    "[museum][script] goto room\n"
                    "[museum][script] pressall: 1 panel(s) exercised\n"
                    "[museum][stats] visited 1 exhibit errors 0  script errors 1\n",
                    "script errors",
                ),
                (
                    "[museum][script] goto room\n"
                    "[museum][script] pressall: 1 panel(s) exercised\n"
                    "[museum][script-error] press: no control matches 'Gone'\n"
                    "[museum][stats] visited 1 exhibit errors 0\n",
                    "execution error",
                ),
            ):
                with self.subTest(message=message):
                    log_path.write_text(bad_log, encoding="utf-8")
                    manifest["runs"][0]["log"]["sha256"] = MODULE.sha256_file(log_path)
                    with self.assertRaisesRegex(RuntimeError, message):
                        MODULE.resolve_execution_evidence(
                            root, manifest, implementation, "museum", "cna"
                        )

    def test_per_room_run_declares_rooms_whose_sweep_found_no_panel(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            (root / "audit").mkdir()
            script_path = root / "audit" / "run.script"
            log_path = root / "audit" / "run.log"
            lines_path = root / "audit" / "run.lines.json.gz"
            script_path.write_text("goto:a;pressall;stats;quit;goto:b;pressall;stats;quit\n", encoding="utf-8")
            log_path.write_text(
                "[museum][script] goto a\n[museum][script] pressall: 2 panel(s) exercised\n[museum][stats] exhibit errors 0\n"
                "[museum][script] goto b\n[museum][script] pressall: 0 panel(s) exercised\n[museum][stats] exhibit errors 0\n",
                encoding="utf-8",
            )
            lines_path.write_bytes(gzip.compress(json.dumps(
                {"schema": MODULE.LINE_COVERAGE_SCHEMA, "sources": {}, "rooms": {"a": {}, "b": {}}}).encode()))
            run = {
                "id": "run", "rooms": ["a", "b"], "interaction": "per-room-coverage", "roomsWithoutControls": ["b"],
                "museumBinarySha256": "museum", "cnaBinarySha256": "cna",
                "script": {"path": "audit/run.script", "sha256": MODULE.sha256_file(script_path)},
                "log": {"path": "audit/run.log", "sha256": MODULE.sha256_file(log_path)},
                "lineCoverage": {"path": "audit/run.lines.json.gz", "sha256": MODULE.sha256_file(lines_path)},
            }
            manifest = {"runs": [run], "records": [], "oracles": []}
            runs, _, _, _ = MODULE.resolve_execution_evidence(root, manifest, [], "museum", "cna")
            self.assertEqual(runs[0]["id"], "run")
            for declared in ([], ["a"], ["a", "b"]):
                with self.subTest(declared=declared):
                    run["roomsWithoutControls"] = declared
                    with self.assertRaisesRegex(RuntimeError, "roomsWithoutControls"):
                        MODULE.resolve_execution_evidence(root, manifest, [], "museum", "cna")

    def test_every_unclaimed_header_needs_one_current_area_rule(self):
        inventory = {"symbols": [
            {"id": "a", "header": "m/include/A.hpp"},
            {"id": "b", "header": "m/include/B.hpp"},
            {"id": "c", "header": "m/include/sub/C.hpp"},
        ]}
        evidence = [{"id": "a", "catalogLinked": True}, {"id": "b", "catalogLinked": False}, {"id": "c", "catalogLinked": False}]
        statuses = {"gap": "g", "supporting": "s", "not-applicable": "n"}

        def manifest(rules):
            return {"schema": MODULE.AREA_SCHEMA, "statuses": statuses, "rules": rules}

        def rule(match, status="supporting", backlog=""):
            return {"match": match, "area": "x", "status": status, "backlog": backlog, "reason": "r"}

        areas = MODULE.classify_areas(inventory, evidence, manifest([rule("m/include/sub/*", "gap", "C01"), rule("m/include/*")]))
        self.assertEqual(areas["demonstratedHeaders"], 1)
        self.assertEqual(areas["classified"], {"gap": {"headers": 1, "declarations": 1}, "supporting": {"headers": 1, "declarations": 1}})
        with self.assertRaisesRegex(RuntimeError, "no area classification"):
            MODULE.classify_areas(inventory, evidence, manifest([rule("m/include/B.hpp")]))
        with self.assertRaisesRegex(RuntimeError, "match no unclaimed header"):
            MODULE.classify_areas(inventory, evidence, manifest([rule("m/include/*"), rule("m/include/A.hpp")]))
        with self.assertRaisesRegex(RuntimeError, "backlog"):
            MODULE.classify_areas(inventory, evidence, manifest([rule("m/include/*", "gap", "")]))

    def test_line_coverage_executes_only_paths_that_ran_in_their_own_room(self):
        records = [
            {"id": "direct", "room": "a", "path": [{"source": "src/X.cpp", "line": 10, "anchor": "call();"}]},
            {"id": "multi", "room": "a", "path": [{"source": "src/X.cpp", "line": 20, "anchor": "f(\n  x);"}]},
            {"id": "indirect", "room": "a", "path": [
                {"source": "src/Helper.cpp", "line": 5, "anchor": "helper();"},
                {"source": "src/X.cpp", "line": 10, "anchor": "call();"},
            ]},
            {"id": "helper-not-run", "room": "a", "path": [
                {"source": "src/Helper.cpp", "line": 9, "anchor": "other();"},
                {"source": "src/X.cpp", "line": 10, "anchor": "call();"},
            ]},
            {"id": "other-room", "room": "b", "path": [{"source": "src/X.cpp", "line": 10, "anchor": "call();"}]},
            {"id": "unvisited", "room": "c", "path": [{"source": "src/X.cpp", "line": 10, "anchor": "call();"}]},
        ]
        coverage = {"a": {"src/X.cpp": "10,21", "src/Helper.cpp": "4-6"}, "b": {"src/X.cpp": "11-30"}}
        self.assertEqual(
            MODULE.executed_by_line_coverage(records, coverage), ["direct", "indirect", "multi"]
        )


if __name__ == "__main__":
    unittest.main()
