"""Measure color and thread axes, keeping decoder input bytes fixed."""

import argparse
import collections
import pathlib
import platform
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


def compare_decoder_builds(before, after, rows):
    """Compare OR RGBA time on the same photos with both decoder builds."""
    builds = [before.resolve(), after.resolve()]
    benches = [ROOT / "decode-bench-before", ROOT / "decode-bench"]
    photos = [r for r in rows if r["name"].startswith("picsum-")]
    result = {
        "baseline_revision": base.run(["git", "-C", before,
            "rev-parse", "HEAD"]).stdout.decode().strip(),
        "runs": 21, "warmups": 2, "format": "rgba", "threads": 1,
        "library_sha256": [base.digest((b / "src/.libs/libsixel.1.dylib")
            .read_bytes()) for b in builds],
        "cases": [],
    }
    for i, row in enumerate(photos):
        samples = [None, None]
        for version in ([0, 1] if i % 2 == 0 else [1, 0]):
            env = {**base.ENV, "DYLD_LIBRARY_PATH":
                str(builds[version] / "src/.libs")}
            proc = subprocess.run([str(benches[version]), *row["input_paths"],
                "21", "1", "rgba"], env=env, check=True,
                capture_output=True, text=True)
            times = [[], []]
            for line in proc.stdout.splitlines():
                _rep, mode, elapsed, _width, _height = line.split(",")
                times[int(mode)].append(float(elapsed))
            assert all(len(t) == 21 for t in times)
            samples[version] = [base.stats(t) for t in times]
        result["cases"].append({
            "name": row["name"], "stream_sha256": row["stream_sha256"],
            "before": samples[0], "after": samples[1],
            "or_speedup": samples[0][1]["median"] / samples[1][1]["median"],
        })
        print(f"before/after {i + 1}/{len(photos)} {row['name']}", flush=True)
    result["median_or_speedup"] = float(np.median(
        [r["or_speedup"] for r in result["cases"]]))
    (OUT / "before-after.json").write_text(json.dumps(result, indent=2) + "\n")


def decoder_only(source, reference, output, baseline=None):
    """Remeasure a decoder on retained streams without re-encoding inputs."""
    global OUT, DECODE, ENC

    source = source.resolve()
    reference = reference.resolve()
    OUT = output.resolve()
    assert OUT != reference, "Use a fresh output directory for a new decoder."
    OUT.mkdir(parents=True, exist_ok=True)
    DECODE = OUT / "decode"
    DECODE.mkdir(exist_ok=True)
    # Worker observations use the retained one-thread Full HD streams.
    ENC = ROOT / "sweeps/encode"
    base.SRC = source
    base.IMG = source / "converters/.libs/img2sixel"
    base.DEC = source / "converters/.libs/sixel2png"
    base.ENV["DYLD_LIBRARY_PATH"] = str(source / "src/.libs")
    sources = [source / name for name in [
        "src/fromsixel.c", "src/decoder-parallel.c", "src/decoder-image.h",
        "src/decoder-ormode.h", "tools/ormode/decode-bench.c",
    ]]
    metadata = {
        "platform": platform.platform(),
        "cpu": base.run(["sysctl", "-n", "machdep.cpu.brand_string"])
            .stdout.decode().strip(),
        "compiler": base.run(["cc", "--version"]).stdout.decode().strip(),
        "decoder_base_revision": base.run(["git", "-C", source, "rev-parse", "HEAD"])
            .stdout.decode().strip(),
        "decoder_source_sha256": {str(p.relative_to(source)):
            base.digest(p.read_bytes()) for p in sources if p.exists()},
        "configure": base.run([source / "config.status", "--config"])
            .stdout.decode().strip(),
        "binary_sha256": {str(p): base.digest(p.read_bytes()) for p in [
            ROOT / "decode-bench", base.DEC, source / "src/.libs/libsixel.1.dylib",
        ]},
        "retained_encoder_metadata": json.loads(
            (reference / "metadata.json").read_text()),
        "reference_metadata_sha256": base.digest(
            (reference / "metadata.json").read_bytes()),
        "encoder_records": "Retained unchanged from reference_sweeps",
        "reference_sweeps": str(reference),
        "decoder_input": "Retained fixed streams; no re-encoding",
        "colors": COLORS, "threads": THREADS,
        "cli_runs": RUNS, "api_runs": 21, "warmups": 2,
    }
    meta = OUT / "metadata.json"
    if meta.exists():
        assert json.loads(meta.read_text()) == metadata, (
            "Use a fresh output directory for a changed decoder build."
        )
    meta.write_text(json.dumps(metadata, indent=2) + "\n")
    for name in ["encode.json", "inputs.json"]:
        (OUT / name).write_bytes((reference / name).read_bytes())
    prior = json.loads((reference / "decode.json").read_text())
    rows = []
    for i, old in enumerate(prior):
        paths = [pathlib.Path(p) for p in old["input_paths"]]
        assert [base.digest(p.read_bytes()) for p in paths] == old["stream_sha256"]
        row = decode_pair(paths, old["name"], old["threads"])
        assert row["rgba_sha256"] == old["rgba_sha256"]
        rows.append(row)
        (OUT / "decode.json").write_text(json.dumps(rows, indent=2) + "\n")
        print(f"decode {i + 1}/{len(prior)} {old['name']} "
              f"dt={old['threads']} rgba={row['rgba_speedup']:.2f} "
              f"indexed={row['indexed_speedup']:.2f}", flush=True)
    observe_workers()
    if baseline is not None:
        compare_decoder_builds(baseline, source, rows)


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
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--decoder-source", type=pathlib.Path)
    parser.add_argument("--reference-sweeps", type=pathlib.Path,
                        default=ROOT / "sweeps")
    parser.add_argument("--output", type=pathlib.Path)
    parser.add_argument("--baseline-source", type=pathlib.Path)
    args = parser.parse_args()
    if args.decoder_source:
        if args.output is None:
            parser.error("--decoder-source requires a fresh --output directory")
        decoder_only(args.decoder_source, args.reference_sweeps, args.output,
                     args.baseline_source)
    else:
        if args.output is not None or args.baseline_source is not None:
            parser.error("--output/--baseline-source require --decoder-source")
        main()
