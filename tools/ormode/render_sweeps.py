"""Plot color/thread axes from saved OR-mode measurements."""

import csv
import json
import re
import statistics

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from measure import ROOT

OUT = ROOT / "sweeps"
COLORS = [2, 4, 8, 16, 32, 64, 128, 256]
THREADS = list(range(1, 13))


def label(name):
    return name.replace("-600x450", "")


def ratio_plot(rows, names, axis, title, filename):
    fig, axes = plt.subplots(3, 2, figsize=(13, 11), layout="constrained")
    panels = [
        ("size_ratio", "Stream size: OR / normal (lower is smaller)"),
        ("encode_cli_speedup", "Encoder CLI: normal / OR time (higher is faster)"),
        ("decode_rgba_speedup", "RGBA decoder API: normal / OR time"),
        ("decode_indexed_speedup", "Indexed decoder API: normal / OR time"),
        ("decode_cli_speedup", "Decoder CLI with RGBA PNG: normal / OR time"),
        ("msssim", "Source MS-SSIM (normal and OR coincide)"),
    ]
    palette = plt.get_cmap("tab10")
    for index, name in enumerate(names):
        series = sorted([r for r in rows if r["image"] == name], key=lambda r: r[axis])
        for ax, (metric, description) in zip(axes.flat, panels, strict=True):
            ax.plot(
                [r[axis] for r in series],
                [r[metric] for r in series],
                marker="o",
                ms=3,
                lw=1.4,
                color=palette(index),
                label=label(name),
            )
            ax.set_title(description, fontsize=11)
            ax.set_xlabel(
                "Requested palette budget"
                if axis == "colors"
                else "Configured thread budget"
            )
            if axis == "colors":
                ax.set_xscale("log", base=2)
                ax.set_xticks(COLORS, labels=list(map(str, COLORS)))
            else:
                ax.set_xticks(THREADS)
    for ax, (metric, _description) in zip(axes.flat, panels, strict=True):
        if metric != "msssim":
            ax.axhline(1, color="black", ls="--", lw=0.8)
        ax.grid(alpha=0.18)
    handles, labels = axes[0, 0].get_legend_handles_labels()
    fig.legend(handles, labels, loc="outside lower center", ncol=4, frameon=False)
    fig.suptitle(title, fontsize=15)
    fig.savefig(OUT / filename, dpi=150)
    plt.close(fig)


def summarize(rows):
    result = {"count": len(rows)}
    for fmt in ["rgba", "indexed", "cli"]:
        values = [r[fmt + "_speedup"] for r in rows]
        result[fmt] = {
            "median": statistics.median(values),
            "min": min(values),
            "max": max(values),
            "or_faster": sum(v > 1 for v in values),
            "normal_faster": sum(v < 1 for v in values),
        }
    return result


def main():
    enc = json.loads((OUT / "encode.json").read_text())
    dec = json.loads((OUT / "decode.json").read_text())
    assert len(enc) == 164 and len(dec) == 264
    for record in dec:
        folder = (
            ROOT / "results" if record["name"].startswith("picsum-") else OUT / "encode"
        )
        baseline = json.loads((folder / f"{record['name']}.json").read_text())
        assert record["rgba_sha256"] == [baseline["modes"][0]["rgba_sha256"]] * 2
        assert record["stream_sha256"] == [
            m["stream_sha256"] for m in baseline["modes"]
        ]
        for metric, count in [("rgba", 21), ("indexed", 21), ("cli", 11)]:
            assert all(
                len(t["samples"]) == count and min(t["samples"]) > 0
                for t in record[metric + "_seconds"]
            )
    inputs = json.loads((OUT / "inputs.json").read_text())
    names = [i["name"] for i in inputs]
    lookup = {(r["name"], r["threads"]): r for r in dec}
    rows = []
    for e in enc:
        image = e["input"]["name"]
        d = lookup[(f"{image}-p{e['colors']}-t1", e["threads"])]
        assert e["changed_pixels"] == 0 and all(e["repeat_stream_equal"])
        baseline = json.loads(
            (OUT / "encode" / f"{image}-p{e['colors']}-t1.json").read_text()
        )
        assert d["rgba_sha256"] == [baseline["modes"][0]["rgba_sha256"]] * 2
        rows.append(
            {
                "image": image,
                "colors": e["colors"],
                "threads": e["threads"],
                "defined_colors": len(
                    set(
                        re.findall(
                            rb"#([0-9]+);[12];",
                            (OUT / "encode" / f"{e['name']}.0.six").read_bytes(),
                        )
                    )
                ),
                "normal_bytes": e["modes"][0]["bytes"],
                "or_bytes": e["modes"][1]["bytes"],
                "size_ratio": e["size_ratio"],
                "encode_normal_ms": e["cli_seconds"][0]["median"] * 1000,
                "encode_or_ms": e["cli_seconds"][1]["median"] * 1000,
                "decode_rgba_normal_ms": d["rgba_seconds"][0]["median"] * 1000,
                "decode_rgba_or_ms": d["rgba_seconds"][1]["median"] * 1000,
                "decode_indexed_normal_ms": d["indexed_seconds"][0]["median"] * 1000,
                "decode_indexed_or_ms": d["indexed_seconds"][1]["median"] * 1000,
                "decode_cli_normal_ms": d["cli_seconds"][0]["median"] * 1000,
                "decode_cli_or_ms": d["cli_seconds"][1]["median"] * 1000,
                "encode_cli_speedup": e["cli_speedup"],
                "decode_rgba_speedup": d["rgba_speedup"],
                "decode_indexed_speedup": d["indexed_speedup"],
                "decode_cli_speedup": d["cli_speedup"],
                "msssim": e["normal_quality"]["MS-SSIM"],
                "deltae": e["normal_quality"]["Δ E00_mean"],
            }
        )
    with (OUT / "axes.csv").open("w") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0]), lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
    plt.rcParams.update(
        {"font.size": 10, "axes.spines.top": False, "axes.spines.right": False}
    )
    ratio_plot(
        [r for r in rows if r["threads"] == 1 and r["image"] != "snake-fullhd"],
        names[:-1],
        "colors",
        "OR mode across palette budgets\nEight fixed images; one encoder and decoder thread; ordinary -E size baseline",
        "color-curves.png",
    )
    ratio_plot(
        [r for r in rows if r["colors"] == 256],
        names,
        "threads",
        "OR mode across thread budgets (256 colors)\nEncoder curves vary encoding budget; decoder curves reuse fixed one-thread streams",
        "thread-curves.png",
    )
    fig, axes = plt.subplots(2, 2, figsize=(12, 8), layout="constrained")
    full_enc = sorted(
        [r for r in enc if r["input"]["name"] == "snake-fullhd"],
        key=lambda r: r["threads"],
    )
    full_dec = sorted(
        [r for r in dec if r["name"] == "snake-fullhd-p256-t1"],
        key=lambda r: r["threads"],
    )
    panels = [
        (full_enc, "cli_seconds", "Encode CLI, including quantization"),
        (full_dec, "rgba_seconds", "Decode API to RGBA"),
        (full_dec, "indexed_seconds", "Decode API to palette indices"),
        (full_dec, "cli_seconds", "Decode CLI, including RGBA PNG output"),
    ]
    for ax, (series, key, title) in zip(axes.flat, panels, strict=True):
        for mode, color, name in [
            (0, "#247ba0", "Normal -E size"),
            (1, "#b3482d", "OR mode"),
        ]:
            ax.plot(
                THREADS,
                [r[key][mode]["median"] * 1000 for r in series],
                "-o",
                ms=4,
                color=color,
                label=name,
            )
            ax.fill_between(
                THREADS,
                [r[key][mode]["q1"] * 1000 for r in series],
                [r[key][mode]["q3"] * 1000 for r in series],
                color=color,
                alpha=0.18,
            )
        ax.set(title=title, xlabel="Configured threads", ylabel="Elapsed time (ms)")
        ax.set_xticks(THREADS)
        ax.grid(alpha=0.2)
        ax.legend(frameon=False)
    fig.suptitle(
        "1920×1080 snake, 256 colors\nPaired medians and interquartile ranges; resize performed before measurement",
        fontsize=14,
    )
    fig.savefig(OUT / "fullhd-threads.png", dpi=150)
    plt.close(fig)
    photos = [r for r in dec if r["name"].startswith("picsum-")]
    summary = {
        "photos": summarize(photos),
        "fullhd": {
            str(r["threads"]): {
                fmt: {
                    "normal_ms": r[fmt + "_seconds"][0]["median"] * 1000,
                    "or_ms": r[fmt + "_seconds"][1]["median"] * 1000,
                    "speedup": r[fmt + "_speedup"],
                }
                for fmt in ["rgba", "indexed", "cli"]
            }
            for r in full_dec
        },
        "encode_pairs": len(enc),
        "decode_pairs": len(dec),
    }
    (OUT / "summary.json").write_text(json.dumps(summary, indent=2) + "\n")
    fig, ax = plt.subplots(1, 2, figsize=(11, 4.5), layout="constrained")
    ax[0].boxplot(
        [[r[f"{fmt}_speedup"] for r in photos] for fmt in ["rgba", "indexed", "cli"]],
        tick_labels=["RGBA API", "Indexed API", "RGBA PNG CLI"],
    )
    ax[0].axhline(1, color="black", ls="--", lw=0.8)
    ax[0].set(
        title="100-photo decoder comparison, one thread",
        ylabel="Normal / OR time (higher is faster)",
    )
    ax[1].scatter(
        [r["bytes"][1] / r["bytes"][0] for r in photos],
        [r["rgba_speedup"] for r in photos],
        s=20,
        alpha=0.65,
        label="RGBA API",
    )
    ax[1].scatter(
        [r["bytes"][1] / r["bytes"][0] for r in photos],
        [r["indexed_speedup"] for r in photos],
        s=20,
        alpha=0.65,
        label="Indexed API",
    )
    ax[1].axhline(1, color="black", ls="--", lw=0.8)
    ax[1].axvline(1, color="black", ls="--", lw=0.8)
    ax[1].set(
        xlabel="OR / normal stream bytes",
        ylabel="Normal / OR decode time",
        title="Byte reduction does not ensure faster decoding",
    )
    ax[1].legend(frameon=False)
    fig.savefig(OUT / "decode-photos.png", dpi=150)
    plt.close(fig)
    print(json.dumps(summary["photos"], indent=2))


if __name__ == "__main__":
    main()
