"""Measure every ordered two-level pair on the mixed workload in 5% splits."""

from __future__ import annotations

import csv
import json
import statistics
import subprocess
import tempfile
from collections import defaultdict
from pathlib import Path


ROOT = Path(__file__).resolve().parent.parent
PROJECT = ROOT.parent
ALGORITHMS = ("LRU", "ARC", "2Q", "LFU", "LIRS")
SPLITS = range(5, 100, 5)
SEEDS = set(range(40))


def main() -> None:
    binary = PROJECT / "build" / "report_measure"
    if not binary.is_file():
        raise FileNotFoundError(f"Сначала соберите программу измерений: {binary}")

    results = ROOT / "results"
    results.mkdir(exist_ok=True)
    measurements = results / "full_mixed_measurements.csv"
    summary_path = results / "full_mixed_summary.csv"

    with tempfile.TemporaryDirectory(prefix="cache-mixed-sweep-") as temporary:
        configs = Path(temporary)
        names = []
        for first in ALGORITHMS:
            for second in ALGORITHMS:
                for first_capacity in SPLITS:
                    second_capacity = 100 - first_capacity
                    name = f"{first}{first_capacity}_{second}{second_capacity}"
                    config = {"cache": [
                        {"name": first, "size": first_capacity},
                        {"name": second, "size": second_capacity},
                    ]}
                    (configs / f"{name}.json").write_text(
                        json.dumps(config, ensure_ascii=False), encoding="utf-8"
                    )
                    names.append((name, first, first_capacity, second, second_capacity))

        subprocess.run([
            str(binary), str(ROOT / "data"), str(configs),
            str(measurements), "mixed",
        ], check=True)

    rates: dict[str, list[float]] = defaultdict(list)
    seeds: dict[str, set[int]] = defaultdict(set)
    with measurements.open(newline="", encoding="utf-8") as file:
        rows = list(csv.DictReader(file))
    expected = len(SEEDS) * (len(names) + 4)
    if len(rows) != expected:
        raise ValueError(f"Ожидалось {expected} измерений, получено {len(rows)}")
    for row in rows:
        config = row["config"]
        seed = int(row["seed"])
        hits = int(row["hits"])
        misses = int(row["misses"])
        if row["kind"] != "mixed" or hits + misses != 20_000 or seed in seeds[config]:
            raise ValueError(f"Некорректное измерение: {row}")
        rates[config].append(100 * hits / 20_000)
        seeds[config].add(seed)

    expected_names = {name for name, *_ in names} | {
        f"BELADY_{capacity}" for capacity in (10, 50, 100, 200)
    }
    if set(rates) != expected_names or any(found != SEEDS for found in seeds.values()):
        raise ValueError("Неполные конфигурации или наборы seed")

    belady = statistics.mean(rates["BELADY_100"])
    summary = []
    for name, first, first_capacity, second, second_capacity in names:
        values = rates[name]
        mean = statistics.mean(values)
        summary.append({
            "config": name,
            "first_algorithm": first,
            "first_capacity": first_capacity,
            "second_algorithm": second,
            "second_capacity": second_capacity,
            "datasets": len(values),
            "mean_hit_rate_percent": f"{mean:.6f}",
            "stddev_hit_rate_percentage_points": f"{statistics.stdev(values):.6f}",
            "delta_vs_BELADY100_percentage_points": f"{mean - belady:.6f}",
        })
    with summary_path.open("w", newline="", encoding="utf-8") as file:
        writer = csv.DictWriter(file, fieldnames=list(summary[0]))
        writer.writeheader()
        writer.writerows(summary)

    best = max(summary, key=lambda row: float(row["mean_hit_rate_percent"]))
    print(f"{len(names)} конфигураций × {len(SEEDS)} датасетов; "
          f"лучший вариант {best['config']}: {best['mean_hit_rate_percent']} %")
    print(f"Результаты: {measurements} и {summary_path}")


if __name__ == "__main__":
    main()
