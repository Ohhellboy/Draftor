#!/usr/bin/env python3
"""Summarize measured public phases; keep conditional device model separate."""
import argparse
import json
from pathlib import Path


def read(path):
    return json.loads(path.read_text())


def non_time(rows):
    return [{k: v for k, v in row.items() if not k.endswith("_ns")} for row in rows]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--evidence", type=Path, required=True)
    args = parser.parse_args()
    root = args.evidence.resolve()
    baseline = read(root / "baseline_0101/METRICS.json")
    current = read(root / "shared_normal_final/METRICS.json")
    sanitized = read(root / "shared_sanitize_final/METRICS.json")
    legacy = read(root / "legacy_final/METRICS.json")
    comparison = read(root / "selected_comparison/COMPARISON.json")
    original = read(root / "original_comparison/COMPARISON.json")
    assert non_time(current["COST"]) == non_time(sanitized["COST"]), "Sanitizer changed non-time cost"
    assert non_time(baseline["COST"]) == non_time(legacy["COST"]), "legacy cost changed"
    for kind in ("AUTH", "ITEM", "MEMBER", "DOCUMENT", "PAGING", "STATE", "MEMORY", "CACHE_MEMORY"):
        assert current[kind] == sanitized[kind], kind + " differs with instrumentation"

    phases = {row["phase"]: row for row in comparison["phases"]}
    selected_names = [name for name in phases if name.startswith("cold_ui/") or
                      name.startswith("segments_xh/") or
                      name.endswith("forward_1_text") or name.endswith("forward_33_text")]
    selected = [phases[name] for name in selected_names]
    memory_fields = ("fixed", "data_payload", "proof_payload", "data_metadata", "proof_metadata")
    memory = {}
    for name, values in (("baseline_0101", baseline), ("selected_0110", current), ("legacy_0110", legacy)):
        rows = values["CACHE_MEMORY"][:3]
        memory[name] = dict(owners=rows,
            measured_component_sum=sum(row[k] for row in rows for k in memory_fields),
            payload_sum=sum(row[k] for row in rows for k in ("data_payload", "proof_payload")),
            scope="3 active authentication owners only; no allocator/control blocks, page cache, output, caller or stack")
    maxima = {}
    for name, data in (("baseline_0101", baseline), ("selected_0110", current)):
        maxima[name] = {key: max(data["COST"], key=lambda row: row[key])
                        for key in ("max_tick_ns", "max_read_ns", "max_seek_ns")}

    # This is only a conditional sensitivity model of a historical cumulative
    # phase, not a calibrated random/sequential TF model or a per-key forecast.
    read_us = 15077854 / 1651
    seek_us = 13068412 / 1556
    non_io_step_us = (29377939 - 15077854 - 13068412) / 2582
    models = []
    for row in selected:
        estimates = {}
        for side in ("before", "after"):
            cost = row[side]
            io = read_us * cost["reads"] + seek_us * cost["seeks"]
            estimates[side] = dict(conditional_io_ms=io / 1000,
                conditional_total_ms=(io + non_io_step_us * cost["steps"]) / 1000,
                sensitivity_low_ms=(0.5 * io + 0.5 * non_io_step_us * cost["steps"]) / 1000,
                sensitivity_high_ms=(1.5 * io + 4 * non_io_step_us * cost["steps"]) / 1000)
        models.append(dict(phase=row["phase"], **estimates))
    stack = {}
    for label in ("baseline_0101", "shared_normal_final"):
        entries = []
        for file in (root / label / "stack").glob("*.su"):
            for line in file.read_text().splitlines():
                parts = line.rsplit("\t", 2)
                if len(parts) == 3 and parts[1].isdigit():
                    entries.append(dict(file=file.name, function=parts[0], bytes=int(parts[1]), kind=parts[2]))
        stack[label] = sorted(entries, key=lambda value: value["bytes"], reverse=True)[:12]
    result = dict(status="PASS", ordinary_sanitizer_non_time_equal=True,
        legacy_format1_per_phase_unchanged=True, phase_count=len(phases),
        accepted_baseline_total=comparison["total"], original_0100_total=original["total"],
        scenarios=comparison["scenarios"], selected_phases=selected,
        unfavorable_count=len(comparison["unfavorable_non_time"]),
        unfavorable=comparison["unfavorable_non_time"], authentication_memory=memory,
        host_time_maxima=maxima, stack_largest_single_functions=stack,
        model=dict(source="received/evidence/historical_device/HANIME_S3_LAST_SPACE_LATENCY_V1_0_20260912_174619.log:474",
            read_us_per_call=read_us, seek_us_per_call=seek_us, residual_us_per_step=non_io_step_us,
            assumptions="reuse historical 1MHz aggregate means; no independent random/sequential, SHA or scheduler calibration; multiplier range is sensitivity, not confidence interval",
            excludes="LCD/font/BLE/event age/other host scheduling; new firmware not run",
            stages=models), device_performance="OPEN; no claim of daily acceptance")
    (root / "PERFORMANCE_SUMMARY.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
    print("PASS summary", len(phases), "phases; memory sums", {k:v["measured_component_sum"] for k,v in memory.items()})


if __name__ == "__main__":
    main()
