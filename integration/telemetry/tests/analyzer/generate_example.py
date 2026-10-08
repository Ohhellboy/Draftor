#!/usr/bin/env python3
"""Generate a small synthetic reconstruction and run the production A1 CLI."""
import argparse
import json
from pathlib import Path
import sys

ROOT = Path(__file__).resolve().parents[2]
sys.path[:0] = [str(ROOT/'host'), str(Path(__file__).resolve().parent)]
from fixtures import cases, reconstruct
from draftor_telemetry.analyzer.cli import main


def generate(output):
    if output.exists() and any(output.iterdir()):
        raise ValueError('Example destination must be empty')
    output.mkdir(parents=True, exist_ok=True)
    selected = ('third_candidate_browsed', 'post_commit_rewrite', 'basic_privacy_insufficient', 'candidate_result_change')
    combined = dict(schema_version=1, schema_revision=2, sessions={}, unassigned_errors=[])
    labels = {}
    for number, name in enumerate(selected, 101):
        trace = cases()[name]
        for event in trace.events:
            event['boot_id'] = number
        combined['sessions'].update(reconstruct(trace.events)['sessions'])
        labels[str(number)] = name
    source = output/'input_reconstruction.json'
    source.write_text(json.dumps(combined, ensure_ascii=False, indent=2)+'\n', encoding='utf-8')
    (output/'fixture_manifest.json').write_text(json.dumps(dict(evidence='HOST / SYNTHETIC; no device input',
                                                              sessions=labels), indent=2)+'\n', encoding='utf-8')
    return main(['--input', str(source), '--output', str(output/'analysis')])


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--output', required=True, type=Path)
    raise SystemExit(generate(parser.parse_args().output))
