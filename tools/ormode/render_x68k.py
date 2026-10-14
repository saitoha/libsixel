"""Plot the NetBSD/x68k console work-count model from saved results."""

import json

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
import numpy as np
from measure import ROOT

rows = json.loads((ROOT / "x68k" / "results.json").read_text())
photos = [r for r in rows if r["image"].startswith("picsum-")]
assert len(photos) == 200, len(photos)


def series(colors, fn):
    return [fn(r) for r in photos if r["colors"] == colors]


panels = [
    (
        "Stream bytes: OR / normal fast\n(lower is smaller)",
        lambda r: r["or"]["bytes"] / r["fast"]["bytes"],
        1.0,
    ),
    (
        "Receiver column updates: normal fast / OR\n(higher means less OR work)",
        lambda r: r["fast"]["column_ops"] / r["or"]["column_ops"],
        1.0,
    ),
    (
        "Sender scan steps: normal / OR\n(higher means less OR work)",
        lambda r: (
            r["sender"]["normal_pixel_examinations"]
            / (r["sender"]["or_pixel_transposes"] + r["sender"]["or_plane_run_steps"])
        ),
        1.0,
    ),
    (
        "Wrong console pixels with normal -Esize\n(percent of the image)",
        lambda r: 100 * r["size"]["ite_mismatched_pixels"] / r["pixels"],
        0.0,
    ),
]

fig, axes = plt.subplots(1, 4, figsize=(15, 4.4), layout="constrained")
rng = np.random.default_rng(0)
for ax, (title, fn, ref) in zip(axes, panels, strict=True):
    data = [series(8, fn), series(16, fn)]
    ax.boxplot(
        data,
        tick_labels=["8 colors\n3 planes", "16 colors\n4 planes"],
        widths=0.5,
        showfliers=False,
    )
    for i, values in enumerate(data, start=1):
        ax.scatter(
            i + rng.uniform(-0.15, 0.15, len(values)),
            values,
            s=6,
            alpha=0.45,
            color="tab:blue",
            lw=0,
        )
    ax.axhline(ref, color="0.4", ls="--", lw=1)
    ax.set_title(title, fontsize=10.5)
    ax.grid(axis="y", alpha=0.3)
fig.suptitle(
    "100 photos at 768 pixels wide, replayed through the NetBSD/x68k ite rules",
    fontsize=12,
)
out = ROOT / "x68k" / "x68k-model.png"
fig.savefig(out, dpi=110)
print(out)
