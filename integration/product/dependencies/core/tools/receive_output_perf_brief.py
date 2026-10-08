#!/usr/bin/env python3
"""Verify the supplied brief and the frozen core baseline; preserve small inputs."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import zipfile

p = argparse.ArgumentParser()
p.add_argument("--brief", type=Path, required=True)
p.add_argument("--root", type=Path, required=True)
a = p.parse_args()
root = a.root.resolve()
ev = root / "evidence/output_perf_v1"
ev.mkdir(parents=True, exist_ok=True)

def sha(b):
    return hashlib.sha256(b).hexdigest()

with zipfile.ZipFile(a.brief) as z:
    assert z.testzip() is None
    prefix = "HanIME_Performance_and_Segmentation_Briefs_v1_0/"
    manifest = json.loads(z.read(prefix + "MANIFEST_SHA256.json"))
    assert set(z.namelist()) == {prefix + r["path"] for r in manifest["files"]} | {prefix + "MANIFEST_SHA256.json"}
    for r in manifest["files"]:
        b = z.read(prefix + r["path"])
        assert len(b) == r["bytes"] and sha(b) == r["sha256"]
    for name in z.namelist():
        rel = Path(name.removeprefix(prefix))
        assert not rel.is_absolute() and ".." not in rel.parts
        dest = ev / "received" / rel
        dest.parent.mkdir(parents=True, exist_ok=True)
        data = z.read(name)
        if dest.exists():
            assert dest.read_bytes() == data
        else:
            dest.write_bytes(data)

baseline = root.parent / "HANIME_Independent_Core_VisibleTextRepair_v2_0.zip"
assert hashlib.file_digest(baseline.open("rb"), "sha256").hexdigest() == "7075dd47b44685f641a04aa3765801f609a3694c3445009433ae62e6f40d8968"
checked = []
with zipfile.ZipFile(baseline) as z:
    assert z.testzip() is None
    for name in z.namelist():
        rel = Path(name.removeprefix("HANIME_Independent_Core/"))
        if rel.parts[0] not in ("src", "include", "tests", "tools", "examples", "dependencies", "registry", "resources") and str(rel) not in ("CMakeLists.txt", "README.md", "PUBLIC_API.md", "IME_ARCHITECTURE.md", "REPORT.md", "VISIBLE_TEXT_HANDOFF.md"):
            continue
        data = z.read(name)
        assert (root / rel).read_bytes() == data, "baseline drift: " + str(rel)
        checked.append({"path": str(rel), "bytes": len(data), "sha256": sha(data)})
        if rel.parts[0] not in ("resources", "registry"):
            dest = ev / "before" / rel
            dest.parent.mkdir(parents=True, exist_ok=True)
            if not dest.exists():
                dest.write_bytes(data)
    for name in ("00-当前状态.md", "01-工程日志.md", "02-设计决策.md", "03-待验证事项.md"):
        dest = ev / "record_before" / name
        dest.parent.mkdir(exist_ok=True)
        if not dest.exists():
            shutil.copy2(root.parent / name, dest)
result = dict(status="PASS", brief_bytes=a.brief.stat().st_size,
              brief_sha256=hashlib.file_digest(a.brief.open("rb"), "sha256").hexdigest(),
              brief_crc_files=16, brief_content_files=15,
              baseline_sha256="7075dd47b44685f641a04aa3765801f609a3694c3445009433ae62e6f40d8968",
              baseline_checked_files=checked,
              scope="Only 01 authorized; 04/05 evidence. User brief accepts API0.11.0; no new GCC/S3 execution claimed.")
(ev / "RECEIPT.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
print("PASS brief 16 CRC / 15 SHA; baseline inputs:", len(checked))
