#!/usr/bin/env python3
"""Plot paired jam and frozen-encoder traces with matching controller observations."""

import argparse
import csv
from pathlib import Path

import matplotlib
matplotlib.use("Agg")
import matplotlib.pyplot as plt


def read_trace(path):
    with path.open(newline="", encoding="utf-8") as handle:
        metadata = handle.readline().strip()
        if not metadata.startswith("# axialtrace,2,step_us=10000,quiet_limit="):
            raise ValueError(f"{path} requires a version 2 trace with a 10 ms step")
        reader = csv.DictReader(handle)
        fields = ["tick", "target", "encoder", "command", "position", "velocity", "stopped"]
        if reader.fieldnames != fields:
            raise ValueError(f"{path} has an unexpected CSV header")
        rows = [{name: int(row[name]) for name in fields} for row in reader]
    if not rows or [row["tick"] for row in rows] != list(range(len(rows))):
        raise ValueError(f"{path} requires contiguous ticks starting at zero")
    return metadata, rows


def plot(jam_path, frozen_path, output, fault_tick):
    jam_policy, jam = read_trace(jam_path)
    frozen_policy, frozen = read_trace(frozen_path)
    if jam_policy != frozen_policy or len(jam) != len(frozen):
        raise ValueError("the two traces must have matching policies and lengths")
    observations = ["tick", "target", "encoder", "command", "stopped"]
    if any(a[key] != b[key] for a, b in zip(jam, frozen) for key in observations):
        raise ValueError("controller observations differ between the two traces")
    if not 0 <= fault_tick < len(jam):
        raise ValueError("fault tick must lie inside the recording")
    differences = [a["tick"] for a, b in zip(jam, frozen) if a["position"] != b["position"]]
    stops = [row["tick"] for row in jam if row["stopped"]]
    if not differences or not stops:
        raise ValueError("the comparison requires divergent positions and a recorded stop")

    ticks = [row["tick"] for row in jam]
    fig, axes = plt.subplots(2, 1, figsize=(10, 6.5), sharex=True,
                             gridspec_kw={"height_ratios": [2.2, 1]}, layout="constrained")
    fig.suptitle("AxialTrace — jam and frozen encoder", fontsize=17, fontweight="bold")
    axes[0].plot(ticks, [row["position"] / 1000 for row in jam],
                 label="Jammed axis · physical position", color="#2166ac", linewidth=2.5)
    axes[0].plot(ticks, [row["position"] / 1000 for row in frozen],
                 label="Frozen encoder · physical position", color="#d77818", linewidth=2.5)
    axes[0].step(ticks, [row["encoder"] for row in jam], where="post",
                 label="Encoder reading · identical in both runs", color="#30343b",
                 linestyle="--", linewidth=1.8)
    axes[0].set_ylabel("Position (encoder counts)")
    axes[0].legend(loc="upper left", frameon=False, fontsize=9)
    axes[0].set_title("The controller receives the same readings while physical positions diverge.",
                      fontsize=10, loc="left", pad=12)
    axes[1].step(ticks, [row["command"] / 1000 for row in jam], where="post",
                 color="#66519c", linewidth=2.2)
    axes[1].set_ylabel("Shared command\n(counts / tick)")
    axes[1].set_xlabel("Simulation tick · 10 ms per tick")
    axes[1].set_ylim(-0.08, max(row["command"] / 1000 for row in jam) + 0.25)
    for ax in axes:
        ax.axvline(fault_tick, color="#b44c4c", linestyle=":", linewidth=1.5)
        ax.axvline(stops[0], color="#3f8065", linestyle=":", linewidth=1.5)
        ax.grid(axis="y", alpha=0.2)
        ax.spines[["top", "right"]].set_visible(False)
        ax.set_xlim(0, ticks[-1])
    axes[1].text(fault_tick + 0.35, 0.88, f"Fault at {fault_tick}",
                  transform=axes[1].get_xaxis_transform(), color="#934040", fontsize=9)
    axes[1].text(stops[0] + 0.35, 0.88, f"Stop at {stops[0]}",
                  transform=axes[1].get_xaxis_transform(), color="#32664f", fontsize=9)
    output.parent.mkdir(parents=True, exist_ok=True)
    fig.savefig(output, dpi=160, metadata={"Software": "AxialTrace"})
    plt.close(fig)
    print(f"observations_match=true first_physical_difference={differences[0]} "
          f"fault_tick={fault_tick} observed_stop={stops[0]}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("jam", type=Path)
    parser.add_argument("frozen", type=Path)
    parser.add_argument("--output", type=Path, default=Path("build/jam-vs-frozen.png"))
    parser.add_argument("--fault-tick", type=int, default=7)
    args = parser.parse_args()
    try:
        plot(args.jam, args.frozen, args.output, args.fault_tick)
    except (OSError, ValueError, KeyError, TypeError) as error:
        parser.exit(2, f"{error}\n")


if __name__ == "__main__":
    main()
