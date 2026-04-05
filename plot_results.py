#!/usr/bin/env python3
"""
plot_results.py — Generate bar charts from simulator CSV output.

Usage:
    pip install matplotlib pandas
    python plot_results.py [--comparison output/policy_comparison.csv]
                           [--results   output/results.csv]
                           [--out       output/policy_comparison.png]
"""
import sys
import argparse
import os

try:
    import pandas as pd
    import matplotlib
    matplotlib.use("Agg")          # non-interactive backend
    import matplotlib.pyplot as plt
    import matplotlib.ticker as mticker
except ImportError as exc:
    sys.exit(f"Missing dependency: {exc}\nRun:  pip install matplotlib pandas")

# ── colour palette ──────────────────────────────────────────────────────────
COLOURS = ["#e94560", "#0f3460", "#533483", "#05c46b", "#f5a623"]

# ── argument parsing ─────────────────────────────────────────────────────────
def parse_args():
    p = argparse.ArgumentParser(description="Plot scheduler simulation results.")
    p.add_argument("--comparison", default="output/policy_comparison.csv",
                   help="Path to policy_comparison.csv")
    p.add_argument("--results",    default="output/results.csv",
                   help="Path to results.csv")
    p.add_argument("--out",        default="output/policy_comparison.png",
                   help="Output PNG file path")
    return p.parse_args()

# ── bar chart helper ─────────────────────────────────────────────────────────
def bar_chart(ax, df, col, ylabel, title, colours):
    bars = ax.bar(df["policy"], df[col], color=colours[:len(df)],
                  edgecolor="white", linewidth=0.6)
    ax.set_title(title, fontsize=11, fontweight="bold", color="#eee", pad=8)
    ax.set_ylabel(ylabel, fontsize=9, color="#ccc")
    ax.set_xlabel("Policy", fontsize=9, color="#ccc")
    ax.tick_params(colors="#ccc", labelsize=8)
    for spine in ax.spines.values():
        spine.set_edgecolor("#444")
    ax.set_facecolor("#1a1a2e")
    # Value labels on top of each bar.
    for bar in bars:
        h = bar.get_height()
        ax.text(bar.get_x() + bar.get_width() / 2.0, h * 1.01,
                f"{h:.2f}", ha="center", va="bottom",
                fontsize=7.5, color="#eee")

# ── main ─────────────────────────────────────────────────────────────────────
def main():
    args = parse_args()

    if not os.path.exists(args.comparison):
        sys.exit(f"File not found: {args.comparison}\n"
                 f"Run ./simulator first to generate CSV output.")

    df = pd.read_csv(args.comparison)
    if df.empty:
        sys.exit("policy_comparison.csv is empty.")

    # Rename columns for display if needed.
    rename = {
        "avg_wait_ms":     "avg_wait_ms",
        "avg_tat_ms":      "avg_tat_ms",
        "throughput":      "throughput",
        "utilization":     "utilization",
        "fairness_index":  "fairness_index",
    }
    df = df.rename(columns=rename)

    metrics = [
        ("avg_wait_ms",    "ms",         "Avg Waiting Time"),
        ("avg_tat_ms",     "ms",         "Avg Turnaround Time"),
        ("throughput",     "jobs/s",     "Throughput"),
        ("utilization",    "fraction",   "Server Utilization"),
        ("fairness_index", "0–1",        "Jain's Fairness Index"),
    ]

    fig, axes = plt.subplots(2, 3, figsize=(16, 9))
    fig.patch.set_facecolor("#16213e")
    fig.suptitle("Multithreaded File Transfer Scheduler — Policy Comparison",
                 fontsize=14, fontweight="bold", color="#e94560", y=1.01)

    axes_flat = axes.flatten()
    for idx, (col, unit, title) in enumerate(metrics):
        if col not in df.columns:
            axes_flat[idx].set_visible(False)
            continue
        bar_chart(axes_flat[idx], df, col,
                  ylabel=unit, title=title, colours=COLOURS)

    # Hide unused subplot.
    axes_flat[-1].set_visible(False)

    plt.tight_layout()
    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    plt.savefig(args.out, dpi=150, bbox_inches="tight",
                facecolor=fig.get_facecolor())
    print(f"Chart saved to: {args.out}")

    # Per-job scatter (wait time vs job size) if results.csv is available.
    if os.path.exists(args.results):
        results_df = pd.read_csv(args.results)
        if not results_df.empty and "wait_ms" in results_df.columns:
            scatter_path = args.out.replace(".png", "_scatter.png")
            fig2, ax2 = plt.subplots(figsize=(10, 6))
            fig2.patch.set_facecolor("#16213e")
            ax2.set_facecolor("#1a1a2e")
            policies = results_df["policy"].unique()
            for i, pol in enumerate(policies):
                sub = results_df[results_df["policy"] == pol]
                ax2.scatter(sub["size_mb"], sub["wait_ms"],
                            label=pol, color=COLOURS[i % len(COLOURS)],
                            alpha=0.7, s=40)
            ax2.set_title("Wait Time vs Job Size (all policies)",
                          fontsize=12, fontweight="bold", color="#eee")
            ax2.set_xlabel("Size (MB)", fontsize=10, color="#ccc")
            ax2.set_ylabel("Wait Time (ms)", fontsize=10, color="#ccc")
            ax2.tick_params(colors="#ccc")
            for spine in ax2.spines.values():
                spine.set_edgecolor("#444")
            ax2.legend(facecolor="#16213e", labelcolor="#eee", fontsize=9)
            plt.tight_layout()
            plt.savefig(scatter_path, dpi=150, bbox_inches="tight",
                        facecolor=fig2.get_facecolor())
            print(f"Scatter chart saved to: {scatter_path}")

if __name__ == "__main__":
    main()
