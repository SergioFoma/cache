"""Generate the cache request datasets described in REPORT.md."""

from __future__ import annotations

import argparse
import bisect
import hashlib
import json
import random
from itertools import accumulate
from pathlib import Path


KEY_COUNT = 400
WARMUP_COUNT = 5_000
MEASURED_COUNT = 20_000
REQUEST_COUNT = WARMUP_COUNT + MEASURED_COUNT
SEEDS = range(40)
HOTSHIFT_WINDOWS = {"hotshift20": 20, "hotshift": 80, "hotshift400": 400}
HOTSHIFT_SET_SIZES = {"hotshift20": 5, "hotshift": 10, "hotshift400": 20}
MIXED_HOTSHIFT_SET_SIZE = 10
HOTSHIFT_PROBABILITY = 0.8
HOTSHIFT_FIRST_KEY = KEY_COUNT
MIXED_BLOCK_SIZE = 2_000
MIXED_PHASES = ("zipf", "hotshift80")
MIXED_HOTSHIFT_WINDOW = 80
MIXED_GROUPS_PER_BLOCK = MIXED_BLOCK_SIZE // MIXED_HOTSHIFT_WINDOW
ZIPF_CUMULATIVE = tuple(accumulate((key + 1) ** -1.2 for key in range(KEY_COUNT)))
ZIPF_TOTAL = ZIPF_CUMULATIVE[-1]
DATA_DIR = Path(__file__).resolve().parent.parent / "data"


def zipf_key(rng: random.Random) -> int:
    return bisect.bisect_left(ZIPF_CUMULATIVE, rng.random() * ZIPF_TOTAL)


def normal_key(rng: random.Random) -> int:
    key = round(rng.gauss(199.5, 60))
    while not 0 <= key < KEY_COUNT:
        key = round(rng.gauss(199.5, 60))
    return key


def generate(kind: str, seed: int) -> list[int]:
    rng = random.Random(seed)
    if kind == "uniform":
        return [rng.randrange(KEY_COUNT) for _ in range(REQUEST_COUNT)]
    if kind == "zipf":
        return [zipf_key(rng) for _ in range(REQUEST_COUNT)]
    if kind == "normal":
        return [normal_key(rng) for _ in range(REQUEST_COUNT)]
    if kind == "repeat":
        keys = [rng.randrange(KEY_COUNT)]
        for _ in range(REQUEST_COUNT - 1):
            keys.append(keys[-1] if rng.random() < 0.8 else rng.randrange(KEY_COUNT))
        return keys
    if kind in HOTSHIFT_WINDOWS:
        window = HOTSHIFT_WINDOWS[kind]
        group_size = HOTSHIFT_SET_SIZES[kind]
        keys = []
        for position in range(REQUEST_COUNT):
            phase = position // window
            if rng.random() < HOTSHIFT_PROBABILITY:
                keys.append(HOTSHIFT_FIRST_KEY + phase * group_size
                            + rng.randrange(group_size))
            else:
                keys.append(rng.randrange(KEY_COUNT))
        return keys
    if kind == "mixed":
        keys = []
        hot_group = 0
        for block in range(REQUEST_COUNT // MIXED_BLOCK_SIZE):
            phase = MIXED_PHASES[block % len(MIXED_PHASES)]
            for offset in range(MIXED_BLOCK_SIZE):
                if phase == "zipf":
                    key = zipf_key(rng)
                else:
                    group = hot_group + offset // MIXED_HOTSHIFT_WINDOW
                    key = (HOTSHIFT_FIRST_KEY + group * MIXED_HOTSHIFT_SET_SIZE
                           + rng.randrange(MIXED_HOTSHIFT_SET_SIZE)) if rng.random() < HOTSHIFT_PROBABILITY else rng.randrange(KEY_COUNT)
                keys.append(key)
            if phase != "zipf":
                hot_group += MIXED_GROUPS_PER_BLOCK
        tail = REQUEST_COUNT - len(keys)
        if tail:
            phase = MIXED_PHASES[(REQUEST_COUNT // MIXED_BLOCK_SIZE) % len(MIXED_PHASES)]
            if phase == "zipf":
                keys.extend(zipf_key(rng) for _ in range(tail))
            else:
                for offset in range(tail):
                    group = hot_group + offset // MIXED_HOTSHIFT_WINDOW
                    key = (HOTSHIFT_FIRST_KEY + group * MIXED_HOTSHIFT_SET_SIZE
                           + rng.randrange(MIXED_HOTSHIFT_SET_SIZE)) if rng.random() < HOTSHIFT_PROBABILITY else rng.randrange(KEY_COUNT)
                    keys.append(key)
        return keys
    raise ValueError(f"Unknown sequence kind: {kind}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--capacity",
        type=int,
        default=100,
        help="cache capacity in the input-file header (default: 100)",
    )
    args = parser.parse_args()
    if args.capacity <= 0:
        parser.error("capacity must be positive")

    DATA_DIR.mkdir(parents=True, exist_ok=True)
    datasets = []
    for kind in ("uniform", "zipf", "normal", "repeat", "hotshift20", "hotshift", "hotshift400", "mixed"):
        for seed in SEEDS:
            keys = generate(kind, seed)
            name = f"{kind}_{seed:02d}.txt"
            content = f"{args.capacity} {REQUEST_COUNT}\n" + " ".join(map(str, keys)) + "\n"
            (DATA_DIR / name).write_text(content, encoding="ascii")
            datasets.append(
                {
                    "file": name,
                    "kind": kind,
                    "seed": seed,
                    "sha256": hashlib.sha256(content.encode("ascii")).hexdigest(),
                }
            )

    manifest = {
        "capacity_in_file": args.capacity,
        "base_key_range": [0, KEY_COUNT - 1],
        "hotshift_key_ranges": {
            kind: [HOTSHIFT_FIRST_KEY,
                   HOTSHIFT_FIRST_KEY +
                   ((REQUEST_COUNT - 1) // window + 1) * HOTSHIFT_SET_SIZES[kind] - 1]
            for kind, window in HOTSHIFT_WINDOWS.items()
        },
        "warmup_requests": WARMUP_COUNT,
        "measured_requests": MEASURED_COUNT,
        "datasets_per_kind": len(SEEDS),
        "zipf_exponent": 1.2,
        "normal_mean": 199.5,
        "normal_standard_deviation": 60,
        "repeat_probability": 0.8,
        "repeat_new_key_distribution": "uniform(keys=0..399)",
        "hotshift_windows": HOTSHIFT_WINDOWS,
        "hotshift_set_sizes": HOTSHIFT_SET_SIZES,
        "hotshift_probability": HOTSHIFT_PROBABILITY,
        "mixed_block_size": MIXED_BLOCK_SIZE,
        "mixed_phases": MIXED_PHASES,
        "mixed_hotshift_window": MIXED_HOTSHIFT_WINDOW,
        "mixed_groups_per_block": MIXED_GROUPS_PER_BLOCK,
        "mixed_hotshift_first_key": HOTSHIFT_FIRST_KEY,
        "mixed_hotshift_set_size": MIXED_HOTSHIFT_SET_SIZE,
        "mixed_hotshift_probability": HOTSHIFT_PROBABILITY,
        "mixed_calm_distribution": "zipf(exponent=1.2, keys=0..399)",
        "mixed_hotshift_background": "uniform(keys=0..399)",
        "datasets": datasets,
    }
    (DATA_DIR / "manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print(f"Generated {len(datasets)} datasets in {DATA_DIR}")


if __name__ == "__main__":
    main()
