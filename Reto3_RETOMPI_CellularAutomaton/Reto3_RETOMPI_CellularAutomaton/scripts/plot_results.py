#!/usr/bin/env python3
"""Genera los graficos a partir de los CSV de results/."""
from __future__ import annotations

import csv
import os
import sys
from collections import defaultdict
from pathlib import Path

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = Path(__file__).resolve().parents[1]
RES = ROOT / "results"
PLOTS = ROOT / "plots"
PLOTS.mkdir(exist_ok=True)


def read_csv(path: Path) -> list[dict]:
    if not path.exists():
        return []
    with path.open() as fp:
        return list(csv.DictReader(fp))


def to_float(rows: list[dict], key: str) -> list[float]:
    return [float(r[key]) for r in rows]


def plot_velocity_density() -> None:
    rows = read_csv(RES / "velocity_density.csv")
    if not rows:
        print("[skip] velocity_density.csv vacio")
        return
    rows.sort(key=lambda r: float(r["density"]))
    d = to_float(rows, "actual_density")
    v = to_float(rows, "v_asymp")

    plt.figure(figsize=(7, 4.5))
    plt.plot(d, v, "o-", linewidth=2, markersize=6, color="#1f77b4")
    plt.plot([0, 0.5, 1], [0, 1.0, 0], "--", color="grey", alpha=0.6,
             label="Limite teorico (rule 184)")
    plt.xlabel("Densidad de carros")
    plt.ylabel("Velocidad asintotica promedio")
    plt.title("Velocidad media vs densidad (autómata regla 184)")
    plt.grid(alpha=0.3)
    plt.legend()
    plt.tight_layout()
    out = PLOTS / "velocity_vs_density.png"
    plt.savefig(out, dpi=130)
    plt.close()
    print(f"[ok] {out}")


def aggregate(rows: list[dict], group_keys: tuple[str, ...], value_key: str):
    bucket: dict[tuple, list[float]] = defaultdict(list)
    for r in rows:
        key = tuple(r[k] for k in group_keys)
        bucket[key].append(float(r[value_key]))
    return {k: (sum(v) / len(v), min(v), max(v)) for k, v in bucket.items()}


def plot_scaling() -> None:
    rows = read_csv(RES / "scaling.csv")
    if not rows:
        print("[skip] scaling.csv vacio")
        return

    serial = [float(r["time_s"]) for r in rows if r["version"] == "serial"]
    t_serial = min(serial) if serial else None

    omp_rows = [r for r in rows if r["version"] == "omp"]
    agg = aggregate(omp_rows, ("schedule", "threads"), "time_s")

    schedules = sorted({r["schedule"] for r in omp_rows})
    thread_set = sorted({int(r["threads"]) for r in omp_rows})

    fig, (ax1, ax2) = plt.subplots(1, 2, figsize=(13, 4.7))
    colors = {"static": "#1f77b4", "dynamic": "#d62728", "guided": "#2ca02c"}

    for sch in schedules:
        ts = []
        speedups = []
        threads = []
        for t in thread_set:
            key = (sch, str(t))
            if key not in agg:
                continue
            mean = agg[key][0]
            ts.append(mean)
            threads.append(t)
            if t_serial is not None:
                speedups.append(t_serial / mean)
        ax1.plot(threads, ts, "o-", label=sch, color=colors.get(sch))
        if speedups:
            ax2.plot(threads, speedups, "o-", label=sch, color=colors.get(sch))

    ax2.plot(thread_set, thread_set, "--", color="grey", alpha=0.6,
             label="Speedup ideal")

    ax1.set_xlabel("Hilos OpenMP")
    ax1.set_ylabel("Tiempo de ejecucion [s]")
    ax1.set_title("Tiempo vs hilos")
    ax1.grid(alpha=0.3)
    ax1.legend()

    ax2.set_xlabel("Hilos OpenMP")
    ax2.set_ylabel("Speedup (T_serial / T_omp)")
    ax2.set_title("Speedup vs hilos")
    ax2.grid(alpha=0.3)
    ax2.legend()

    plt.tight_layout()
    out = PLOTS / "scaling.png"
    plt.savefig(out, dpi=130)
    plt.close()
    print(f"[ok] {out}")

    fig, ax = plt.subplots(figsize=(7, 4.5))
    for sch in schedules:
        eff = []
        threads = []
        for t in thread_set:
            key = (sch, str(t))
            if key not in agg or t_serial is None:
                continue
            mean = agg[key][0]
            eff.append((t_serial / mean) / t * 100.0)
            threads.append(t)
        ax.plot(threads, eff, "o-", label=sch, color=colors.get(sch))
    ax.axhline(100, color="grey", linestyle="--", alpha=0.6,
               label="Eficiencia ideal")
    ax.set_xlabel("Hilos OpenMP")
    ax.set_ylabel("Eficiencia paralela [%]")
    ax.set_title("Eficiencia vs hilos")
    ax.set_ylim(0, 110)
    ax.grid(alpha=0.3)
    ax.legend()
    plt.tight_layout()
    out = PLOTS / "efficiency.png"
    plt.savefig(out, dpi=130)
    plt.close()
    print(f"[ok] {out}")


def plot_chunk_sweep() -> None:
    rows = read_csv(RES / "chunk_sweep.csv")
    if not rows:
        print("[skip] chunk_sweep.csv vacio")
        return
    omp = [r for r in rows if r["version"] == "omp"]
    agg = aggregate(omp, ("schedule", "chunk"), "time_s")

    schedules = sorted({r["schedule"] for r in omp})
    chunks = sorted({int(r["chunk"]) for r in omp})

    fig, ax = plt.subplots(figsize=(7.5, 4.7))
    colors = {"static": "#1f77b4", "dynamic": "#d62728", "guided": "#2ca02c"}
    for sch in schedules:
        xs, ys = [], []
        for c in chunks:
            key = (sch, str(c))
            if key in agg:
                xs.append(c)
                ys.append(agg[key][0])
        ax.plot(xs, ys, "o-", label=sch, color=colors.get(sch))
    ax.set_xscale("log", base=2)
    ax.set_xlabel("Chunk size (log2)")
    ax.set_ylabel("Tiempo [s]")
    ax.set_title("Efecto del chunk size (8 hilos)")
    ax.grid(alpha=0.3, which="both")
    ax.legend()
    plt.tight_layout()
    out = PLOTS / "chunk_sweep.png"
    plt.savefig(out, dpi=130)
    plt.close()
    print(f"[ok] {out}")


def plot_antipatterns() -> None:
    rows = read_csv(RES / "antipatterns.csv")
    if not rows:
        print("[skip] antipatterns.csv vacio")
        return
    bucket = defaultdict(list)
    for r in rows:
        bucket[r["version"]].append(float(r["time_s"]))
    means = {k: sum(v) / len(v) for k, v in bucket.items()}

    label_order = [
        ("serial", "serial"),
        ("omp", "omp\n(reduction)"),
        ("omp_bad_falseshare", "false-share\n(array sin pad)"),
        ("omp_bad_atomic", "atomic"),
        ("omp_bad_critical", "critical"),
    ]
    labels, vals = [], []
    for key, lbl in label_order:
        if key in means:
            labels.append(lbl)
            vals.append(means[key])

    if not vals:
        print("[skip] sin datos en antipatterns")
        return

    fig, ax = plt.subplots(figsize=(8.5, 4.7))
    colors = ["#1f77b4", "#2ca02c", "#ff7f0e", "#d62728", "#9467bd"]
    bars = ax.bar(labels, vals, color=colors[: len(vals)])
    ax.set_ylabel("Tiempo [s] (menor es mejor)")
    ax.set_title("Anti-patrones de OpenMP en el autómata de tráfico")
    for b, v in zip(bars, vals):
        ax.text(b.get_x() + b.get_width() / 2, v, f"{v:.2f}s",
                ha="center", va="bottom", fontsize=9)
    ax.grid(axis="y", alpha=0.3)
    plt.tight_layout()
    out = PLOTS / "antipatterns.png"
    plt.savefig(out, dpi=130)
    plt.close()
    print(f"[ok] {out}")


def main() -> int:
    plot_velocity_density()
    plot_scaling()
    plot_chunk_sweep()
    plot_antipatterns()
    return 0


if __name__ == "__main__":
    sys.exit(main())
