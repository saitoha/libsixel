"""Render the recorded study; this script never reruns benchmark commands."""

import csv
import json
import os
import pathlib
import statistics

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

ROOT = pathlib.Path(
    os.environ.get("OR_MODE_STUDY_DIR", pathlib.Path(__file__).parent)
).resolve()
OUT = ROOT / "results"


def summary(rows):
    r = [x for x in rows if x["input"]["name"].startswith("picsum-")]
    return {
        "count": len(r),
        "increased": sum(x["size_ratio"] > 1 for x in r),
        "decreased": sum(x["size_ratio"] < 1 for x in r),
        "aggregate_size_ratio": sum(x["modes"][1]["bytes"] for x in r)
        / sum(x["modes"][0]["bytes"] for x in r),
        "mean_size_ratio": statistics.mean(x["size_ratio"] for x in r),
        "median_size_ratio": statistics.median(x["size_ratio"] for x in r),
        "min_size_ratio": min(x["size_ratio"] for x in r),
        "max_size_ratio": max(x["size_ratio"] for x in r),
        "median_cli_speedup": statistics.median(x["cli_speedup"] for x in r),
        "min_cli_speedup": min(x["cli_speedup"] for x in r),
        "max_cli_speedup": max(x["cli_speedup"] for x in r),
        "median_indexed_speedup": statistics.median(x["indexed_speedup"] for x in r),
        "min_indexed_speedup": min(x["indexed_speedup"] for x in r),
        "max_indexed_speedup": max(x["indexed_speedup"] for x in r),
        "all_cli_pixels_equal": all(x["changed_pixels"] == 0 for x in rows),
        "all_indexed_pixels_equal": all(
            x.get("indexed_changed_pixels", 0) == 0 for x in rows
        ),
        "all_repeats_equal": all(all(x["repeat_stream_equal"]) for x in rows),
    }


def main():
    rows = json.loads((OUT / "results.json").read_text())
    s = summary(rows)
    (OUT / "summary.json").write_text(json.dumps(s, indent=2) + "\n")
    print(json.dumps(s, indent=2))
    cols = [
        "name",
        "colors",
        "threads",
        "normal_bytes",
        "or_bytes",
        "size_ratio",
        "normal_ms",
        "or_ms",
        "cli_speedup",
        "indexed_speedup",
        "changed_pixels",
        "alpha_changed_pixels",
        "normal_msssim",
        "or_msssim",
        "normal_deltae",
        "or_deltae",
    ]
    with (OUT / "summary.csv").open("w") as f:
        writer = csv.DictWriter(f, fieldnames=cols, lineterminator="\n")
        writer.writeheader()
        for r in rows:
            writer.writerow(
                {
                    "name": r["name"],
                    "colors": r["colors"],
                    "threads": r["threads"],
                    "normal_bytes": r["modes"][0]["bytes"],
                    "or_bytes": r["modes"][1]["bytes"],
                    "size_ratio": r["size_ratio"],
                    "normal_ms": r["cli_seconds"][0]["median"] * 1000,
                    "or_ms": r["cli_seconds"][1]["median"] * 1000,
                    "cli_speedup": r["cli_speedup"],
                    "indexed_speedup": r.get("indexed_speedup", ""),
                    "changed_pixels": r["changed_pixels"],
                    "alpha_changed_pixels": r["alpha_changed_pixels"],
                    "normal_msssim": r["normal_quality"]["MS-SSIM"],
                    "or_msssim": r["or_quality"]["MS-SSIM"],
                    "normal_deltae": r["normal_quality"]["Δ E00_mean"],
                    "or_deltae": r["or_quality"]["Δ E00_mean"],
                }
            )
    photos = [r for r in rows if r["input"]["name"].startswith("picsum-")]
    plt.rcParams.update(
        {"font.size": 11, "axes.spines.top": False, "axes.spines.right": False}
    )
    fig, ax = plt.subplots(2, 2, figsize=(13, 9), layout="constrained")
    ordered = sorted(r["size_ratio"] for r in photos)
    ax[0, 0].bar(
        range(1, len(ordered) + 1),
        [(x - 1) * 100 for x in ordered],
        color=["#b3482d" if x > 1 else "#247ba0" for x in ordered],
        width=1,
    )
    ax[0, 0].axhline(0, color="black", lw=0.8)
    ax[0, 0].set(
        xlabel="Photo rank (sorted by size change)",
        ylabel="OR bytes vs normal size policy (%)",
        title=f"Size: {s['decreased']} smaller / {s['increased']} larger",
    )
    values = [
        [r["cli_speedup"] for r in photos],
        [r["indexed_speedup"] for r in photos],
    ]
    ax[0, 1].boxplot(
        values, tick_labels=["Whole CLI", "Indexed serialization"], showfliers=True
    )
    ax[0, 1].axhline(1, color="black", lw=0.8, ls="--")
    ax[0, 1].set(
        ylabel="Normal time / OR time (higher is faster)",
        title="Per-photo speed ratios; distinct timing boundaries",
    )
    ax[1, 0].scatter(
        [(r["size_ratio"] - 1) * 100 for r in photos],
        [r["cli_speedup"] for r in photos],
        color="#247ba0",
        alpha=0.7,
        s=25,
    )
    ax[1, 0].axhline(1, color="black", lw=0.8, ls="--")
    ax[1, 0].axvline(0, color="black", lw=0.8, ls="--")
    ax[1, 0].set(
        xlabel="OR stream size change (%)",
        ylabel="Whole CLI speed ratio",
        title="Larger output can still encode faster",
    )
    controls = [
        r
        for r in rows
        if not r["input"]["name"].startswith("picsum-")
        and r["colors"] == 256
        and r["threads"] == 1
    ]
    ax[1, 1].barh(
        [r["input"]["name"].replace("-600x450", "") for r in controls],
        [(r["size_ratio"] - 1) * 100 for r in controls],
        color=["#b3482d" if r["size_ratio"] > 1 else "#247ba0" for r in controls],
    )
    ax[1, 1].axvline(0, color="black", lw=0.8)
    ax[1, 1].set(
        xlabel="OR stream size change (%)",
        title="Additional image classes (256 colors, one thread)",
    )
    fig.suptitle(
        "OR mode versus ordinary -E size\n100 fixed-ID photos, 800×600, 256 colors, one thread",
        fontsize=15,
    )
    fig.savefig(OUT / "comparison.png", dpi=150)
    plt.close(fig)


if __name__ == "__main__":
    main()
