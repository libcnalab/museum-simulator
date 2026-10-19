#!/usr/bin/env python3

import importlib.util
import pathlib
import sys
import tempfile
import unittest


ROOT = pathlib.Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location(
    "generate_api_inventory", ROOT / "tools" / "generate_api_inventory.py"
)
assert SPEC is not None and SPEC.loader is not None
MODULE = importlib.util.module_from_spec(SPEC)
sys.modules[SPEC.name] = MODULE
SPEC.loader.exec_module(MODULE)


class GenerateApiInventoryTests(unittest.TestCase):
    def test_nested_module_name_and_language(self):
        renderer = pathlib.Path(
            "modules/renderers/easygl/include/CNA/Renderer/Public.hpp"
        )
        c_header = pathlib.Path("modules/c-api/include/CNA/C/runtime.h")
        self.assertEqual(MODULE.module_name(renderer), "renderers/easygl")
        self.assertEqual(MODULE.language_name(renderer), "cpp")
        self.assertEqual(MODULE.language_name(c_header), "c")

    def test_discovery_is_total_and_excludes_internal_paths(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            files = {
                "modules/core/include/CNA/Public.hpp": "public-cpp",
                "modules/core/include/Microsoft/Xna/AlsoPublic.hpp": "public-xna",
                "modules/core/include/CNA/Internal/Hidden.hpp": "hidden",
                "modules/core/include/CNA/deTAIl/HiddenToo.hpp": "hidden",
                "modules/c-api/include/CNA/C/cna.h": "public-c",
                "modules/c-api/include/CNA/C/helper.hpp": "public-cpp-in-c-module",
                "modules/core/include/Private/NotPublic.hpp": "wrong-root",
                "modules/core/include/CNA/NotAHeader.txt": "wrong-suffix",
            }
            for relative, text in files.items():
                path = root / relative
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text(text)

            inventory = MODULE.discover_headers(root)
            included = {path.as_posix() for path in inventory.included}
            excluded = {path.as_posix() for path in inventory.excluded}
            self.assertEqual(
                included,
                {
                    "modules/core/include/CNA/Public.hpp",
                    "modules/core/include/Microsoft/Xna/AlsoPublic.hpp",
                    "modules/c-api/include/CNA/C/cna.h",
                    "modules/c-api/include/CNA/C/helper.hpp",
                },
            )
            self.assertEqual(
                excluded,
                {
                    "modules/core/include/CNA/Internal/Hidden.hpp",
                    "modules/core/include/CNA/deTAIl/HiddenToo.hpp",
                },
            )
            self.assertEqual(inventory.exported_detail, ())

    def test_reviewed_public_facade_admits_only_its_exported_detail_header(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            exported = next(iter(MODULE.PUBLIC_DETAIL_EXPORTS))
            facade = MODULE.PUBLIC_DETAIL_EXPORTS[exported]
            hidden = exported.parent / "UnrelatedHelper.hpp"
            for path in (exported, facade, hidden):
                target = root / path
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_text(path.name)

            inventory = MODULE.discover_headers(root)
            self.assertIn(exported, inventory.included)
            self.assertIn(exported, inventory.exported_detail)
            self.assertNotIn(exported, inventory.excluded)
            self.assertIn(facade, inventory.included)
            self.assertIn(hidden, inventory.excluded)

    def test_header_fingerprint_covers_paths_and_contents(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = pathlib.Path(temporary)
            first = pathlib.Path("modules/a/include/CNA/A.hpp")
            second = pathlib.Path("modules/a/include/CNA/B.hpp")
            for path, body in ((first, "same"), (second, "same")):
                target = root / path
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_text(body)
            original = MODULE.public_header_sha256(root, (first, second))
            (root / second).write_text("changed")
            changed_content = MODULE.public_header_sha256(root, (first, second))
            changed_path = MODULE.public_header_sha256(root, (second, first))
            self.assertNotEqual(original, changed_content)
            self.assertNotEqual(changed_content, changed_path)

    def test_internal_and_detail_namespaces_are_not_public_declarations(self):
        self.assertTrue(MODULE.is_internal_qualified_name("CNA::Internal::Worker"))
        self.assertTrue(MODULE.is_internal_qualified_name("N::detail::Trait"))
        self.assertTrue(MODULE.is_internal_qualified_name("N::Detail::Trait"))
        self.assertFalse(
            MODULE.is_internal_qualified_name(
                "Microsoft::Xna::Framework::Content::Pipeline::IntermediateSerializer"
            )
        )
        self.assertTrue(
            MODULE.mentions_internal_qualified_name(
                "CNA::Internal::Xnb::XnbAssetWriteResult"
            )
        )
        self.assertFalse(
            MODULE.mentions_internal_qualified_name(
                "Microsoft::Xna::Framework::Content::Pipeline::IntermediateSerializer"
            )
        )

    def test_declaration_delta_reports_added_and_removed_stable_ids(self):
        previous = {
            "symbols": [
                {"id": "keep", "qualifiedName": "N::Keep", "signature": ""},
                {"id": "old", "qualifiedName": "N::Old", "signature": "()"},
            ]
        }
        current = {
            "symbols": [
                {"id": "keep", "qualifiedName": "N::Keep", "signature": ""},
                {"id": "new", "qualifiedName": "N::New", "signature": "(int)"},
            ]
        }
        added, removed = MODULE.declaration_delta(previous, current)
        self.assertEqual([symbol["id"] for symbol in added], ["new"])
        self.assertEqual([symbol["id"] for symbol in removed], ["old"])

    def test_internal_type_classification_must_match_leaking_declarations(self):
        payload = {"publicDeclarationsReferencingInternalTypes": [{"id": "a"}, {"id": "b"}]}
        categories = {"plumbing": "backend hook"}

        def write(entries):
            path = pathlib.Path(self.temporary.name) / "classification.json"
            path.write_text(MODULE.json.dumps({
                "schema": MODULE.INTERNAL_CLASSIFICATION_SCHEMA,
                "categories": categories,
                "entries": entries,
            }))
            return path

        entry = lambda key: {"id": key, "category": "plumbing", "museumUse": "not used by the museum"}
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        data = MODULE.load_internal_classification(write([entry("a"), entry("b")]), payload)
        self.assertEqual(len(data["entries"]), 2)
        with self.assertRaisesRegex(RuntimeError, "unclassified b"):
            MODULE.load_internal_classification(write([entry("a")]), payload)
        with self.assertRaisesRegex(RuntimeError, "no longer leaking c"):
            MODULE.load_internal_classification(write([entry("a"), entry("b"), entry("c")]), payload)
        with self.assertRaisesRegex(RuntimeError, "unknown category"):
            MODULE.load_internal_classification(
                write([entry("a"), {"id": "b", "category": "other", "museumUse": "x"}]), payload
            )


if __name__ == "__main__":
    unittest.main()
