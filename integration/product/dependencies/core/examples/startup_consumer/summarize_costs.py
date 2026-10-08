#!/usr/bin/env python3
"""Summarize actual startup_consumer output; never upgrade a failed gate."""
import argparse
import hashlib
import json
from pathlib import Path


def fields(line):
    result = {}
    for word in line.split()[1:]:
        if "=" not in word:
            continue
        key, value = word.split("=", 1)
        try:
            result[key] = int(value)
        except ValueError:
            try:
                result[key] = float(value)
            except ValueError:
                result[key] = value
    return result


def summarize(path):
    data = path.read_bytes()
    lines = data.decode("utf-8").splitlines()
    costs = [fields(line) for line in lines if line.startswith("COST ")]
    for cost in costs:
        # Sensitivity ONLY. Neither range endpoint was measured on the S3.
        # The central printed gate model remains unchanged in its own fields.
        work = cost["work"] + cost["local"]
        cost["uncalibrated_sensitivity_lower_s"] = (
            cost["pread_bytes"] / 110000
            + cost["offset_discontinuities"] * 0.0002
            + cost["opens"] * 0.01
            + cost["steps"] * 0.00002
            + work / 10000000
        )
        cost["uncalibrated_sensitivity_upper_s"] = (
            cost["pread_bytes"] / 64000
            + cost["offset_discontinuities"] * 0.014
            + cost["opens"] * 0.04
            + cost["steps"] * 0.0001
            + work / 1000000
        )
    return {
        "log": str(path),
        "sha256": hashlib.sha256(data).hexdigest(),
        "model": next((fields(line) for line in lines if line.startswith("MODEL ")), None),
        "costs": costs,
        "memory": [fields(line) for line in lines if line.startswith("MEMORY ")],
        "relative_gates": [fields(line) for line in lines if line.startswith("RELATIVE_COLLISION_GATE ")],
        "pass_markers": [line for line in lines if line.startswith("PASS ")],
        "failure_markers": [line for line in lines if line.startswith("FAIL ")],
        "device_verified": False,
        "sensitivity_is_measurement": False,
        "notes": "POSIX bytes are not SPI bytes. Runtime model is not measured S3 time; leak detection is off in the sanitizer run.",
    }


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("logs", nargs="+", type=Path)
    args = parser.parse_args()
    print(json.dumps({"runs": [summarize(path) for path in args.logs]}, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
