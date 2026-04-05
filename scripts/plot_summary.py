#!/usr/bin/env python3
"""Plot benchmark summary CSV as bar charts.

Usage:
  python scripts/plot_summary.py --input output/summary.csv --output output/figures
"""

import argparse
import csv
import os
from typing import Dict, List

import matplotlib.pyplot as plt


def load_rows(path: str) -> List[Dict[str, str]]:
    with open(path, "r", newline="", encoding="utf-8") as f:
        reader = csv.DictReader(f)
        return list(reader)


def ensure_dir(path: str) -> None:
    os.makedirs(path, exist_ok=True)


def to_float(rows: List[Dict[str, str]], key: str) -> List[float]:
    return [float(r[key]) for r in rows]


def plot_metric(
    schedulers: List[str], values: List[float], title: str, ylabel: str, output_path: str
) -> None:
    plt.figure(figsize=(9, 5))
    colors = ["#2E86AB", "#F18F01", "#A23B72", "#3B8EA5", "#6C9A8B"]
    plt.bar(schedulers, values, color=colors[: len(schedulers)])
    plt.title(title)
    plt.ylabel(ylabel)
    plt.grid(axis="y", linestyle="--", alpha=0.3)
    plt.tight_layout()
    plt.savefig(output_path, dpi=180)
    plt.close()


def main() -> None:
    parser = argparse.ArgumentParser(description="Plot scheduler benchmark summary")
    parser.add_argument("--input", required=True, help="Path to summary.csv")
    parser.add_argument("--output", default="output/figures", help="Output directory")
    args = parser.parse_args()

    rows = load_rows(args.input)
    if not rows:
        raise RuntimeError("summary.csv has no rows")

    ensure_dir(args.output)

    schedulers = [r["scheduler"] for r in rows]

    plot_metric(
        schedulers,
        to_float(rows, "avg_waiting_time"),
        "Average Waiting Time by Scheduler",
        "seconds",
        os.path.join(args.output, "avg_waiting_time.png"),
    )

    plot_metric(
        schedulers,
        to_float(rows, "avg_turnaround_time"),
        "Average Turnaround Time by Scheduler",
        "seconds",
        os.path.join(args.output, "avg_turnaround_time.png"),
    )

    plot_metric(
        schedulers,
        to_float(rows, "throughput"),
        "Throughput by Scheduler",
        "requests / second",
        os.path.join(args.output, "throughput.png"),
    )

    plot_metric(
        schedulers,
        to_float(rows, "fairness_index"),
        "Jain Fairness Index by Scheduler",
        "index (0~1)",
        os.path.join(args.output, "fairness_index.png"),
    )

    print(f"Charts written to {args.output}")


if __name__ == "__main__":
    main()
