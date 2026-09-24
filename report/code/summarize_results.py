"""Validate raw cache measurements and summarize the 40 seeds per workload."""

from __future__ import annotations

import csv
import hashlib
import json
import statistics
from collections import defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
DATA = ROOT / "data"
RESULTS = ROOT / "results"


def main() -> None:
    manifest = json.loads((DATA / "manifest.json").read_text(encoding="utf-8"))
    datasets = manifest["datasets"]
    assert len(datasets) == manifest["datasets_per_kind"] * 8
    for dataset in datasets:
        content = (DATA / dataset["file"]).read_bytes()
        if hashlib.sha256(content).hexdigest() != dataset["sha256"]:
            raise ValueError(f"Checksum mismatch: {dataset['file']}")

    with (RESULTS / "measurements.csv").open(newline="", encoding="utf-8") as file:
        rows = list(csv.DictReader(file))
    configs = [json.loads(path.read_text()) for path in (ROOT / "config").glob("*.json")]
    capacities = {10, 50, 100, 200} | {
        config["cache"][0]["size"] for config in configs if len(config["cache"]) == 1
    }
    expected_configs = len(configs) + len(capacities)
    assert len(rows) == len(datasets) * expected_configs, len(rows)
    expected_datasets = {dataset["file"]: dataset for dataset in datasets}
    groups: dict[tuple[str, int, str], list[dict[str, str]]] = defaultdict(list)
    unique = set()
    for row in rows:
        dataset = expected_datasets[row["dataset"]]
        assert row["kind"] == dataset["kind"]
        assert int(row["seed"]) == dataset["seed"]
        assert int(row["requests"]) == manifest["measured_requests"]
        assert int(row["hits"]) + int(row["misses"]) == int(row["requests"])
        assert abs(float(row["hit_rate_percent"]) - 100 * int(row["hits"]) / int(row["requests"])) < 1e-5
        key = (row["dataset"], row["config"])
        assert key not in unique, key
        unique.add(key)
        groups[(row["kind"], int(row["capacity"]), row["config"])].append(row)

    summary = []
    for (kind, capacity, config), group in sorted(groups.items()):
        assert len(group) == 40, (kind, capacity, config, len(group))
        rates = [100 * int(row["hits"]) / int(row["requests"]) for row in group]
        hits = [int(row["hits"]) for row in group]
        summary.append({
            "kind": kind,
            "capacity": capacity,
            "config": config,
            "levels": group[0]["levels"],
            "datasets": len(group),
            "mean_hits": f"{statistics.mean(hits):.3f}",
            "mean_hit_rate_percent": f"{statistics.mean(rates):.6f}",
            "variance_hit_rate_percentage_points_squared": f"{statistics.variance(rates):.6f}",
            "stddev_hit_rate_percentage_points": f"{statistics.stdev(rates):.6f}",
            "min_hit_rate_percent": f"{min(rates):.6f}",
            "max_hit_rate_percent": f"{max(rates):.6f}",
        })

    path = RESULTS / "summary.csv"
    with path.open("w", newline="", encoding="utf-8") as file:
        writer = csv.DictWriter(file, fieldnames=summary[0])
        writer.writeheader()
        writer.writerows(summary)
    print(f"Validated {len(rows)} measurements and wrote {len(summary)} summary rows")


if __name__ == "__main__":
    main()
