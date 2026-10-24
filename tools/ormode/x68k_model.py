"""Count NetBSD/x68k console work for normal and OR SIXEL streams.

The receiver half replays each stream through ite-model, a host-side copy of
the ite parser rules.  The sender half applies sayaka's two serializers'
scan structure to the intended index raster.  Both report operation counts,
not host time: the 25 MHz MC68EC030 target cannot be timed on this host.
"""

import json
import platform
import re
import subprocess

import numpy as np
from measure import OUT, ROOT, SRC, digest, run

STUDY = ROOT / "x68k"
STUDY.mkdir(exist_ok=True)
MODEL = ROOT / "ite-model"
WIDTH = 768  # MAX_SIXEL_WIDTH in sys/arch/x68k/dev/itevar.h
CASES = [(8, 7), (16, 15)]  # palette budget, ite register mask
VARIANTS = {
    "fast": ["-Efast"],
    "size": ["-Esize"],
    "or": ["-Esize", "-O"],
}


def encode(path, colors, extra):
    args = [
        SRC / "converters/.libs/img2sixel",
        "--threading-policy=1",
        "--precision-policy=8bit",
        "--loaders=builtin!",
        "--gpu-policy=off",
        "--palette-type=rgb",
        f"-p{colors}",
        f"-w{WIDTH}",
        *extra,
        "-o",
        "-",
        path,
    ]
    return run(args).stdout, list(map(str, args))


def geometry(stream):
    m = re.search(rb'"1;1;(\d+);(\d+)', stream)
    return int(m.group(1)), int(m.group(2))


def replay(stream, mask, width, height, stem):
    ite = STUDY / f"{stem}.ite.raw"
    dec = STUDY / f"{stem}.dec.raw"
    p = subprocess.run(
        [str(MODEL), str(mask), str(width), str(height), str(ite), str(dec)],
        input=stream,
        capture_output=True,
        check=True,
    )
    counts = json.loads(p.stdout)
    shape = (height, width)
    return (
        counts,
        np.fromfile(ite, np.uint8).reshape(shape),
        np.fromfile(dec, np.uint8).reshape(shape),
    )


def sender_work(raster, palette_count):
    """Pixel examinations in sayaka's normal and OR serializers."""
    planes = max(1, int(palette_count - 1).bit_length())
    normal = 0
    passes = 0
    for top in range(0, raster.shape[0], 6):
        band = raster[top : top + 6]
        rows, width = band.shape
        normal += rows * width  # min/max column scan per color
        present = np.unique(band)
        passes += len(present)
        for color in present:
            xs = np.nonzero((band == color).any(axis=0))[0]
            normal += (int(xs[-1]) - int(xs[0]) + 1) * rows
    transpose = raster.size  # one multiply-and-mask per pixel
    runs = planes * raster.shape[1] * -(-raster.shape[0] // 6)
    return {
        "planes": planes,
        "normal_pixel_examinations": normal,
        "normal_color_passes": passes,
        "or_pixel_transposes": transpose,
        "or_plane_run_steps": runs,
        "bands": -(-raster.shape[0] // 6),
    }


def main():
    photos = json.loads((ROOT / "inputs.json").read_text())
    controls = json.loads((ROOT / "controls.json").read_text())
    items = [{"name": f"picsum-{p['id']}", "path": p["path"]} for p in photos]
    items += [c for c in controls if c["name"] in ("snake", "egret")]
    rows = []
    for item in items:
        for colors, mask in CASES:
            stem = f"{item['name']}-p{colors}"
            row = {"image": item["name"], "colors": colors, "mask": mask}
            reference = None
            for name, extra in VARIANTS.items():
                stream, command = encode(item["path"], colors, extra)
                width, height = geometry(stream)
                counts, ite, dec = replay(stream, mask, width, height, f"{stem}.{name}")
                if name == "fast":
                    # fast paints every pixel once, so replacement and OR
                    # accumulation agree; this is the intended raster.
                    reference = dec
                    assert np.array_equal(ite, dec)
                    row["palette_entries"] = counts["palette_defs"]
                row[name] = {
                    "bytes": len(stream),
                    "stream_sha256": digest(stream),
                    "command": command,
                    "ite_mismatched_pixels": int(np.count_nonzero(ite != reference)),
                    **counts,
                }
            row["pixels"] = int(reference.size)
            row["sender"] = sender_work(reference, row["palette_entries"])
            rows.append(row)
            print(stem, row["fast"]["bytes"], row["or"]["bytes"], flush=True)
    meta = {
        "revision": run(["git", "-C", SRC, "rev-parse", "HEAD"])
        .stdout.decode()
        .strip(),
        "platform": platform.platform(),
        "width": WIDTH,
        "cases": [{"colors": c, "ite_mask": m} for c, m in CASES],
        "variants": VARIANTS,
        "receiver_model": "tools/ormode/ite-model.c",
        "receiver_sources": {
            "ite.c": "NetBSD sys/arch/x68k/dev/ite.c 1.73",
            "ite_tv.c": "NetBSD sys/arch/x68k/dev/ite_tv.c 1.21",
            "itevar.h": "NetBSD sys/arch/x68k/dev/itevar.h 1.18",
        },
        "binary_sha256": {
            "img2sixel": digest((SRC / "converters/.libs/img2sixel").read_bytes()),
            "libsixel": digest((SRC / "src/.libs/libsixel.1.dylib").read_bytes()),
            "ite-model": digest(MODEL.read_bytes()),
        },
    }
    (STUDY / "metadata.json").write_text(json.dumps(meta, indent=2) + "\n")
    (STUDY / "results.json").write_text(json.dumps(rows, indent=1) + "\n")
    (OUT / "x68k-model.json").write_text(json.dumps(rows, indent=1) + "\n")


if __name__ == "__main__":
    main()
