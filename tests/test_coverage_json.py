#!/usr/bin/env python3

import argparse
import collections
import json
import subprocess
import unittest


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser()
    parser.add_argument("--bin", required=True)
    return parser.parse_args()


class CoverageJsonTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        arguments = parse_args()
        completed = subprocess.run(
            [arguments.bin, "--coverage-json"],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            check=True,
        )
        cls.payload = json.loads(completed.stdout)

    def test_schema_and_room_records_are_complete(self):
        self.assertEqual(self.payload["schema"], "museum-api-claims/1")
        rooms = self.payload["rooms"]
        self.assertGreater(len(rooms), 0)
        self.assertEqual(len({room["key"] for room in rooms}), len(rooms))
        for room in rooms:
            self.assertTrue(room["key"])
            self.assertTrue(room["title"])
            self.assertTrue(room["exhibit"])
            self.assertIn(room["origin"], {"xna", "cna-extension", "application"})
            self.assertTrue(room["apis"])
            self.assertEqual(len(room["apis"]), len(set(room["apis"])))

    def test_claim_index_is_the_exact_reverse_room_mapping(self):
        expected = collections.defaultdict(list)
        for room in self.payload["rooms"]:
            for api in room["apis"]:
                expected[api].append(room["key"])
        actual = {claim["api"]: claim["rooms"] for claim in self.payload["claims"]}
        self.assertEqual(actual, dict(sorted(expected.items())))
        self.assertEqual(len(actual), len(self.payload["claims"]))

    def test_removed_renderer_maturity_name_is_not_claimed(self):
        claims = {claim["api"] for claim in self.payload["claims"]}
        self.assertIn("CNA::getGraphicsRendererMaturity", claims)
        self.assertNotIn("CNA::getGraphicsBackendMaturity", claims)


if __name__ == "__main__":
    unittest.main(argv=[__file__])
