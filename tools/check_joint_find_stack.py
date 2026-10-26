#!/usr/bin/env python3
"""Find B1's existing stack limits, with joint paths and both Alpha2 context frames.

Adapted from Find B1 tools/check_find_stack.py. No numeric gate is relaxed;
the inherited product gate is consumed, never rewritten by this tool.
"""
from pathlib import Path
import argparse
import json
import re

ROOT = Path(__file__).resolve().parents[1]
RULES = {
 'paint': (r'::find_ui::paint\(', 512), 'publish': (r'::find_ui::publish\(', 192),
 'raw': (r'find_display.cpp:\d+:\d+:void .*::raw\(', 256),
 'selected': (r'find_display.cpp:\d+:\d+:void .*::selected\(', 192),
 'whole': (r'find_display.cpp:\d+:\d+:bool .*::whole\(', 128),
 'viewport': (r'find_display.cpp:\d+:\d+:.*::Window .*::viewport\(', 160),
 'text': (r'find_display.cpp:\d+:\d+:int .*::Paint::text\(', 96),
 'glyph': (r'find_display.cpp:\d+:\d+:int .*::Paint::glyph\(', 96),
 'rect': (r'find_display.cpp:\d+:\d+:void .*::Paint::rect\(', 64),
 'pixel': (r'find_display.cpp:\d+:\d+:void .*::Paint::pixel\(', 48),
 'font_glyph': (r'ui16_font.cpp:\d+:\d+:.*::Font::glyph\(', 80),
 'coverage': (r'::ui16::canRender\(', 96),
 'count': (r'::FindSession::countStep\(', 96),
 'search': (r'::FindSession::step\(', 160),
 'service': (r'::App::serviceReaders\(', 256),
}


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--root', type=Path, default=ROOT)
    p.add_argument('--output', type=Path)
    args = p.parse_args()
    root = args.root.resolve()
    build = root / 'TestBuilds/s3/DISPLAY_TX_PROTO/build/sketch'
    original = root / 'evidence/s3/STACK_GATE.json'
    old = json.loads(original.read_text())
    failures = [] if old['passed'] else ['Inherited product stack gate failed; no override']
    rows = []
    for path in sorted(build.glob('*.su')):
        for line in path.read_text().splitlines():
            parts = line.split('\t')
            if len(parts) == 3:
                rows.append(dict(symbol=parts[0], bytes=int(parts[1]), kind=parts[2], file=str(path)))
    frames = {}
    for name, (pattern, limit) in RULES.items():
        matches = [row for row in rows if re.search(pattern, row['symbol'])]
        if not matches:
            failures.append(name + ': missing emitted function')
            continue
        frames[name] = dict(max(matches, key=lambda x: x['bytes']), limit=limit)
        if any(row['kind'] != 'static' or row['bytes'] > limit for row in matches):
            failures.append(name + ': frame budget exceeded')
    contexts = {}
    for name, pattern in {
        'presentContextWrapper': r':void \{anonymous\}::presentContext\(\)$',
        'presentContextPart': r':void \{anonymous\}::_ZN12_GLOBAL__N_114presentContextEv\$part\$0\(\)$',
    }.items():
        matches = [row for row in rows if re.search(pattern, row['symbol'])]
        if len(matches) != 1:
            failures.append(name + ': expected one emitted Alpha2 context frame')
        else:
            contexts[name] = matches[0]
            if matches[0]['kind'] != 'static':
                failures.append(name + ': non-static stack')
    oldframes = old['frames']
    for name in ('loop', 'deviceLoop', 'draw'):
        if name not in oldframes:
            failures.append(name + ': absent from original stack gate')
    estimates = {}
    if not failures:
        # The old Alpha2 presentContext gate already accounts for the split
        # part, but not its live wrapper. Include both once in the Find prefix.
        assert oldframes['presentContext']['symbol'] == contexts['presentContextPart']['symbol']
        size = lambda *names: sum(frames[n]['bytes'] for n in names)
        prefix = 64 + sum(oldframes[n]['bytes'] for n in ('loop', 'deviceLoop', 'draw'))
        prefix += sum(row['bytes'] for row in contexts.values())
        estimates['find_paint_with_context_conservative'] = prefix + size(
            'paint', 'raw', 'viewport', 'text', 'glyph', 'rect', 'pixel', 'font_glyph', 'coverage') + 512
        if not old.get('nested'):
            failures.append('existing nested report absent')
        else:
            # Retain original B1 conservative maximum and 128 B allowance;
            # add Alpha2's wrapper, since old nested paths include only its part.
            estimates['find_materialization_increment_over_existing_paths'] = (
                max(old['nested'].values()) + size('coverage', 'font_glyph') + 128
                + contexts['presentContextWrapper']['bytes'])
        if any(value > 5632 for value in estimates.values()):
            failures.append('reviewed branch allowance exceeds existing 5632 B budget')
    result = dict(passed=not failures, failures=failures, frames=frames,
                  alpha2_context_frames=contexts, inherited_product_gate=str(original),
                  conservative_branch_bytes=estimates, branch_budget_bytes=5632, task_stack_bytes=8192,
                  scope='Actual Xtensa emitted frames with unchanged Find B1 limits and reserves; both Alpha2 presentContext wrapper and split part included. Conservative reviewed branches, not whole-program maximum or device low-water.')
    output = args.output or root / 'evidence/joint/FIND_STACK_GATE.json'
    output.parent.mkdir(parents=True, exist_ok=True)
    output.write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps(dict(passed=result['passed'], failures=failures, branches=estimates), indent=2))
    return bool(failures)


if __name__ == '__main__':
    raise SystemExit(main())
