#!/usr/bin/env python3
"""Reproduce loader-boundary PNG timing and sample comparisons (NumPy)."""

import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import platform
import struct
import subprocess
import tempfile
import zlib

import numpy as np


ROOT = Path(__file__).resolve().parents[1]
FORMATS = {3: "RGB888", 17: "RGBA8888", 32: "RGBFLOAT32", 33: "LINEARRGBFLOAT32",
           128: "PAL1", 129: "PAL2", 130: "PAL4", 131: "PAL8"}


def run(command, **kwargs):
    return subprocess.check_output(command, **kwargs).decode().strip()


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def chunk(kind, payload):
    return (struct.pack(">I", len(payload)) + kind + payload
            + struct.pack(">I", zlib.crc32(kind + payload)))


def png(path, samples, depth=8, color=2, paeth=True, metadata=()):
    """Write controlled standard PNGs, without incidental writer metadata."""
    height, width = samples.shape[:2]
    data = samples.astype(">u2" if depth == 16 else "u1").tobytes()
    rows = np.frombuffer(data, dtype=np.uint8).reshape(height, -1).astype(int)
    bpp = (samples.shape[2] if samples.ndim == 3 else 1) * (depth // 8)
    filtered = bytearray()
    for y, row in enumerate(rows):
        if paeth:
            a = np.zeros_like(row)
            a[bpp:] = row[:-bpp]
            b = rows[y - 1] if y else np.zeros_like(row)
            c = np.zeros_like(row)
            c[bpp:] = b[:-bpp]
            p = a + b - c
            pa, pb, pc = abs(p - a), abs(p - b), abs(p - c)
            predictor = np.where((pa <= pb) & (pa <= pc), a,
                                 np.where(pb <= pc, b, c))
            filtered.extend(b"\x04" + ((row - predictor) % 256).astype("u1").tobytes())
        else:
            filtered.extend(b"\x00" + row.astype("u1").tobytes())
    path.write_bytes(b"\x89PNG\r\n\x1a\n"
                     + chunk(b"IHDR", struct.pack(">IIBBBBB", width, height,
                                                   depth, color, 0, 0, 0))
                     + b"".join(chunk(k, v) for k, v in metadata)
                     + chunk(b"IDAT", zlib.compress(filtered, 6))
                     + chunk(b"IEND", b""))


def linear(values):
    return np.where(values <= 0.04045, values / 12.92,
                    ((values + 0.055) / 1.055) ** 2.4)


def fixtures(directory):
    raw = subprocess.check_output(["magick", str(ROOT / "images/snake.png"),
                                   "-resize", "512x512!", "-alpha", "off",
                                   "-depth", "8", "rgb:-"])
    photo = np.frombuffer(raw, dtype=np.uint8).reshape(512, 512, 3)
    y, x = np.mgrid[:512, :512]
    high = np.stack(((x * 127 + y * 3) % 65536,
                     (x * 11 + y * 127) % 65536,
                     (x * 73 + y * 31) % 65536), axis=2).astype(np.uint16)
    alpha8 = np.full((512, 512, 1), 128, dtype=np.uint8)
    alpha16 = np.full((512, 512, 1), 32768, dtype=np.uint16)
    cases = [
        ("rgb8-none", "RGB8 / None filter", photo, 8, 2, False, (), False, "none"),
        ("rgb8-paeth", "RGB8 / Paeth filter", photo, 8, 2, True, (), False, "none"),
        ("rgb16-paeth", "RGB16 / Paeth filter", high, 16, 2, True, (), False, "none"),
        ("rgba8", "RGBA8 / explicit background", np.concatenate((photo, alpha8), 2),
         8, 6, True, (), True, "none"),
        ("rgba16", "RGBA16 / explicit background", np.concatenate((high, alpha16), 2),
         16, 6, True, (), True, "none"),
        ("gama16", "RGB16 / gAMA=1 / CMS", high, 16, 2, True,
         ((b"gAMA", struct.pack(">I", 100000)),), False, "builtin"),
    ]
    result = []
    for key, label, pixels, depth, color, paeth, metadata, bg, cms in cases:
        path = directory / (key + ".png")
        png(path, pixels, depth, color, paeth, metadata)
        source = pixels[:, :, :3].astype(float) / ((1 << depth) - 1)
        expected = source if cms != "none" else linear(source)
        if bg:
            alpha = pixels[:, :, 3:4].astype(float) / ((1 << depth) - 1)
            expected = expected * alpha + linear(np.array(128 / 255)) * (1 - alpha)
        result.append((key, label, path, bg, cms, expected))
    return result


def measure(args):
    import matplotlib

    env = {k: v for k, v in os.environ.items()
           if not k.startswith(("SIXEL_", "LSQA_"))}
    env.update(LC_ALL="C", SIXEL_THREADS="1",
               DYLD_LIBRARY_PATH=str(args.build / "src/.libs"),
               LD_LIBRARY_PATH=str(args.build / "src/.libs"))
    records = []
    with tempfile.TemporaryDirectory(prefix="png-loader-inputs-") as temp:
        directory = Path(temp)
        for key, label, path, bg, cms, expected in fixtures(directory):
            record = {"id": key, "label": label, "input_sha256": digest(path),
                      "input_bytes": path.stat().st_size, "backends": {}}
            arrays = {}
            for batch in range(args.batches):
                for backend in (("builtin", "libpng") if batch % 2 == 0
                                else ("libpng", "builtin")):
                    order = (f"{backend}:cms_engine={cms}:cms_target=gamma:"
                             "prefer_8bit=0:background_colorspace=gamma!")
                    dump = directory / (backend + ".f32")
                    command = [str(args.probe), order, str(path), str(args.repeats),
                               str(dump), str(int(bg)), "0"]
                    obs = json.loads(run(command, env=env))
                    if obs["width"] != 512 or obs["height"] != 512:
                        raise RuntimeError("Unexpected decoded dimensions")
                    values = np.fromfile(dump, dtype=np.float32).reshape(512, 512, 4)
                    if not np.isfinite(values).all() or not (values[:, :, 3] == 1).all():
                        raise RuntimeError("Non-finite samples or unexpected mask")
                    native = values[:, :, :3].astype(float)
                    canonical = native if obs["format"] == 33 else linear(native)
                    if batch == 0:
                        arrays[backend] = (native, canonical)
                        record["backends"][backend] = {
                            "order": order, "format": FORMATS[obs["format"]],
                            "samples_ms": [],
                            "native_sha256": digest(dump),
                            "max_linear_reference_error": float(abs(canonical - expected).max()),
                            "linear_reference_rmse": float(np.sqrt(np.mean((canonical - expected) ** 2))),
                        }
                    elif not np.array_equal(native, arrays[backend][0]):
                        raise RuntimeError("Nondeterministic loader output")
                    record["backends"][backend]["samples_ms"].append(obs["ms"])
            diff = arrays["builtin"][1] - arrays["libpng"][1]
            record["native_values_equal"] = bool(np.array_equal(
                arrays["builtin"][0], arrays["libpng"][0]))
            record["max_linear_backend_difference"] = float(abs(diff).max())
            for backend, result in record["backends"].items():
                result["median_ms"] = float(np.median(result["samples_ms"]))
                result["q1_ms"], result["q3_ms"] = map(float, np.quantile(result["samples_ms"], [.25, .75]))
            records.append(record)
            print(key, {b: round(v["median_ms"], 3) for b, v in record["backends"].items()},
                  "difference", record["max_linear_backend_difference"], flush=True)

        # Small analytic cases distinguish policy selection from precision.
        policy = []
        source = np.array([[[240, 32, 16, 128]]], dtype=np.uint8)
        policy_path = directory / "background.png"
        png(policy_path, source, color=6,
            metadata=((b"bKGD", struct.pack(">HHH", 16, 128, 240)),))
        for backend in ("builtin", "libpng"):
            for priority in ("file_first", "explicit_first"):
                for space in ("gamma", "linear"):
                    order = (f"{backend}:cms_engine=none:background_policy={priority}:"
                             f"background_colorspace={space}!")
                    dump = directory / "policy.f32"
                    obs = json.loads(run([str(args.probe), order, str(policy_path),
                                          "1", str(dump), "1", "0"], env=env))
                    values = np.fromfile(dump, dtype=np.float32)[:3].astype(float)
                    if obs["format"] != 33:
                        values = linear(values)
                    policy.append({"order": order, "format": FORMATS[obs["format"]],
                                   "linear_rgb": values.tolist()})
        precision = []
        depth_path = directory / "key16.png"
        png(depth_path, np.array([[[32768, 32769, 32770], [1, 2, 3]]]),
            depth=16, metadata=((b"tRNS", struct.pack(">HHH", 1, 2, 3)),))
        indexed_path = directory / "indexed.png"
        png(indexed_path, np.array([[0, 1, 2]], dtype=np.uint8), color=3,
            metadata=((b"PLTE", bytes([240, 32, 16, 16, 240, 32, 32, 16, 240])),
                      (b"tRNS", bytes([128, 128, 128]))))
        for name, path, bg, palette, options in [
            ("16-bit tRNS / key on", depth_path, 0, 0, "trns_keycolor=1"),
            ("16-bit tRNS / key off", depth_path, 0, 0, "trns_keycolor=0"),
            ("indexed alpha / gamma", indexed_path, 1, 1, "background_colorspace=gamma"),
            ("indexed alpha / linear", indexed_path, 1, 1, "background_colorspace=linear"),
        ]:
            for backend in ("builtin", "libpng"):
                order = f"{backend}:cms_engine=none:{options}!"
                dump = directory / "precision.f32"
                obs = json.loads(run([str(args.probe), order, str(path), "1",
                                      str(dump), str(bg), str(palette)], env=env))
                values = np.fromfile(dump, dtype=np.float32).reshape(-1, 4).astype(float)
                precision.append({"case": name, "order": order,
                                  "format": FORMATS[obs["format"]],
                                  "native_rgb_and_coverage": values.tolist()})
    library = args.build / "src/.libs/libsixel.1.dylib"
    if not library.exists():
        library = args.build / "src/.libs/libsixel.so"
    return {"revision": args.revision, "measured_at_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
            "platform": platform.platform(),
            "machine": platform.machine(),
            "processor": run(["sysctl", "-n", "machdep.cpu.brand_string"])
            if platform.system() == "Darwin" else platform.processor(),
            "compiler": run(["cc", "--version"]),
            "libpng_version": run(["pkg-config", "--modversion", "libpng"]),
            "magick_version": run(["magick", "-version"]),
            "zlib_version": zlib.ZLIB_RUNTIME_VERSION,
            "numpy_version": np.__version__, "matplotlib_version": matplotlib.__version__,
            "source_sha256": digest(ROOT / "images/snake.png"),
            "probe_sha256": digest(args.probe), "library_sha256": digest(library),
            "config_sha256": digest(args.build / "config.h"),
            "configure_arguments": run([str(args.build / "config.status"), "--config"]),
            "batches": args.batches, "repeats_per_batch": args.repeats,
            "warmups_per_batch": 2, "dimensions": [512, 512],
            "scope": "warm file read + dispatch + decode + normalization + frame callback + release; no encoder",
            "palette_fusion": False, "records": records, "background_probe": policy,
            "precision_probes": precision}


def plot(data, output):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    plt.rcParams.update({"font.family": "DejaVu Sans", "font.size": 10,
                         "svg.hashsalt": "libsixel-png-loaders"})
    fig, axes = plt.subplots(2, 1, figsize=(9.2, 7.4), layout="constrained")
    colors = {"builtin": "#087f8c", "libpng": "#a94819"}
    for ax, group, title in zip(axes, (data["records"][:3], data["records"][3:]),
                               ("Opaque samples / CMS off", "Alpha composition and color conversion")):
        for i, record in enumerate(group):
            for backend, offset in (("builtin", -.18), ("libpng", .18)):
                value = record["backends"][backend]
                median = value["median_ms"]
                ax.barh(i + offset, median, height=.30, color=colors[backend],
                        label=backend if i == 0 else None)
                ax.errorbar(median, i + offset,
                            xerr=[[median - value["q1_ms"]], [value["q3_ms"] - median]],
                            color="#222222", capsize=3, linewidth=1)
                ax.annotate(f"{median:.2f}", (value["q3_ms"], i + offset),
                            xytext=(5, 0), textcoords="offset points", va="center", fontsize=9)
        ax.set_yticks(range(len(group)), [r["label"] for r in group])
        ax.invert_yaxis()
        ax.set_xlim(0, max(v["q3_ms"] for r in group for v in r["backends"].values()) * 1.2)
        ax.set_xlabel("Milliseconds per complete loader call (lower is faster)")
        ax.set_title(title, loc="left", fontweight="bold")
        ax.spines[["top", "right"]].set_visible(False)
        ax.xaxis.grid(True, alpha=.18)
        ax.set_axisbelow(True)
        ax.legend(loc="lower right" if title.startswith("Opaque")
                  else "upper right", frameon=False)
    fig.suptitle("PNG loader cost includes adapter overhead", fontsize=16, fontweight="bold")
    fig.supxlabel(f"512 × 512 · {data['batches']} alternating paired batches × {data['repeats_per_batch']} calls · median and IQR\n"
                  f"{data['machine']} · libpng {data['libpng_version']} · -O3 · one calling thread · panels use different scales\n"
                  "Recorded libpng adapter prepares APNG even for static PNG. These are not codec-only timings.",
                  fontsize=9)
    fig.savefig(output, metadata={"Date": None})
    plt.close(fig)
    # Matplotlib emits trailing spaces in SVG path data; keep diffs clean.
    output.write_text("\n".join(line.rstrip() for line in output.read_text().splitlines()) + "\n")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build", type=Path)
    parser.add_argument("--probe", type=Path)
    parser.add_argument("--revision")
    parser.add_argument("--batches", type=int, default=9)
    parser.add_argument("--repeats", type=int, default=8)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--plot-only", action="store_true")
    parser.add_argument("--check", action="store_true",
                        help="validate saved statistics and deterministic SVG regeneration")
    args = parser.parse_args()
    if args.plot_only or args.check:
        data = json.loads(args.output.read_text())
    else:
        if not args.build or not args.probe or not args.revision:
            parser.error("measurement requires --build, --probe and --revision")
        if args.batches < 3 or args.repeats < 1:
            parser.error("require at least three batches and one repeat")
        data = measure(args)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        args.output.write_text(json.dumps(data, indent=2) + "\n")
    if args.check:
        for record in data["records"]:
            for value in record["backends"].values():
                samples = np.asarray(value["samples_ms"])
                if (len(samples) != data["batches"] or not np.isfinite(samples).all()
                        or (samples <= 0).any()):
                    raise RuntimeError("Invalid timing samples")
                if value["median_ms"] != float(np.median(samples)):
                    raise RuntimeError("Stale median")
                if [value["q1_ms"], value["q3_ms"]] != list(np.quantile(samples, [.25, .75])):
                    raise RuntimeError("Stale quartiles")
        with tempfile.TemporaryDirectory(prefix="png-loader-figure-check-") as temp:
            rendered = Path(temp) / "figure.svg"
            plot(data, rendered)
            if rendered.read_bytes() != args.output.with_suffix(".svg").read_bytes():
                raise RuntimeError("Stale figure or different Matplotlib/font environment")
        print("PNG timing statistics and figure regeneration: OK")
    else:
        plot(data, args.output.with_suffix(".svg"))


if __name__ == "__main__":
    main()
