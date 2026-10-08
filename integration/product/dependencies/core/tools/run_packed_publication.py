#!/usr/bin/env python3
"""Publish final-byte-validated authentication transport and independently check it.

The output must be a fresh directory. Uses an already-built Host publisher;
never rebuilds identities, edits original resources, or obtains trust from JSON.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import subprocess
import sys
import time


def sha(path):
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path(__file__).resolve().parents[1])
    parser.add_argument("--build", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True)
    parser.add_argument("--logs", type=Path, required=True)
    parser.add_argument("--sanitize", action="store_true")
    parser.add_argument("--all-resources", action="store_true", help="Unselected all-format-2 comparison")
    args = parser.parse_args()
    root, build, output, logs = [p.resolve() for p in (args.root, args.build, args.out, args.logs)]
    assert not output.exists(), "fresh-only publication; never replace an accepted release"
    assert output.parent.is_dir()
    logs.mkdir(parents=True, exist_ok=True)
    data = root / "resources/production/order_a"
    env = dict(os.environ)
    if args.sanitize:
        env.update(ASAN_OPTIONS="detect_leaks=0", UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
    commands = []

    def run(label, argv):
        argv = list(map(str, argv))
        start = time.monotonic()
        with (logs / (label + ".log")).open("w") as stream:
            result = subprocess.run(argv, stdout=stream, stderr=subprocess.STDOUT, env=env)
        commands.append(dict(label=label, argv=argv, exit=result.returncode,
                             host_wall_seconds=time.monotonic() - start))
        (logs / "COMMANDS.json").write_text(json.dumps(commands, indent=2) + "\n")
        assert result.returncode == 0, label + " failed; see " + str(logs)

    run("PUBLISH", [build / "hanime_publish_verified", "--shared", data / "shared.hns",
        "--namespace", "0x48414e494d450001", "--scheme", "xiaohe", data / "xiaohe.hnm",
        "--scheme", "full_pinyin", data / "full_pinyin.hnm", "--out", output,
        "--base-order", "--packed-auth-all" if args.all_resources else "--packed-auth"])
    run("ORACLE", [sys.executable, root / "tools/verify_release_index.py", data, output])
    (logs / "RESULT.json").write_text(json.dumps(dict(
        status="PASS", sanitizer=args.sanitize, leak_detection=False,
        publisher_sha256=sha(build / "hanime_publish_verified"),
        profile="all resources" if args.all_resources else "shared format 2, scheme/order format 1",
        files={p.name: dict(bytes=p.stat().st_size, sha256=sha(p)) for p in sorted(output.iterdir())},
        device=False), indent=2) + "\n")
    print("PASS final-byte publication and independent packet oracle:", output)


if __name__ == "__main__":
    main()
