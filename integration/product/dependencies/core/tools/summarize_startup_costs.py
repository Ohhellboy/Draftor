#!/usr/bin/env python3
"""Parse public-consumer cost evidence. A budget failure stays a failure."""
import argparse
import json
from pathlib import Path


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('log', type=Path)
    args = p.parse_args()
    lines = args.log.read_text().splitlines()
    costs, memory, gates = [], [], []
    for line in lines:
        if not line.startswith(('COST ', 'MEMORY ', 'RELATIVE_COLLISION_GATE ')):
            continue
        values = {}
        for word in line.split()[1:]:
            if '=' not in word:
                continue
            key, val = word.split('=', 1)
            try:
                val = float(val) if '.' in val else int(val)
            except ValueError:
                pass
            values[key] = val
        if line.startswith('COST '):
            costs.append(values)
        elif line.startswith('MEMORY '):
            memory.append(values)
        else:
            gates.append(values)
    failed = any(c.get('gate') == 'FAIL' for c in costs if c.get('mode') == 'fast')
    failed |= any(c.get('status') != 'PASS' for c in gates)
    failed |= any(line.startswith('FAIL ') for line in lines)
    complete = any(line.startswith('PASS named_sample_virtual_cost_gates') for line in lines)
    print(json.dumps({'source_log': str(args.log), 'complete': complete, 'budget_failed': failed,
                      'device_30s': 'OPEN; model is not S3 evidence',
                      'model': {'sequential_wall_envelope_bytes_per_s': 85672,
                                'extra_offset_discontinuity_s_assumed': 0.007,
                                'open_s_envelope': 0.028,
                                'extra_schedule_s_per_step_assumed': 0.00005,
                                'abstract_work_units_per_s_assumed': 3000000,
                                'note': 'Sequential wall envelope already includes scheduling; extra terms are conservative stress assumptions, not independent device calibration.'},
                      'costs': costs, 'memory': memory, 'relative_gates': gates}, indent=2))
    if failed or not complete:
        raise SystemExit(1)


if __name__ == '__main__':
    main()
