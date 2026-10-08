#!/usr/bin/env python3
"""Independent expected literals/negative cases; not converter-generated oracle."""
import copy
import importlib.util
import json
from pathlib import Path
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("conversion", ROOT / "tools/convert_step2f.py")
c = importlib.util.module_from_spec(spec)
spec.loader.exec_module(c)
DATA = ROOT / "resources/step2f"
BUILDER = Path(sys.argv.pop(1)).resolve()


class ConversionTests(unittest.TestCase):
    def setUp(self):
        self.t, self.manifest, self.contract = c.load_source(DATA / "source_v1_1.zip")
        self.snapshot = json.loads((DATA / "candidate_adoption.json").read_text())
        self.manual = json.loads((DATA / "full_pinyin.json").read_text())

    def prepared(self):
        return c.prepare(self.t, self.manifest, self.snapshot, self.manual)

    def test_F1_source_and_complete_dispositions(self):
        anchors, mappings, inventory = self.prepared()
        self.assertEqual([len(inventory[n]) for n in c.TABLES], [42, 42, 42, 128, 8, 20])
        self.assertEqual(len(anchors), 13)
        self.assertEqual(len(mappings), 25)
        self.assertEqual(sum(bool(x["m4_stable_lexeme_key"]) for x in self.t["forms"]), 31)
        self.assertEqual(sum(x["runtime_condition_required"] == "true" for x in self.t["relations"]), 10)
        self.assertTrue(all(row["source_row_sha256"] and row["disposition"] for rows in inventory.values() for row in rows))
        with tempfile.TemporaryDirectory() as td:
            corrupt = Path(td) / "wrong.zip"
            corrupt.write_bytes((DATA / "source_v1_1.zip").read_bytes() + b"changed")
            with self.assertRaisesRegex(ValueError, "source ZIP"):
                c.load_source(corrupt)

    def test_F2_reference_condition_errors(self):
        for relation, field, value in (("R004", "target_form_key", "not-a-form"),
                                       ("R002", "runtime_condition", "wrongly copied old note"),
                                       ("R011", "runtime_condition_required", "false"),
                                       ("R036", "availability", "profile_or_explicit")):
            t = copy.deepcopy(self.t)
            next(x for x in t["relations"] if x["legacy_relation_id"] == relation)[field] = value
            with self.assertRaises(ValueError):
                c.validate_tables(t, self.contract)
        t = copy.deepcopy(self.t)
        t["output_candidates"][1]["linked_relation_edge_ids"] = "missing-edge"
        with self.assertRaisesRegex(ValueError, "output_candidates:3"):
            c.validate_tables(t, self.contract)
        contract = copy.deepcopy(self.contract)
        contract["edges"][0]["target_text"] = "wrong"
        with self.assertRaisesRegex(ValueError, "contract field"):
            c.validate_tables(self.t, contract)

    def test_F3_key_adoption_and_manual_mapping(self):
        for field, value in (("source_key", "SLK1:" + "1" * 64), ("default_output", "not original"),
                             ("source_file_sha256", "0" * 64), ("formal_adoption", "Approved")):
            s = copy.deepcopy(self.snapshot)
            s["anchors"][0][field] = value
            with self.assertRaises(ValueError):
                c.prepare(self.t, self.manifest, s, self.manual)
        manual = copy.deepcopy(self.manual)
        manual["entries"][0]["source_key"] = "SLK1:" + "1" * 64
        with self.assertRaisesRegex(ValueError, "binding"):
            c.prepare(self.t, self.manifest, self.snapshot, manual)
        anchors, mappings, _ = self.prepared()
        ren = next(a for a in anchors if a["id"] == "ren")
        self.assertEqual(ren["source_key"], "SLK1:8c816cf96715d449d21f7bd8ff7335bd39b9f6ddbecae776649bbf3bdbd7b0db")
        self.assertEqual({m["code"] for m in mappings if m["source_key"] == ren["source_key"]}, {"ren", "rf"})
        self.assertEqual({m["scheme_id"] for m in mappings if m["anchor_id"] == "form-52460004"}, {"xiaohe"})
        self.assertEqual({a["source_key"] for a in anchors if a["text"] in ("时", "時")}, {
            "SLK1:3da586e699dd499839e5febcc599199fdf81873e7d271eee13e3bf4ec3d68913",
            "SLK1:52a02b7d8c3e37b29d9eb66bdceb4220f55071ba324cfe786d97f3d2504e3656"})

    def test_F4_output_gates_and_supplementary(self):
        anchors, _, inventory = self.prepared()
        by_text = {a["text"]: a for a in anchors}
        o = lambda text, form: [r for r in by_text[text]["outputs"] if r["form"] == form]
        self.assertEqual(o("机器", "opencc-s2t-1.4.2")[0]["text"], "機器")
        self.assertFalse(o("机", "relation:R002")[0]["condition"])
        self.assertTrue(o("后", "relation:R011")[0]["condition"])
        self.assertFalse(o("后", "source-preserve")[0]["condition"])
        self.assertEqual(o("才", "relation:R036")[0]["availability"], 6)
        self.assertTrue(o("才", "relation:R036")[0]["condition"])
        self.assertEqual(len(o("发", "opencc-s2t-1.4.2")), 2)
        self.assertTrue(all(x["condition"] for x in o("发", "opencc-s2t-1.4.2")))
        self.assertTrue(o("發", "opencc-s2t-1.4.2")[0]["blocked"])
        self.assertTrue(o("面条", "relation:R028")[0]["blocked"])
        self.assertTrue(o("人", "product-default-traditional")[0]["blocked"])
        u = next(x for x in inventory["forms"] if x["source"]["text"] == "𨿍")
        self.assertEqual(json.loads(json.dumps(u, ensure_ascii=False))["source"]["text"].encode(), b"\xf0\xa8\xbf\x8d")
        self.assertIn("PendingIdentity", u["disposition"])
        self.assertNotIn("𨿍", by_text)

    def test_F5_determinism_and_deliberate_reorder(self):
        with tempfile.TemporaryDirectory() as td:
            for name, reverse in (("a", False), ("again", False), ("b", True)):
                c.convert(DATA / "source_v1_1.zip", DATA / "candidate_adoption.json", DATA / "full_pinyin.json", BUILDER, Path(td) / name, reverse)
            a, again, b = (Path(td) / n for n in ("a", "again", "b"))
            for name in ("shared.hns", "full_pinyin.hnm", "xiaohe.hnm", "manifest.json", "build_input.txt"):
                self.assertEqual((a / name).read_bytes(), (again / name).read_bytes())
            ma, mb = (json.loads((p / "manifest.json").read_text()) for p in (a, b))
            ka = {x["source_key"]: x["runtime_ref"] for x in ma["runtime_anchors"]}
            kb = {x["source_key"]: x["runtime_ref"] for x in mb["runtime_anchors"]}
            self.assertEqual(set(ka), set(kb))
            self.assertNotEqual(ka, kb)
            self.assertNotEqual(ma["resource_builder"]["shared_bundle_id"], mb["resource_builder"]["shared_bundle_id"])


if __name__ == "__main__":
    unittest.main(verbosity=2)
