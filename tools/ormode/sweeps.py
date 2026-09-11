"""Measure color and thread axes, keeping decoder input bytes fixed."""

import collections
import json
import os
import subprocess
import time

import measure as base
import numpy as np
from PIL import Image

ROOT = base.ROOT
OUT = ROOT / "sweeps"
OUT.mkdir(exist_ok=True)
ENC = OUT / "encode"
ENC.mkdir(exist_ok=True)
DECODE = OUT / "decode"
DECODE.mkdir(exist_ok=True)
COLORS = [2, 4, 8, 16, 32, 64, 128, 256]
THREADS = list(range(1, 13))
RUNS = 11


def decode_pair(paths, name, threads):
    saved = DECODE / f"{name}-dt{threads}.json"
    hashes = [base.digest(p.read_bytes()) for p in paths]
    if saved.exists():
        row = json.loads(saved.read_text())
        assert row["stream_sha256"] == hashes
        return row
    row = {
        "name": name,
        "threads": threads,
        "stream_sha256": hashes,
        "input_paths": list(map(str, paths)),
        "bytes": [p.stat().st_size for p in paths],
    }
    for fmt in ["rgba", "indexed"]:
        proc = base.run([ROOT / "decode-bench", *paths, 21, threads, fmt])
        samples = [[], []]
        for line in proc.stdout.decode().splitlines():
            _rep, mode, elapsed, width, height = line.split(",")
            samples[int(mode)].append(float(elapsed))
        assert all(len(s) == 21 for s in samples)
        row[fmt + "_seconds"] = [base.stats(s) for s in samples]
        row[fmt + "_speedup"] = np.median(samples[0]) / np.median(samples[1])
        row["dimensions"] = [int(width), int(height)]
    # The in-process harness verifies every output pixel outside timing.
    # Independently verify the CLI's RGBA result at this thread budget.
    arrays = []
    commands = []
    for path in paths:
        args = [base.DEC, f"--threads={threads}", "--direct", "-i", path, "-o", "-"]
        png = base.run(args).stdout
        arrays.append(base.rgba(png))
        commands.append([str(x) for x in args[:-1] + [os.devnull]])
    assert arrays[0].shape == arrays[1].shape
    assert np.array_equal(arrays[0], arrays[1])
    row["rgba_sha256"] = [base.digest(a.tobytes()) for a in arrays]
    row["cli_changed_pixels"] = 0
    row["cli_commands"] = commands
    samples = [[], []]
    for rep in range(-2, RUNS):
        for mode in [0, 1] if rep % 2 == 0 else [1, 0]:
            begin = time.perf_counter()
            base.run(commands[mode])
            elapsed = time.perf_counter() - begin
            if rep >= 0:
                samples[mode].append(elapsed)
    row["cli_seconds"] = [base.stats(s) for s in samples]
    row["cli_speedup"] = np.median(samples[0]) / np.median(samples[1])
    saved.write_text(json.dumps(row, indent=2) + "\n")
    return row


def observe_workers():
    """Confirm effective worker counts in untimed Full HD API calls."""
    observations = []
    paths = [ENC / f"snake-fullhd-p256-t1.{mode}.six" for mode in [0, 1]]
    for threads in [1, 4, 12]:
        for fmt in ["rgba", "indexed"]:
            log = OUT / f"trace-{fmt}-{threads}.jsonl"
            log.unlink(missing_ok=True)
            subprocess.run(
                [str(ROOT / "decode-bench"), *map(str, paths), "0", str(threads), fmt],
                env={**base.ENV, "SIXEL_LOG_PATH": str(log)},
                check=True,
                capture_output=True,
            )
            records = [
                json.loads(line)
                for line in log.read_text().splitlines()
                if line.strip()
            ]
            counts = collections.Counter(
                "/".join(str(record.get(key)) for key in ["worker", "role", "event"])
                for record in records
            )
            expected = 0 if threads == 1 else 4 * threads
            assert counts["decoder/scan/start"] == expected
            assert counts["decoder/paint/start"] == expected
            observations.append(
                {
                    "threads": threads,
                    "format": fmt,
                    "calls": 4,
                    "event_counts": dict(counts),
                }
            )
    (OUT / "thread-observations.json").write_text(
        json.dumps(observations, indent=2) + "\n"
    )


def main():
    metadata = {
        "revision": base.run(["git", "-C", base.SRC, "rev-parse", "HEAD"])
        .stdout.decode()
        .strip(),
        "colors": COLORS,
        "threads": THREADS,
        "cli_runs": RUNS,
        "api_runs": 21,
        "warmups": 2,
        "binary_sha256": {
            str(p): base.digest(p.read_bytes())
            for p in [
                base.IMG,
                base.DEC,
                ROOT / "decode-bench",
                base.SRC / "src/.libs/libsixel.1.dylib",
            ]
        },
        "decoder_input": "Encode once at threads=1, then reuse identical stream bytes across all decoder budgets.",
    }
    meta = OUT / "metadata.json"
    if meta.exists():
        assert json.loads(meta.read_text()) == metadata, (
            "Use a fresh sweeps directory for a changed build."
        )
    meta.write_text(json.dumps(metadata, indent=2) + "\n")
    items = base.fixtures()
    image = Image.open(base.SRC / "images/snake.png").convert("RGB")
    image = image.resize((1920, 1080), Image.Resampling.LANCZOS)
    path = ROOT / "inputs/snake-fullhd.png"
    image.save(path)
    fullhd = {
        "name": "snake-fullhd",
        "path": str(path),
        "sha256": base.digest(path.read_bytes()),
        "dimensions": [1920, 1080],
        "preprocess": "RGB conversion and Lanczos resize to exactly 1920x1080 before timing",
    }
    (OUT / "inputs.json").write_text(json.dumps([*items, fullhd], indent=2) + "\n")
    # Interleave ascending and descending color/thread traversal by image.
    cases = []
    for i, item in enumerate(items):
        cases.extend((item, c, 1) for c in (COLORS if i % 2 == 0 else COLORS[::-1]))
    for i, item in enumerate([*items, fullhd]):
        cases.extend(
            (item, 256, t)
            for t in (THREADS if i % 2 == 0 else THREADS[::-1])
            if t != 1 or item is fullhd
        )
    base.OUT = ENC
    enc = []
    for i, (item, colors, threads) in enumerate(cases):
        name = f"{item['name']}-p{colors}-t{threads}"
        saved = ENC / f"{name}.json"
        row = (
            json.loads(saved.read_text())
            if saved.exists()
            else base.measure(item, colors, threads, micro=threads == 1)
        )
        enc.append(row)
        print(
            f"encode {i + 1}/{len(cases)} {name} size={row['size_ratio']:.3f} speed={row['cli_speedup']:.2f} pixels={row['changed_pixels']}",
            flush=True,
        )
        (OUT / "encode.json").write_text(json.dumps(enc, indent=2) + "\n")
    decoder_cases = []
    seen = set()
    for item, colors, threads in cases:
        name = f"{item['name']}-p{colors}-t1"
        key = (name, threads)
        if key not in seen:
            seen.add(key)
            decoder_cases.append(
                ([ENC / f"{name}.{m}.six" for m in [0, 1]], name, threads)
            )
    prior = json.loads((ROOT / "results/results.json").read_text())
    for row in prior:
        if row["input"]["name"].startswith("picsum-"):
            decoder_cases.append(
                (
                    [ROOT / "results" / f"{row['name']}.{m}.six" for m in [0, 1]],
                    row["name"],
                    1,
                )
            )
    dec = []
    for i, (paths, name, threads) in enumerate(decoder_cases):
        row = decode_pair(paths, name, threads)
        baseline = json.loads((paths[0].parent / f"{name}.json").read_text())
        expected_hash = baseline["modes"][0]["rgba_sha256"]
        assert row["rgba_sha256"] == [expected_hash, expected_hash]
        dec.append(row)
        print(
            f"decode {i + 1}/{len(decoder_cases)} {name} dt={threads} rgba={row['rgba_speedup']:.2f} indexed={row['indexed_speedup']:.2f} cli={row['cli_speedup']:.2f}",
            flush=True,
        )
        (OUT / "decode.json").write_text(json.dumps(dec, indent=2) + "\n")

    observe_workers()


if __name__ == "__main__":
    main()
