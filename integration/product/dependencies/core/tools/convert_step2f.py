#!/usr/bin/env python3
"""Bounded v1.1 *build-side* importer. No old generator, OpenCC or key allocator.

Python's standard library reads fixed UTF-8 TSV evidence, an explicit candidate
snapshot and manual full-code table. Full tables are Host BUILD memory only.
The runtime consumes the existing ResourceBuilder wire format, not this module.
"""
import argparse
import csv
import hashlib
import io
import json
from pathlib import Path
import re
import subprocess
import zipfile

SOURCE_SHA = "9af95f797a48ef0c1dd014e1388a9effde25a1996afafaca838686a0ac56d285"
CONTRACT_SHA = "71dd76d0ee16952953c164097de772179123eaa19d094da57a2febcf5c32df90"
TRIAL_NS = "0x54465249414c0001"
PREFIX = "HANIME_SimplifiedTraditional_IdentityHandoff_v1.1/"
TABLES = ("forms", "identity_evidence", "relations", "output_candidates",
          "same_text_identity_cases", "pending_items")
AVAIL = {"profile_or_explicit": 3, "expanded_or_explicit": 6,
         "condition_required": 3, "condition_required_expanded_only": 6}


def require(value, record, reason):
    if not value:
        raise ValueError(f"{record}: {reason}")


def digest(data):
    return hashlib.sha256(data).hexdigest()


def canonical(value):
    return json.dumps(value, ensure_ascii=False, sort_keys=True, separators=(",", ":")).encode("utf-8")


def slk(value, where):
    require(re.fullmatch(r"SLK1:[0-9a-f]{64}", value) and int(value[5:], 16), where, "missing/invalid full SLK1 key")
    return value


def table(data, name):
    text = data.decode("utf-8", errors="strict")
    rows = list(csv.reader(io.StringIO(text), delimiter="\t"))
    require(rows and len(set(rows[0])) == len(rows[0]), name, "invalid header")
    for i, row in enumerate(rows[1:], 2):
        require(len(row) == len(rows[0]), f"{name}:{i}", "column count mismatch")
    return [dict(zip(rows[0], row)) for row in rows[1:]]


def load_source(path):
    raw = Path(path).read_bytes()
    require(digest(raw) == SOURCE_SHA, "source ZIP", "not the fixed reviewed v1.1 package")
    with zipfile.ZipFile(io.BytesIO(raw)) as archive:
        require(archive.testzip() is None, "source ZIP", "CRC failure")
        manifest = json.loads(archive.read(PREFIX + "MANIFEST.json"))
        # Validate all archive bytes, but never execute or parse build_handoff.py.
        for name, sha in manifest["files_sha256"].items():
            require(digest(archive.read(PREFIX + name)) == sha, name, "file SHA-256 mismatch")
        tables = {n: table(archive.read(PREFIX + "data/" + n + ".tsv"), n) for n in TABLES}
        contract = json.loads(archive.read(PREFIX + "generated/relation_semantics_contract_v2.json"))
    identity = contract["identity"]
    body = {k: v for k, v in contract.items() if k != "identity"}
    sha = digest(canonical(body))
    require(contract["contract_version"] == identity["relation_version"] == 2 and
            sha == CONTRACT_SHA == identity["relation_sha256"] and
            identity["relation_checksum32"] == "0x" + sha[:8].upper(), "condition contract", "version/checksum32/full SHA mismatch")
    validate_tables(tables, contract)
    return tables, manifest, contract


def validate_tables(t, contract):
    forms = {r["handoff_form_key"]: r for r in t["forms"]}
    edges = {r["relation_edge_id"]: r for r in t["relations"]}
    require(len(forms) == len(t["forms"]) == 42 and len(edges) == len(t["relations"]) == 42,
            "tables", "duplicate/missing form or edge")
    ie = {r["handoff_form_key"]: r for r in t["identity_evidence"]}
    require(set(ie) == set(forms), "identity_evidence", "form coverage mismatch")
    for k, r in forms.items():
        require(r["text"].encode().hex().upper() == r["utf8_hex"].replace(" ", ""), k, "UTF-8 mismatch")
        require(r["record_kind"] == ("character" if len(r["text"]) == 1 else "word"), k, "record kind mismatch")
        key = r["m4_stable_lexeme_key"]
        if key:
            slk(key, k)
        require(ie[k]["long_term_key"] == key and ie[k]["text"] == r["text"], k, "identity evidence mismatch")
    ce = {e["relation_edge_id"]: e for e in contract["edges"]}
    require(set(ce) == set(edges), "contract", "edge coverage mismatch")
    for k, r in edges.items():
        for side in ("source", "target"):
            f = r[side + "_form_key"]
            require(f in forms, r["legacy_relation_id"], f"unknown {side} form {f}")
            require(r[side + "_text"] == forms[f]["text"] and
                    r[side + "_long_term_key"] == forms[f]["m4_stable_lexeme_key"], k, "form/text/key disagreement")
        required = r["runtime_condition_required"]
        require(required in ("true", "false"), k, "invalid condition boolean")
        require((r["runtime_condition_kind"] == "WORD_OR_SENSE_PREREQUISITE" and bool(r["runtime_condition"]))
                if required == "true" else (r["runtime_condition_kind"] == "NONE" and not r["runtime_condition"]),
                r["legacy_relation_id"], "condition kind/text inconsistent")
        require(r["availability"] in AVAIL, k, "unsupported availability")
        for field, expected in ce[k].items():
            actual = r.get(field)
            if isinstance(expected, bool):
                actual = actual == "true"
            elif isinstance(expected, list):
                actual = actual.split("|") if actual else []
            require(actual == expected, r["legacy_relation_id"], "contract field mismatch: " + field)
    seen = set()
    for i, r in enumerate(t["output_candidates"], 2):
        loc = f"output_candidates:{i}"
        pair = (r["source_form_key"], r["profile_key"])
        require(pair not in seen, loc, "duplicate source/profile")
        seen.add(pair)
        for col, text_col in (("source_form_key", "source_text"), ("output_form_key", "output_text")):
            require(r[col] in forms and forms[r[col]]["text"] == r[text_col], loc, "broken form reference")
        linked = r["linked_relation_edge_ids"].split("|") if r["linked_relation_edge_ids"] else []
        for edge in linked:
            require(edge in edges and edges[edge]["source_form_key"] == r["source_form_key"] and
                    edges[edge]["target_form_key"] == r["output_form_key"], loc, "broken linked edge " + edge)
        require(r["linked_runtime_condition_required"] == ("true" if any(edges[e]["runtime_condition_required"] == "true" for e in linked) else "false"), loc, "lost linked condition")
        if r["candidate_role"] == "SOURCE_PRESERVE_ROUTE":
            require(r["source_form_key"] == r["output_form_key"] and not linked, loc, "invalid preserve route")
        else:
            require(r["candidate_role"] == "TOOL_OUTPUT_OBSERVATION", loc, "unknown candidate role")
    require(len(seen) == 128, "outputs", "coverage count mismatch")
    same = t["same_text_identity_cases"]
    require(len(same) == 8 and len({r["m6_binding_stable_key"] for r in same}) == 8, "same_text", "lost distinct source identities")
    for r in same:
        key = slk(r["m6_binding_stable_key"], r["baseline_runtime_lexeme_ref"])
        require(key == r["m4_key_for_canonical_surface"] and r["baseline_runtime_lexeme_ref"] == r["m6_binding_runtime_ref"] and
                r["output_text"] == r["traditional_field"], "same_text", "M4/M6 binding mismatch")


def prepare(t, source_manifest, snapshot, manual):
    require(snapshot["trial_namespace"] == TRIAL_NS and snapshot["purpose"] == "ISOLATED_HOST_TRIAL_NOT_PRODUCTION" and
            snapshot["source_zip_sha256"] == SOURCE_SHA, "snapshot", "trial namespace/purpose/source mismatch")
    forms = {r["handoff_form_key"]: r for r in t["forms"]}
    same = {r["baseline_runtime_lexeme_ref"]: r for r in t["same_text_identity_cases"]}
    relations = {r["legacy_relation_id"]: r for r in t["relations"]}
    anchors, mappings = [], []
    seen_keys, seen_ids = set(), set()
    by_form = {}
    for a in snapshot["anchors"]:
        loc = a["id"]
        slk(a["source_key"], loc)
        require(loc not in seen_ids and a["source_key"] not in seen_keys, loc, "duplicate anchor/key")
        seen_ids.add(loc); seen_keys.add(a["source_key"])
        require(a["trial_status"] == "TrialAdmitted" and a["formal_adoption"] == "PendingReview" and a["base_weight"] == 0, loc, "not a neutral candidate trial")
        name = a["source_table"]
        require(name in ("forms", "same_text_identity_cases"), loc, "unsupported source table")
        require(a["source_file_sha256"] == source_manifest["files_sha256"]["data/" + name + ".tsv"], loc, "source file hash mismatch")
        rows = forms if name == "forms" else same
        require(a["source_record"] in rows, loc, "unknown source record")
        r = rows[a["source_record"]]
        key = r["m4_stable_lexeme_key"] if name == "forms" else r["m4_key_for_canonical_surface"]
        original = r["text"] if name == "forms" else r["simplified_field"]
        require(a["source_key"] == key and a["text"] == original and a["default_output"] == original and
                a["kind"] == ("character" if len(original) == 1 else "word"), loc, "source key/text/kind/default changed")
        require(set(a["approved_edges"]) <= set(relations), loc, "unknown approved edge")
        for e in a["approved_edges"]:
            require(name == "forms" and relations[e]["source_form_key"] == a["source_record"] and
                    relations[e]["language_data_set"] == "REVIEWED_LANGUAGE_RELATION_SAMPLE", loc, "edge not reviewed or wrong owner: " + e)
        item = dict(a, source_row_sha256=digest(canonical(r)), outputs=[])
        def output(form, text, condition="", availability=7, blocked=False, basis="source-preserve"):
            item["outputs"].append(dict(form=form, text=text, condition=condition,
                                        availability=availability, blocked=blocked, basis=basis))
        output("source-preserve", original)
        # Named unresolved product default is known but not selected. Not fallback.
        output("product-default-traditional", "", blocked=True, basis="ProfileNotSelected; no product default adopted")
        if name == "forms":
            by_form[a["source_record"]] = item
            for e, rel in relations.items():
                if rel["source_form_key"] != a["source_record"]:
                    continue
                admitted = e in a["approved_edges"]
                condition = rel["runtime_condition"] if rel["runtime_condition_required"] == "true" else ""
                # Edge aliases allow inspecting one relation without changing its gate.
                selectors = ["relation:" + e] + ([v for v in rel["profile_keys"].split("|") if v])
                for selector in selectors:
                    output(selector, rel["target_text"], condition, AVAIL[rel["availability"]], not admitted,
                           e + (" trial attachment; condition retained" if admitted else " OutputNotReady: attachment not trial-admitted"))
            for o in t["output_candidates"]:
                if o["source_form_key"] != a["source_record"] or o["candidate_role"] == "SOURCE_PRESERVE_ROUTE":
                    continue
                if not any(x["form"] == o["profile_key"] for x in item["outputs"]):
                    output(o["profile_key"], o["output_text"], blocked=True, basis="ObservationOnly; no admitted relation")
        else:
            require(a["same_text_output"] and not a["approved_edges"], loc, "same-text route not explicitly admitted")
            output("m6-traditional", r["traditional_field"], availability=3,
                   basis="Step2F trial + same_text_identity_cases:" + a["source_record"])
        for code in r["xiaohe_codes"].split("|"):
            if code:
                require(re.fullmatch("[a-z]+", code), loc, "invalid actual xiaohe code")
                mappings.append(dict(anchor_id=loc, source_key=key, scheme_id="xiaohe", code=code,
                                     scheme_weight=0, status="TrialAdmittedActualSource", basis=name + ":" + a["source_record"],
                                     weight_basis="neutral trial, no linguistic frequency"))
        anchors.append(item)
    require(manual["authority"] == "NEW_MANUAL_PROVISIONAL_READING_NOT_EXPORTED_FROM_XIAOHE", "full_pinyin", "missing manual provenance")
    require(len(manual["entries"]) == len(anchors) and {r["anchor_id"] for r in manual["entries"]} == seen_ids, "full_pinyin", "anchor coverage mismatch")
    for row in manual["entries"]:
        a = next(a for a in anchors if a["id"] == row["anchor_id"])
        require(row["source_key"] == a["source_key"] and row["source_text"] == a["text"] and row["scheme_id"] == "full_pinyin" and row["scheme_weight"] == 0,
                row["anchor_id"], "manual binding/weight mismatch")
        if row["status"] == "TrialAdmittedManualPendingReview":
            require(re.fullmatch("[a-z]+", row["code"]), row["anchor_id"], "invalid full code")
            mappings.append(row)
        else:
            require(row["status"] == "PendingMapping" and not row["code"], row["anchor_id"], "pending mapping secretly enabled")
    # Every source row retains its original values and a specific disposition.
    inventory = {}
    for name, rows in t.items():
        inventory[name] = []
        for row in rows:
            reason = "EvidenceOnly: outside the bounded trial set"
            if name in ("forms", "identity_evidence"):
                key = row["handoff_form_key"]
                if key in by_form:
                    reason = "TrialAnchor: formal adoption pending"
                elif not forms[key]["m4_stable_lexeme_key"]:
                    reason = "PendingIdentity: no key allocated"
                if key == "RSH1:FORM:5246001A":
                    reason = "TrialOutputFormOnly: R004 belongs to machine source key; no separate key"
            elif name == "relations":
                a = by_form.get(row["source_form_key"])
                if a:
                    reason = "TrialRelationMetadata: runtime gates retained" if row["legacy_relation_id"] in a["approved_edges"] else "OutputNotReady: blocked selector retained on trial anchor"
            elif name == "output_candidates":
                if row["source_form_key"] in by_form:
                    reason = "TrialSourcePreserve" if row["candidate_role"] == "SOURCE_PRESERVE_ROUTE" else "EvidenceOnlyObservation: consult compiled relation/gates; observation alone never grants output"
            elif name == "same_text_identity_cases" and row["baseline_runtime_lexeme_ref"] in ("120664", "122250"):
                reason = "TrialDistinctAnchor: original default + explicit M6 output, separate learning"
            elif name == "pending_items":
                reason = "Pending: " + row["status"] + "; " + row["required_action"]
            inventory[name].append(dict(source_row_sha256=digest(canonical(row)), disposition=reason, source=row))
    p = snapshot["pending_identity"]
    require(p["status"] == "PendingIdentity" and p["text"] == "𨿍" and p["text"].encode().hex() == "f0a8bf8d" and
            not forms[p["source_record"]]["m4_stable_lexeme_key"] and p["proposed_xiaohe_code"] == "ee", "U+28FCD", "pending identity/text changed")
    return anchors, mappings, inventory


def token(text):
    return text.encode("utf-8").hex() or "-"


def convert(source, snapshot_path, manual_path, builder, destination, reverse=False):
    t, m, c = load_source(source)
    snapshot = json.loads(Path(snapshot_path).read_text(encoding="utf-8"))
    manual = json.loads(Path(manual_path).read_text(encoding="utf-8"))
    anchors, mappings, inventory = prepare(t, m, snapshot, manual)
    if reverse:
        anchors.reverse()
    out = Path(destination)
    out.mkdir(parents=True, exist_ok=True)
    lines = ["HANIME_STEP2F_TRIAL_V1 " + str(int(TRIAL_NS, 16))]
    for a in anchors:
        lines.append("L " + " ".join([a["source_key"], "1" if a["kind"] == "character" else "2", token(a["default_output"]),
                                       token(a["identity_boundary"]), token(snapshot["snapshot_id"] + ":" + a["id"])]))
        for o in a["outputs"]:
            lines.append("O " + " ".join([a["source_key"], token(o["form"]), token(o["text"]), token(o["condition"]),
                                           str(o["availability"]), str(int(o["blocked"])), token(o["basis"])]))
    for r in mappings:
        lines.append("C " + " ".join([r["source_key"], r["scheme_id"], r["code"]]))
    normalized = out / "build_input.txt"
    normalized.write_text("\n".join(lines) + "\n", encoding="utf-8")
    subprocess.run([str(Path(builder).resolve()), str(normalized), str(out)], check=True)
    report = dict(purpose=snapshot["purpose"], snapshot=snapshot,
                  snapshot_sha256=digest(Path(snapshot_path).read_bytes()), manual_full_pinyin=manual,
                  manual_sha256=digest(Path(manual_path).read_bytes()), source_zip_sha256=SOURCE_SHA,
                  source_manifest=m, condition_identity=c["identity"], build_order="reversed" if reverse else "snapshot",
                  builder_Approved_meaning="trial admission only, not production registry approval",
                  runtime_anchors=[dict(a, runtime_ref=i + 1) for i, a in enumerate(anchors)], mappings=mappings,
                  inventory=inventory, resource_builder=json.loads((out / "builder_manifest.json").read_text()),
                  output_files_sha256={p.name: digest(p.read_bytes()) for p in sorted(out.iterdir()) if p.suffix in (".hns", ".hnm")})
    (out / "manifest.json").write_text(json.dumps(report, ensure_ascii=False, sort_keys=True, indent=2) + "\n", encoding="utf-8")
    print(f"CONVERT PASS anchors={len(anchors)} mappings={len(mappings)} source_forms=42 relations=42 observations=128 order={report['build_order']}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", required=True)
    parser.add_argument("--snapshot", required=True)
    parser.add_argument("--full-pinyin", required=True)
    parser.add_argument("--builder", required=True)
    parser.add_argument("--out", required=True)
    parser.add_argument("--reverse", action="store_true")
    args = parser.parse_args()
    try:
        convert(args.source, args.snapshot, args.full_pinyin, args.builder, args.out, args.reverse)
    except (ValueError, KeyError, OSError, subprocess.CalledProcessError) as error:
        parser.exit(1, "REJECT " + str(error) + "\n")
