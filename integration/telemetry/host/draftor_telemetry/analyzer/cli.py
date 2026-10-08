"""Analyze an existing reconstruction summary.json without opening devices/network."""
import argparse
import json
from pathlib import Path
import tempfile
from . import Options, iter_analysis
from .summary import Summary
from .adapter import validate_summary


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--input', required=True, type=Path, help='Existing SessionReconstructor summary.json')
    parser.add_argument('--output', required=True, type=Path, help='New or empty output directory')
    parser.add_argument('--post-commit-us', type=int, default=2_000_000)
    parser.add_argument('--reroute-us', type=int, default=2_000_000)
    parser.add_argument('--max-input-bytes', type=int, default=128*1024*1024)
    parser.add_argument('--max-session-events', type=int, default=200_000)
    args = parser.parse_args(argv)
    try:
        options = Options(args.post_commit_us, args.reroute_us, args.max_session_events)
        if args.max_input_bytes <= 0:
            raise ValueError('--max-input-bytes must be positive')
        if args.input.stat().st_size > args.max_input_bytes:
            raise ValueError('Input exceeds --max-input-bytes; split at session boundaries')
        if args.output.exists() and (not args.output.is_dir() or any(args.output.iterdir())):
            raise ValueError('Output directory must be new or empty')
        with args.input.open('rb') as stream:
            data = stream.read(args.max_input_bytes+1)
        if len(data) > args.max_input_bytes:
            raise ValueError('Input exceeds --max-input-bytes')
        reconstruction = json.loads(data)
        del data
        validate_summary(reconstruction)
        summary = Summary(reconstruction, options)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        # Finish validation and all output before publishing; a bad input leaves no partial analysis.
        with tempfile.TemporaryDirectory(prefix='.analyzer-a1-', dir=args.output.parent) as staging:
            staging = Path(staging)
            with (staging/'analysis.jsonl').open('w', encoding='utf-8') as stream:
                for row in iter_analysis(reconstruction, options):
                    stream.write(json.dumps(row, ensure_ascii=False, sort_keys=True)+'\n')
                    summary.accept(row)
            (staging/'summary.json').write_text(json.dumps(summary.data, indent=2, sort_keys=True)+'\n', encoding='utf-8')
            (staging/'summary.md').write_text(summary.markdown(), encoding='utf-8')
            args.output.mkdir(exist_ok=True)
            for path in sorted(staging.iterdir()):
                path.rename(args.output/path.name)
        print(str(args.output/'summary.md'))
        return 0
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.exit(2, 'analyzer: '+str(error)+'\n')
