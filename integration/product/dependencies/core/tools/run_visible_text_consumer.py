#!/usr/bin/env python3
"""Build the same frozen Basic Host 0.6 consumer against an explicit core tree.

Use Python >= 3.11. No downloads, state database, device or parallel working
tree dependency. A reused build directory is explicit. Trace analysis is Host
diagnostic aggregation, never part of the bounded runtime.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import time
from collections import defaultdict


def sha(path):
    with path.open("rb") as source:
        return hashlib.file_digest(source, "sha256").hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True, help="Current package containing this consumer and frozen dependencies")
    parser.add_argument("--core", type=Path, help="Core to consume; defaults to root")
    parser.add_argument("--release", type=Path, help="Explicit trusted compile-time pin and transport directory")
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--logs", type=Path, required=True)
    parser.add_argument("--case", choices=("hotspots", "all", "reuse"), default="all")
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--profile", choices=("off", "output", "anchors", "balanced", "extra"), default="off")
    parser.add_argument("--relink-existing", action="store_true",
                        help="Diagnostic only: recompile this consumer, reuse the recorded libraries in this explicit build")
    args = parser.parse_args()
    root = args.root.resolve()
    core = args.core.resolve() if args.core else root
    release = args.release.resolve() if args.release else root / "resources/base_order_v1"
    release_manifest = json.loads((release / "release_manifest.json").read_text())
    build, logs = args.build.resolve(), args.logs.resolve()
    assert build != root and root not in build.parents, "consumer build must be outside core"
    assert build != core and core not in build.parents, "consumer build must be outside consumed core"
    logs.mkdir(parents=True, exist_ok=True)
    source = build / "external consumer source"
    shutil.copytree(root / "examples/visible_text_consumer", source, dirs_exist_ok=True)
    env = dict(os.environ)
    if args.sanitize:
        env.update(ASAN_OPTIONS="detect_leaks=0", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
    commands = []

    def run(label, arguments):
        arguments = list(map(str, arguments))
        start = time.monotonic()
        with (logs / (label + ".log")).open("w") as output:
            result = subprocess.run(arguments, stdout=output, stderr=subprocess.STDOUT, env=env)
        commands.append(dict(label=label, argv=arguments, exit=result.returncode,
                             host_wall_seconds=time.monotonic() - start))
        (logs / "COMMANDS.json").write_text(json.dumps(commands, indent=2) + "\n")
        assert result.returncode == 0, f"{label}: {logs / (label + '.log')}"

    cmake = shutil.which("cmake")
    assert cmake, "CMake must already be installed"
    run("COMPILER", ["c++", "--version"])
    run("GXX_IDENTITY", ["g++", "--version"])
    libraries = [build / "objects/libvisible_basic_host.a", build / "objects/core/libhanime_core.a"]
    if args.relink_existing:
        assert all(p.is_file() for p in libraries), "no previously compiled libraries"
        (logs / "REUSED_LIBRARIES.json").write_text(json.dumps(
            {str(p): sha(p) for p in libraries}, indent=2) + "\n")
        flags = ["-std=c++17", "-O3", "-Wall", "-Wextra", "-Wpedantic", "-Werror", "-fstack-usage"]
        if args.sanitize:
            flags += ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
        run("RELINK_CONSUMER", ["c++", *flags, "-I" + str(core / "include"),
            "-I" + str(root / "dependencies/basic_host_v0_6/include"),
            "-I" + str(release), "-MMD",
            "-MF", build / "objects/consumer_relink.d", source / "main.cpp",
            *libraries, "-o", build / "objects/visible_text_consumer"])
    else:
        run("CONFIGURE", [cmake, "-S", source, "-B", build / "objects", "-DCMAKE_BUILD_TYPE=Release",
            "-DHANIME_CORE_PATH=" + str(core),
            "-DBASIC_HOST_PATH=" + str(root / "dependencies/basic_host_v0_6"),
            "-DHANIME_RELEASE_PIN_DIR=" + str(release),
            "-DVISIBLE_SANITIZERS=" + ("ON" if args.sanitize else "OFF")])
        run("BUILD", [cmake, "--build", build / "objects", "--parallel", "4"])
    run("CONSUMER", [build / "objects/visible_text_consumer",
        root / "resources/production/order_a", release,
        logs / "READ_TRACE.jsonl", args.case] + ([args.profile] if args.profile != "off" else []))
    output = (logs / "CONSUMER.log").read_text()
    assert "PASS public Basic Host 0.6" in output
    parsed = defaultdict(list)
    for line in output.splitlines():
        if " {" in line:
            kind, payload = line.split(" ", 1)
            parsed[kind].append(json.loads(payload))
    for item in parsed["AUTH"]:
        # These successful scenarios never deactivate an owner within a phase.
        # A replaced owner begins at zero, as explicitly captured by the driver.
        item["data_evictions_derived"] = item["data_loads"] - (
            item["resident_data"] - item["resident_data_begin"])
        item["proof_evictions_derived"] = item["proof_loads"] - (
            item["resident_proof"] - item["resident_proof_begin"])
        assert item["data_evictions_derived"] >= 0 and item["proof_evictions_derived"] >= 0
    (logs / "METRICS.json").write_text(json.dumps(parsed, ensure_ascii=False, indent=2) + "\n")

    # Repeated raw loads are not automatically avoidable: verification, working
    # set and cache eviction determine whether reuse would remain valid.
    trace_summary = defaultdict(lambda: dict(reads=0, bytes=0, seeks=0,
                                             unique_blocks=set(), repeated_blocks=0))
    layouts = parsed["SHARED_LAYOUT"]
    assert layouts and all(layout == layouts[0] for layout in layouts)
    layout = layouts[0]
    classifications = defaultdict(lambda: defaultdict(int))
    names = ("lexeme_table", "relation_table", "selector_table",
             "string_descriptor_table", "key_order_auxiliary")
    regions = [("global_metadata", 0, layout["header_end"]),
               ("text_payload", layout["payload_offset"],
                layout["payload_offset"] + layout["payload_bytes"])]
    for name, section in zip(names, layout["sections"]):
        regions.append((name, section["offset"],
                        section["offset"] + section["pages"] * layout["page_bytes"]))
    with (logs / "READ_TRACE.jsonl").open() as stream:
        for line in stream:
            event = json.loads(line)
            key = (event["phase"], event["role"])
            item = trace_summary[key]
            item["reads"] += 1
            item["bytes"] += event["bytes"]
            item["seeks"] += event["seek"]
            scheme = "full_pinyin" if "/full_pinyin/" in event["phase"] else "xiaohe"
            name = "shared" if event["role"].startswith("shared") else scheme + (
                "_base" if event["role"].startswith("order") else "")
            spec = next(r for r in release_manifest["files"] if r["logical_name"] == name)
            packed_file = spec.get("verification_format", 1) == 2
            stride = spec["packet_bytes"] if packed_file else 1024
            block = event["offset"] // stride
            item["repeated_blocks"] += int(block in item["unique_blocks"])
            item["unique_blocks"].add(block)
            spans = []
            if packed_file:
                assert event["role"].endswith("proof"), "packed consumer accessed redundant original data"
                position, remaining = event["offset"], event["bytes"]
                while remaining:
                    number, within = divmod(position, stride)
                    amount = min(remaining, stride - within)
                    data_bytes = max(0, min(within + amount, 512) - within)
                    if event["role"] == "shared_proof":
                        classifications[event["phase"]]["binary_proof_bytes"] += amount - data_bytes
                        if data_bytes:
                            logical_start = number * 512 + within
                            valid = min(data_bytes, max(0, spec["file_bytes"] - logical_start))
                            classifications[event["phase"]]["last_block_padding"] += data_bytes - valid
                            spans.append((logical_start, logical_start + valid))
                    position += amount
                    remaining -= amount
            elif event["role"] == "shared_data":
                spans.append((event["offset"], event["offset"] + event["bytes"]))
            for start, end in spans:
                credited = 0
                for category, lo, hi in regions:
                    overlap = max(0, min(end, hi) - max(start, lo))
                    classifications[event["phase"]][category] += overlap
                    credited += overlap
                assert credited == end-start, "shared raw bytes not assigned to authenticated layout"
    summary = []
    for (phase, role), item in trace_summary.items():
        item["unique_blocks"] = len(item["unique_blocks"])
        summary.append(dict(phase=phase, role=role, **item))
    (logs / "TRACE_SUMMARY.json").write_text(json.dumps(summary, indent=2) + "\n")
    (logs / "SHARED_PHYSICAL_BYTE_CLASSES.json").write_text(json.dumps(dict(
        scope="Host analysis using public layout; format 2 splits data/proof inside each packet; table regions include physical page headers and padding",
        verification_format=release_manifest["verification_index_format"],
        layout=layout, phases=classifications), indent=2) + "\n")
    for cost in parsed["COST"]:
        rows = [r for r in summary if r["phase"] == cost["phase"]]
        assert sum(r["bytes"] for r in rows) == cost["bytes"]
        assert sum(r["reads"] for r in rows) == cost["reads"]
        assert sum(r["seeks"] for r in rows) == cost["seeks"]
    stack = logs / "stack"
    stack.mkdir(exist_ok=True)
    for path in (build / "objects").rglob("*.su"):
        if path.name in {"main.cpp.su", "paged_shared_resource.cpp.su", "verified_resource.cpp.su",
                         "bounded_output.cpp.su", "composition.cpp.su", "submission.cpp.su",
                         "exact_ranker.cpp.su", "composition_commit.cpp.su",
                         "member_eligibility.cpp.su", "base_order.cpp.su"}:
            shutil.copy2(path, stack / path.name)
    dependency_path = (build / "objects/consumer_relink.d") if args.relink_existing else next(
        (build / "objects/CMakeFiles/visible_text_consumer.dir").rglob("main.cpp.o.d"))
    dependencies = dependency_path.read_text()
    normalized_dependencies = dependencies.replace("\\\n", "").replace("\\ ", " ")
    for private in (core / "src", core / "tests", root / "dependencies/basic_host_v0_6/src"):
        assert str(private) + "/" not in normalized_dependencies, "consumer included private implementation"
    (logs / "PUBLIC_DEPENDENCIES.txt").write_text(dependencies)
    frozen = root / "dependencies/basic_host_v0_6"
    (logs / "RESULT.json").write_text(json.dumps(dict(
        status="PASS", case=args.case, profile=args.profile, core=str(core), source=str(source),
        release=str(release), verification_format=release_manifest["verification_index_format"],
        trusted_pin_sha256=sha(release / "release_pin.hpp"),
        source_sha256={p.name: sha(p) for p in source.iterdir() if p.is_file()},
        basic_host_sha256={str(p.relative_to(frozen)): sha(p) for p in sorted(frozen.rglob("*")) if p.is_file()},
        binary_sha256=sha(build / "objects/visible_text_consumer"),
        sanitizer=args.sanitize, leak_detection=False, device=False,
        production_libraries_reused=args.relink_existing,
        production_libraries_sha256={str(p): sha(p) for p in libraries},
        physical_port="real POSIX read plus conditional lseek",
        trace_file_sha256=sha(logs / "READ_TRACE.jsonl")), indent=2) + "\n")
    print("PASS visible-text public consumer:", logs)


if __name__ == "__main__":
    main()
