"""Build the report charts from measured, summarized results."""

from __future__ import annotations

import csv
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib.ticker import MultipleLocator


ROOT = Path(__file__).resolve().parent.parent
SUMMARY = ROOT / "results" / "summary.csv"
FIGURES = ROOT / "figures"
KINDS = (
    ("uniform", "Равномерное распределение"),
    ("zipf", "Распределение Ципфа"),
    ("normal", "Нормальное распределение"),
    ("repeat", "Повторы подряд"),
    ("hotshift20", "Смена ключей № 1: 5 ключей / 20 запросов"),
    ("hotshift", "Смена ключей № 2: 10 ключей / 80 запросов"),
    ("hotshift400", "Смена ключей № 3: 20 ключей / 400 запросов"),
    ("mixed", "Смешанная: Ципф / смена через 80"),
)
ALGORITHMS = ("LRU", "ARC", "2Q", "LFU", "LIRS")
CAPACITIES = (10, 25, 50, 75, 100, 125, 150, 175, 200)
COLORS = {
    "LRU": "#4477AA",
    "ARC": "#0086A8",
    "2Q": "#228833",
    "LFU": "#9A7900",
    "LIRS": "#C64A62",
    "BELADY": "#663399",
}
LINE_STYLES = {
    "LRU": ("-", "o"),
    "ARC": ("--", "s"),
    "2Q": ("-.", "^"),
    "LFU": (":", "D"),
    "LIRS": ("-", "P"),
    "BELADY": ("--", "X"),
}


def read_rates():
    with SUMMARY.open(newline="", encoding="utf-8") as file:
        rows = list(csv.DictReader(file))
    rates = {
        (row["kind"], int(row["capacity"]), row["config"]):
            float(row["mean_hit_rate_percent"])
        for row in rows
    }
    for kind, _ in KINDS:
        for capacity in CAPACITIES:
            for algorithm in (*ALGORITHMS, "BELADY"):
                key = kind, capacity, f"{algorithm}_{capacity}"
                if key not in rates:
                    raise ValueError(f"Missing result: {key}")
    deviations = {
        (row["kind"], int(row["capacity"]), row["config"]):
            float(row["stddev_hit_rate_percentage_points"])
        for row in rows
    }
    return rates, deviations


def plot_comparison(rates, deviations) -> None:
    fig, axes = plt.subplots(4, 2, figsize=(12, 14), layout="constrained")
    for ax, (kind, title) in zip(axes.flat, KINDS):
        values = [rates[kind, 100, f"{algorithm}_100"] for algorithm in ALGORITHMS]
        ideal = rates[kind, 100, "BELADY_100"]
        colors = [COLORS[a] for a in ALGORITHMS]
        errors = [deviations[kind, 100, f"{a}_100"] for a in ALGORITHMS]
        ax.bar(ALGORITHMS, values, color=colors, width=0.72,
               yerr=errors, capsize=4, error_kw={"elinewidth": 1.2})
        span = max(values) - min(values)
        pad = max(0.15, span * 0.45, max(errors) * 1.8)
        lower = max(0.0, min(values) - pad)
        upper = min(100.0, max(values) + pad * 2.0)
        ax.set_ylim(lower, upper)
        ax.set_title(title, fontweight="bold")
        ax.text(0.98, 0.97, f"Белади: {ideal:.2f} %",
                ha="right", va="top", color=COLORS["BELADY"],
                transform=ax.transAxes, fontsize=10,
                bbox={"facecolor": "white", "edgecolor": COLORS["BELADY"],
                      "boxstyle": "round,pad=0.3"})
        for x, value in enumerate(values):
            delta = 100 * (value - ideal) / ideal
            ax.text(x, value + errors[x] + pad * 0.12, f"Δ {delta:+.2f} %",
                    ha="center", va="bottom", fontsize=9)
        ax.set_ylabel(f"Доля попаданий, %\n(ось от {lower:.2f} %)")
        ax.grid(axis="y", color="#dddddd", linewidth=0.7)
        ax.set_axisbelow(True)
    fig.suptitle("Сравнение кешей при ёмкости 100; усы: ±1 s", fontsize=16, fontweight="bold")
    fig.savefig(FIGURES / "cache_comparison_100.png", dpi=200)
    plt.close(fig)


def plot_capacity(rates, deviations) -> None:
    fig, axes = plt.subplots(4, 2, figsize=(16, 15), layout="constrained")
    all_algorithms = (*ALGORITHMS, "BELADY")
    for ax, (kind, title) in zip(axes.flat, KINDS):
        for algorithm in all_algorithms:
            values = [rates[kind, capacity, f"{algorithm}_{capacity}"]
                      for capacity in CAPACITIES]
            line_style, marker = LINE_STYLES[algorithm]
            errors = [deviations[kind, capacity, f"{algorithm}_{capacity}"]
                      for capacity in CAPACITIES]
            ax.errorbar(CAPACITIES, values, yerr=errors, capsize=3,
                    elinewidth=1, color=COLORS[algorithm],
                    marker=marker, markersize=6, linewidth=1.7,
                    linestyle=line_style,
                    label="Белади" if algorithm == "BELADY" else algorithm)
        ax.set_title(title, fontweight="bold")
        ax.set_xticks(range(0, 201, 20))
        ax.xaxis.set_minor_locator(MultipleLocator(10))
        ax.set_xlim(0, 200)
        ax.set_ylim(0, 100)
        ax.set_yticks(range(0, 101, 20))
        ax.yaxis.set_minor_locator(MultipleLocator(10))
        ax.set_xlabel("Ёмкость кеша, ключей")
        ax.set_ylabel("Доля попаданий, %")
        ax.set_aspect("equal", adjustable="box")
        ax.set_facecolor("#f8fafc")
        ax.grid(which="both", axis="both", color="#d1d9e2", linewidth=0.5)
        ax.set_axisbelow(True)


    handles, labels = axes.flat[0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="outside lower center", ncol=6,
               frameon=False, fontsize=12, title="Алгоритмы")
    fig.suptitle("Доля попаданий при разных ёмкостях; усы: ±1 s",
                 fontsize=17, fontweight="bold")
    fig.savefig(FIGURES / "cache_capacity_all_scenarios.png", dpi=200)
    plt.close(fig)


def main() -> None:
    plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 10})
    FIGURES.mkdir(exist_ok=True)
    rates, deviations = read_rates()
    plot_comparison(rates, deviations)
    plot_capacity(rates, deviations)
    print("Wrote cache_comparison_100.png and cache_capacity_all_scenarios.png")


if __name__ == "__main__":
    main()
