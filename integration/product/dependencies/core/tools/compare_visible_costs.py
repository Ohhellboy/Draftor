#!/usr/bin/env python3
"""Compare two actual executions of one public Basic Host 0.6 consumer."""
import argparse
import json
from pathlib import Path


def load(path, name):
    return json.loads((path / name).read_text())


def totals(rows):
    keys = ("bytes", "reads", "seeks", "steps", "work", "local_work",
            "shared_data_bytes", "shared_proof_bytes", "scheme_data_bytes",
            "scheme_proof_bytes", "order_data_bytes", "order_proof_bytes")
    return {key: sum(row[key] for row in rows) for key in keys}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--baseline", type=Path, required=True)
    parser.add_argument("--candidate", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    before = load(args.baseline, "METRICS.json")
    after = load(args.candidate, "METRICS.json")
    before_result = load(args.baseline, "RESULT.json")
    after_result = load(args.candidate, "RESULT.json")
    assert before_result["source_sha256"] == after_result["source_sha256"], "different consumer bytes"
    assert before_result["basic_host_sha256"] == after_result["basic_host_sha256"], "different frozen Basic Host"
    assert before["CONFIG"] == after["CONFIG"], "different capacities or modes"
    result_kinds = ("ITEM", "MEMBER", "DOCUMENT", "PAGING", "STATE", "SHARED_LAYOUT")
    for kind in result_kinds:
        assert before[kind] == after[kind], "functional result differs: " + kind
    assert len(before["COST"]) == len(after["COST"])
    before_auth = {(item["phase"], item["role"]): item for item in before["AUTH"]}
    after_auth = {(item["phase"], item["role"]): item for item in after["AUTH"]}
    rows = []
    unfavorable = []
    for old, new in zip(before["COST"], after["COST"]):
        assert old["phase"] == new["phase"]
        phase = old["phase"]
        roles = {}
        for role in ("shared_data", "scheme_data", "order_data"):
            key = (phase, role)
            if key in before_auth:
                assert key in after_auth
                roles[role] = dict(before=before_auth[key], after=after_auth[key])
        row = dict(phase=phase, before=old, after=new, authentication=roles)
        rows.append(row)
        increases = {key: new[key] - old[key] for key in ("bytes", "reads", "seeks", "steps", "work")
                     if new[key] > old[key]}
        if increases:
            unfavorable.append(dict(phase=phase, increases=increases))
    stages = {}
    for row in rows:
        scenario = row["phase"].split("/")[0]
        stages.setdefault(scenario, []).append(row)
    scenario_totals = {key: dict(before=totals([r["before"] for r in values]),
                                 after=totals([r["after"] for r in values]))
                       for key, values in stages.items()}
    result = dict(
        status="PASS", claim="functional equality and actual Host access comparison, not S3 performance acceptance",
        baseline=str(args.baseline.resolve()), candidate=str(args.candidate.resolve()),
        source_sha256=before_result["source_sha256"], config=before["CONFIG"],
        functional_equality={kind: len(before[kind]) for kind in result_kinds},
        total=dict(before=totals(before["COST"]), after=totals(after["COST"])),
        scenarios=scenario_totals, unfavorable_non_time=unfavorable, phases=rows,
        baseline_shared_classes=load(args.baseline, "SHARED_PHYSICAL_BYTE_CLASSES.json"),
        candidate_shared_classes=load(args.candidate, "SHARED_PHYSICAL_BYTE_CLASSES.json"))
    args.output.mkdir(parents=True, exist_ok=True)
    (args.output / "COMPARISON.json").write_text(json.dumps(result, ensure_ascii=False, indent=2) + "\n")
    lines = ["# 固定 Basic Host 0.6 新旧访问成本", "",
             "同一消费者与固定容量；真实 POSIX read/lseek。不是 S3 秒数，未将并行适配优化算入。",
             "全局布局来自公开已认证信息；表区域包含物理页头与填充。所有阶段和身份正文断言见 COMPARISON.json。",
             "AUTH 淘汰由成功载入减驻留变化推算，trace 重复块另列。", "",
             "## 完整剧本（不同场景，不是一次按键）", "",
             "| 场景 | bytes 旧→新 | read 旧→新 | seek 旧→新 | advance 旧→新 |",
             "|---|---:|---:|---:|---:|"]
    for name, value in [("全部", result["total"]), *scenario_totals.items()]:
        old, new = value["before"], value["after"]
        cells = [f'{old[key]:,} → {new[key]:,}' for key in ("bytes", "reads", "seeks", "steps")]
        lines.append("| " + name + " | " + " | ".join(cells) + " |")
    lines += ["", "## 各阶段", "",
              "每项保留完整顺序。shared_proof 是同名物理端口：格式1只含证明，格式2含数据＋证明整包，不能把后者全称为证明流量。数据／证明拆分见 SHARED_PHYSICAL_BYTE_CLASSES.json。总 bytes 包含三个角色的全部读取。",
              "最长 tick/read/seek 的 Host 纳秒及所有缓存命中/淘汰见 JSON，不能作为设备尾时延。", "",
              "| 阶段 | bytes 旧→新 | read 旧→新 | seek 旧→新 | shared proof bytes 旧→新 | steps 旧→新 |",
              "|---|---:|---:|---:|---:|---:|"]
    for row in rows:
        old, new = row["before"], row["after"]
        cells = [f'{old[key]:,} → {new[key]:,}'
                 for key in ("bytes", "reads", "seeks", "shared_proof_bytes", "steps")]
        lines.append("| " + row["phase"] + " | " + " | ".join(cells) + " |")
    lines += ["", "## 非时间不利样本", ""]
    if unfavorable:
        lines += [f'- {item["phase"]}: {json.dumps(item["increases"], ensure_ascii=False)}'
                  for item in unfavorable]
    else:
        lines += ["本次指定完整剧本未出现 bytes/read/seek/steps/work 增加的阶段；不外推未测输入。"]
    lines += ["", "## 边界", "",
              "排名 Ready、文字完整可用、最终文档接受分开记录；程序没有 LCD 呈现事件。",
              "所有准备／确认／重核均在总成本内。热缓存可能令 prepare/confirm raw I/O 为零，但生产身份与输出重核仍执行。",
              "当前数据只能判断访问成本，不能宣布实际交互合格；设备效果仍 OPEN。"]
    (args.output / "COSTS.md").write_text("\n".join(lines) + "\n")
    print("PASS", len(rows), "phases; functional equality; comparison:", args.output)


if __name__ == "__main__":
    main()
