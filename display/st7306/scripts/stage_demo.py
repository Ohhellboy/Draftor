#!/usr/bin/env python3
"""Prepare a fresh standalone Arduino sketch; never compile or flash automatically."""
import argparse
from pathlib import Path
import shutil

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("platform", choices=("esp32s3", "pico2w"))
parser.add_argument("output", type=Path, help="New directory outside this source tree")
args = parser.parse_args()
root = Path(__file__).resolve().parents[1]
out = args.output.resolve()
if out == root or root in out.parents:
    parser.error("Choose a new directory outside the source tree")
if out.exists():
    parser.error("Output already exists; choose a new directory")
files = ("ST7306_OsptekLpm8.h", "ST7306_OsptekLpm8.cpp",
         "st7306_osptek_lpm8_adapter.hpp", "st7306_osptek_lpm8_protocol.hpp")
name = "minimal_" + args.platform
sketch = out / name
sketch.mkdir(parents=True)
for filename in files:
    shutil.copyfile(root / "platform" / args.platform / filename, sketch / filename)
shutil.copyfile(root / "examples" / name / (name + ".ino"), sketch / (name + ".ino"))
print(sketch)
