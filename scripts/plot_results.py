#!/usr/bin/env python3
"""
plot_results.py — generate all experiment graphs from results/results.csv
and results/latency_samples.csv.

Dependencies: pandas, matplotlib (pip install pandas matplotlib)

Generates 12 PNGs in results/plots/:
  w{1,2}_abort_vs_contention.png
  w{1,2}_throughput_vs_threads.png
  w{1,2}_throughput_vs_contention.png
  w{1,2}_latency_vs_threads.png
  w{1,2}_latency_vs_contention.png
  w{1,2}_latency_distribution.png
"""

import os
import sys
import pandas as pd
import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt

RESULTS_DIR = "results"
PLOTS_DIR   = os.path.join(RESULTS_DIR, "plots")
CSV_PATH    = os.path.join(RESULTS_DIR, "results.csv")
LAT_PATH    = os.path.join(RESULTS_DIR, "latency_samples.csv")

PROTOCOL_COLORS = {"occ": "#e05c5c", "2pl": "#4c87c8"}
LINESTYLES      = ["-", "--", "-.", ":"]


def load_data():
    if not os.path.exists(CSV_PATH):
        print(f"ERROR: {CSV_PATH} not found. Run scripts/run_experiments.sh first.")
        sys.exit(1)
    df = pd.read_csv(CSV_PATH)
    df["workload"] = df["workload"].astype(str)
    return df


def load_latency_data():
    if not os.path.exists(LAT_PATH):
        return None
    df = pd.read_csv(LAT_PATH)
    df["workload"] = df["workload"].astype(str)
    return df


def save_fig(name):
    os.makedirs(PLOTS_DIR, exist_ok=True)
    path = os.path.join(PLOTS_DIR, name)
    plt.savefig(path, dpi=150, bbox_inches="tight")
    plt.close()
    print(f"  Saved: {path}")


# ---------------------------------------------------------------------------
# 1. Abort rate vs. contention (hotset_prob), per txn_type, threads=4
# ---------------------------------------------------------------------------
def plot_abort_vs_contention(df, workload):
    sub = df[(df["workload"] == workload) & (df["threads"] == 4)].copy()
    if sub.empty:
        print(f"  [w{workload}] No data for abort_vs_contention (threads=4). Skipping.")
        return

    fig, ax = plt.subplots(figsize=(7, 5))
    txn_types = sub["txn_type"].unique()

    for i, txn_type in enumerate(sorted(txn_types)):
        for protocol, color in PROTOCOL_COLORS.items():
            rows = sub[(sub["txn_type"] == protocol_col(txn_type, sub)) |
                       ((sub["txn_type"] == txn_type) & (sub["protocol"] == protocol))]
            rows = sub[(sub["txn_type"] == txn_type) & (sub["protocol"] == protocol)]
            if rows.empty:
                continue
            grouped = rows.groupby("hotset_prob")["type_abort_pct"].mean().reset_index()
            label = f"{protocol} / {txn_type}"
            ax.plot(grouped["hotset_prob"], grouped["type_abort_pct"],
                    color=color, linestyle=LINESTYLES[i % len(LINESTYLES)],
                    marker="o", label=label)

    ax.set_xlabel("Hotset Probability (contention)")
    ax.set_ylabel("Abort Rate (%)")
    ax.set_title(f"Workload {workload}: Abort Rate vs. Contention (threads=4)")
    ax.legend(fontsize=8)
    ax.grid(True, alpha=0.3)
    save_fig(f"w{workload}_abort_vs_contention.png")


def protocol_col(txn_type, df):
    """Helper — unused, kept for clarity."""
    return txn_type


# ---------------------------------------------------------------------------
# 2. Throughput vs. threads, hotset_prob=0.7
# ---------------------------------------------------------------------------
def plot_throughput_vs_threads(df, workload):
    sub = df[(df["workload"] == workload) & (df["hotset_prob"] == 0.7)].copy()
    if sub.empty:
        print(f"  [w{workload}] No data for throughput_vs_threads (hotset=0.7). Skipping.")
        return

    # Throughput is per-run (same for all txn_type rows), take first occurrence per run group
    run_df = sub.drop_duplicates(subset=["protocol", "threads", "hotset_prob"])

    fig, ax = plt.subplots(figsize=(7, 5))
    for protocol, color in PROTOCOL_COLORS.items():
        rows = run_df[run_df["protocol"] == protocol]
        if rows.empty:
            continue
        grouped = rows.groupby("threads")["throughput_tps"].mean().reset_index()
        ax.plot(grouped["threads"], grouped["throughput_tps"],
                color=color, marker="o", label=protocol.upper())

    ax.set_xlabel("Number of Threads")
    ax.set_ylabel("Throughput (txn/s)")
    ax.set_title(f"Workload {workload}: Throughput vs. Threads (hotset_prob=0.7)")
    ax.legend()
    ax.grid(True, alpha=0.3)
    save_fig(f"w{workload}_throughput_vs_threads.png")


# ---------------------------------------------------------------------------
# 3. Throughput vs. contention, threads=4
# ---------------------------------------------------------------------------
def plot_throughput_vs_contention(df, workload):
    sub = df[(df["workload"] == workload) & (df["threads"] == 4)].copy()
    if sub.empty:
        print(f"  [w{workload}] No data for throughput_vs_contention (threads=4). Skipping.")
        return

    run_df = sub.drop_duplicates(subset=["protocol", "threads", "hotset_prob"])

    fig, ax = plt.subplots(figsize=(7, 5))
    for protocol, color in PROTOCOL_COLORS.items():
        rows = run_df[run_df["protocol"] == protocol]
        if rows.empty:
            continue
        grouped = rows.groupby("hotset_prob")["throughput_tps"].mean().reset_index()
        ax.plot(grouped["hotset_prob"], grouped["throughput_tps"],
                color=color, marker="o", label=protocol.upper())

    ax.set_xlabel("Hotset Probability (contention)")
    ax.set_ylabel("Throughput (txn/s)")
    ax.set_title(f"Workload {workload}: Throughput vs. Contention (threads=4)")
    ax.legend()
    ax.grid(True, alpha=0.3)
    save_fig(f"w{workload}_throughput_vs_contention.png")


# ---------------------------------------------------------------------------
# 4. Avg latency vs. threads, per txn_type, hotset_prob=0.7
# ---------------------------------------------------------------------------
def plot_latency_vs_threads(df, workload):
    sub = df[(df["workload"] == workload) & (df["hotset_prob"] == 0.7)].copy()
    if sub.empty:
        print(f"  [w{workload}] No data for latency_vs_threads (hotset=0.7). Skipping.")
        return

    fig, ax = plt.subplots(figsize=(7, 5))
    txn_types = sorted(sub["txn_type"].unique())

    for i, txn_type in enumerate(txn_types):
        for protocol, color in PROTOCOL_COLORS.items():
            rows = sub[(sub["txn_type"] == txn_type) & (sub["protocol"] == protocol)]
            if rows.empty:
                continue
            grouped = rows.groupby("threads")["type_avg_latency_us"].mean().reset_index()
            ax.plot(grouped["threads"], grouped["type_avg_latency_us"],
                    color=color, linestyle=LINESTYLES[i % len(LINESTYLES)],
                    marker="o", label=f"{protocol} / {txn_type}")

    ax.set_xlabel("Number of Threads")
    ax.set_ylabel("Avg Latency (µs)")
    ax.set_title(f"Workload {workload}: Latency vs. Threads (hotset_prob=0.7)")
    ax.legend(fontsize=8)
    ax.grid(True, alpha=0.3)
    save_fig(f"w{workload}_latency_vs_threads.png")


# ---------------------------------------------------------------------------
# 5. Avg latency vs. contention, per txn_type, threads=4
# ---------------------------------------------------------------------------
def plot_latency_vs_contention(df, workload):
    sub = df[(df["workload"] == workload) & (df["threads"] == 4)].copy()
    if sub.empty:
        print(f"  [w{workload}] No data for latency_vs_contention (threads=4). Skipping.")
        return

    fig, ax = plt.subplots(figsize=(7, 5))
    txn_types = sorted(sub["txn_type"].unique())

    for i, txn_type in enumerate(txn_types):
        for protocol, color in PROTOCOL_COLORS.items():
            rows = sub[(sub["txn_type"] == txn_type) & (sub["protocol"] == protocol)]
            if rows.empty:
                continue
            grouped = rows.groupby("hotset_prob")["type_avg_latency_us"].mean().reset_index()
            ax.plot(grouped["hotset_prob"], grouped["type_avg_latency_us"],
                    color=color, linestyle=LINESTYLES[i % len(LINESTYLES)],
                    marker="o", label=f"{protocol} / {txn_type}")

    ax.set_xlabel("Hotset Probability (contention)")
    ax.set_ylabel("Avg Latency (µs)")
    ax.set_title(f"Workload {workload}: Latency vs. Contention (threads=4)")
    ax.legend(fontsize=8)
    ax.grid(True, alpha=0.3)
    save_fig(f"w{workload}_latency_distribution.png")


# ---------------------------------------------------------------------------
# 6. Latency distribution (histogram) — representative run
# ---------------------------------------------------------------------------
def plot_latency_distribution(lat_df, workload):
    if lat_df is None:
        print(f"  [w{workload}] No latency_samples.csv found. Skipping distribution plot.")
        return

    sub = lat_df[lat_df["workload"] == workload].copy()
    if sub.empty:
        print(f"  [w{workload}] No latency samples for workload {workload}. Skipping.")
        return

    fig, ax = plt.subplots(figsize=(8, 5))
    txn_types = sorted(sub["txn_type"].unique())

    for i, txn_type in enumerate(txn_types):
        for protocol, color in PROTOCOL_COLORS.items():
            rows = sub[(sub["txn_type"] == txn_type) & (sub["protocol"] == protocol)]
            if rows.empty:
                continue
            ax.hist(rows["latency_us"], bins=50, alpha=0.5, color=color,
                    linestyle=LINESTYLES[i % len(LINESTYLES)],
                    label=f"{protocol} / {txn_type}", density=True)

    ax.set_xlabel("Latency (µs)")
    ax.set_ylabel("Density")
    ax.set_title(f"Workload {workload}: Latency Distribution (representative run: threads=4, hotset=0.7)")
    ax.legend(fontsize=8)
    ax.grid(True, alpha=0.3)
    save_fig(f"w{workload}_latency_distribution.png")


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------
def main():
    print(f"Loading {CSV_PATH}...")
    df = load_data()
    print(f"  {len(df)} rows loaded.")

    lat_df = load_latency_data()
    if lat_df is not None:
        print(f"  {len(lat_df)} latency samples loaded.")

    os.makedirs(PLOTS_DIR, exist_ok=True)

    for workload in ["1", "2"]:
        print(f"\n--- Workload {workload} ---")
        plot_abort_vs_contention(df, workload)
        plot_throughput_vs_threads(df, workload)
        plot_throughput_vs_contention(df, workload)
        plot_latency_vs_threads(df, workload)
        plot_latency_vs_contention(df, workload)
        plot_latency_distribution(lat_df, workload)

    print(f"\nDone. Plots saved to {PLOTS_DIR}/")


if __name__ == "__main__":
    main()
