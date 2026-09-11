"""Paired OR-mode study: end-to-end CLI and identical-index serialization."""

import hashlib
import io
import json
import os
import pathlib
import platform
import statistics
import subprocess
import time

import numpy as np
from PIL import Image

ROOT = pathlib.Path(
    os.environ.get("OR_MODE_STUDY_DIR", pathlib.Path(__file__).parent)
).resolve()
SRC = ROOT / "source"
OUT = ROOT / "results"
OUT.mkdir(exist_ok=True)
ENV = {
    k: v
    for k, v in os.environ.items()
    if not k.startswith(("SIXEL_", "LSQA_", "DYLD_"))
}
ENV.update(
    LC_ALL="C", TZ="UTC", SIXEL_THREADS="1", DYLD_LIBRARY_PATH=str(SRC / "src/.libs")
)
IMG = SRC / "converters/.libs/img2sixel"
DEC = SRC / "converters/.libs/sixel2png"
LSQA = SRC / "assessment/.libs/lsqa"
RUNS = 11


def run(args, data=None):
    p = subprocess.run(
        [str(x) for x in args],
        input=data,
        capture_output=True,
        check=False,
        env=ENV,
    )
    if p.returncode:
        raise RuntimeError((args, p.returncode, p.stderr.decode(errors="replace")))
    return p


def digest(data):
    return hashlib.sha256(data).hexdigest()


def rgba(data):
    return np.array(Image.open(io.BytesIO(data)).convert("RGBA"))


def stats(values):
    return {
        "median": statistics.median(values),
        "q1": float(np.percentile(values, 25)),
        "q3": float(np.percentile(values, 75)),
        "samples": values,
    }


def command(path, colors, threads, mode, output="-"):
    args = [
        IMG,
        f"--threads={threads}",
        "--precision=8bit",
        "--loaders=builtin!",
        "--gpu-policy=off",
        "--palette-type=rgb",
        "-Esize",
        f"-p{colors}",
        "-o",
        output,
    ]
    if mode:
        args += ["-O"]
    return args + [path]


def decode(data):
    return run([DEC, "--threads=1", "--direct", "-i", "-", "-o", "-"], data).stdout


def measure(item, colors=256, threads=1, micro=True):
    name = f"{item['name']}-p{colors}-t{threads}"
    path = pathlib.Path(item["path"])
    streams = []
    pngs = []
    arrays = []
    ms = []
    for mode in range(2):
        cmd = command(path, colors, threads, mode)
        p = run(cmd)
        stream = p.stdout
        header = stream.split(b"q", 1)[0]
        assert b";5" in header if mode else b";5" not in header
        png = decode(stream)
        (OUT / f"{name}.{mode}.six").write_bytes(stream)
        (OUT / f"{name}.{mode}.png").write_bytes(png)
        streams.append(stream)
        pngs.append(png)
        arrays.append(rgba(png))
        ms.append(
            {
                "bytes": len(stream),
                "stream_sha256": digest(stream),
                "rgba_sha256": digest(arrays[-1].tobytes()),
                "command": list(map(str, cmd)),
            }
        )
    # Pairwise pixel equality is stronger than a perceptual score of one.
    assert arrays[0].shape == arrays[1].shape
    diff = np.abs(arrays[0].astype(np.int16) - arrays[1].astype(np.int16))
    row = {
        "name": name,
        "input": item,
        "colors": colors,
        "threads": threads,
        "modes": ms,
        "shape": list(arrays[0].shape),
        "changed_pixels": int(np.count_nonzero(np.any(diff, axis=2))),
        "max_channel_error": int(diff.max()),
        "alpha_changed_pixels": int(np.count_nonzero(diff[:, :, 3])),
    }
    # Exact repeats detect byte-level nondeterminism independently of timings.
    row["repeat_stream_equal"] = [
        run(command(path, colors, threads, m)).stdout == streams[m] for m in range(2)
    ]
    # Read-only quality evaluation is outside all timing samples.
    quality = json.loads(run([LSQA, "-L", "builtin!", path, "-"], pngs[0]).stdout)
    q = quality.get("quality", quality)
    row["normal_quality"] = {
        k: q[k] for k in ["MS-SSIM", "Δ E00_mean", "Δ Chroma_mean", "PSNR_Y"]
    }
    if row["changed_pixels"]:
        quality = json.loads(run([LSQA, "-L", "builtin!", path, "-"], pngs[1]).stdout)
        q = quality.get("quality", quality)
        row["or_quality"] = {k: q[k] for k in row["normal_quality"]}
    else:
        row["or_quality"] = row["normal_quality"].copy()
    times = [[], []]
    for rep in range(-2, RUNS):
        for mode in [0, 1] if rep % 2 == 0 else [1, 0]:
            cmd = command(path, colors, threads, mode, os.devnull)
            start = time.perf_counter()
            run(cmd)
            elapsed = time.perf_counter() - start
            if rep >= 0:
                times[mode].append(elapsed)
    row["cli_seconds"] = [stats(t) for t in times]
    if micro:
        bench = run(
            [ROOT / "indexed-bench", OUT / f"{name}.0.six", OUT / f"{name}.indexed", 21]
        )
        timings = [[], []]
        for line in bench.stdout.decode().splitlines():
            rep, mode, sec, _size = line.split(",")
            timings[int(mode)].append(float(sec))
        row["indexed_seconds"] = [stats(t) for t in timings]
        decoded = [
            rgba(decode((OUT / f"{name}.indexed.{mode}.six").read_bytes()))
            for mode in ["normal", "or"]
        ]
        delta = np.abs(decoded[0].astype(np.int16) - decoded[1].astype(np.int16))
        row["indexed_changed_pixels"] = int(np.count_nonzero(np.any(delta, axis=2)))
        row["indexed_max_channel_error"] = int(delta.max())
        row["indexed_matches_cli_normal"] = bool(np.array_equal(decoded[0], arrays[0]))
    row["size_ratio"] = ms[1]["bytes"] / ms[0]["bytes"]
    row["cli_speedup"] = (
        row["cli_seconds"][0]["median"] / row["cli_seconds"][1]["median"]
    )
    if micro:
        row["indexed_speedup"] = (
            row["indexed_seconds"][0]["median"] / row["indexed_seconds"][1]["median"]
        )
    (OUT / f"{name}.json").write_text(json.dumps(row, indent=2) + "\n")
    return row


def fixtures():
    paths = [
        "snake.png",
        "egret.jpg",
        "autumn.png",
        "fisheye.png",
        "vimperator3.png",
        "map16.png",
        "measurements/palette-pipeline/smooth-gradient-600x450.png",
        "measurements/palette-pipeline/rare-colors-600x450.png",
    ]
    items = []
    for path in paths:
        source = SRC / "images" / path
        im = Image.open(source)
        name = source.stem
        if max(im.size) > 1000:
            im = im.convert("RGB")
            im.thumbnail((800, 600), Image.Resampling.LANCZOS)
            target = ROOT / "inputs" / f"{name}-resized.png"
            im.save(target)
            name += "-resized"
        else:
            target = source
        items.append(
            {
                "name": name,
                "path": str(target),
                "source": str(source),
                "source_sha256": digest(source.read_bytes()),
                "sha256": digest(target.read_bytes()),
                "dimensions": list(Image.open(target).size),
            }
        )
    return items


def main():
    metadata = {
        "revision": run(["git", "-C", SRC, "rev-parse", "HEAD"])
        .stdout.decode()
        .strip(),
        "platform": platform.platform(),
        "cpu": run(["sysctl", "-n", "machdep.cpu.brand_string"])
        .stdout.decode()
        .strip(),
        "runs": RUNS,
        "warmups": 2,
        "env": ENV,
        "compiler": run(["cc", "--version"]).stdout.decode(),
        "configure": (ROOT / "configure.log").read_text(),
        "binary_sha256": {
            str(p): digest(p.read_bytes())
            for p in [
                IMG,
                DEC,
                LSQA,
                SRC / "src/.libs/libsixel.1.dylib",
                ROOT / "indexed-bench",
            ]
        },
    }
    # Avoid publishing irrelevant ambient environment entries.
    metadata["env"] = {
        k: ENV[k] for k in ["LC_ALL", "TZ", "SIXEL_THREADS", "DYLD_LIBRARY_PATH"]
    }
    previous = OUT / "metadata.json"
    if previous.exists():
        old = json.loads(previous.read_text())
        if any(old[k] != metadata[k] for k in ["revision", "binary_sha256", "runs"]):
            raise RuntimeError(
                "Use a fresh study directory for a different build or run count"
            )
    previous.write_text(json.dumps(metadata, indent=2) + "\n")
    items = json.loads((ROOT / "inputs.json").read_text())
    for item in items:
        item["name"] = "picsum-" + item["id"]
    controls = fixtures()
    (ROOT / "controls.json").write_text(json.dumps(controls, indent=2) + "\n")
    preflight = run(
        command(controls[0]["path"], 256, 1, 0, os.devnull)[:-1]
        + ["-v", controls[0]["path"]]
    )
    assert b"work=rgb888" in preflight.stderr
    (OUT / "preflight.txt").write_bytes(preflight.stderr)
    cases = [(item, 256, 1) for item in items] + [
        (item, p, t) for item in controls for p in [16, 256] for t in [1, 4]
    ]
    rows = []
    for i, (item, p, t) in enumerate(cases):
        name = f"{item['name']}-p{p}-t{t}"
        saved = OUT / f"{name}.json"
        row = (
            json.loads(saved.read_text())
            if saved.exists()
            else measure(item, p, t, micro=t == 1)
        )
        rows.append(row)
        print(
            f"{i + 1}/{len(cases)} {name}: size {row['size_ratio']:.3f} cli {row['cli_speedup']:.2f}x pixels {row['changed_pixels']}",
            flush=True,
        )
        (OUT / "results.json").write_text(json.dumps(rows, indent=2) + "\n")


if __name__ == "__main__":
    main()
