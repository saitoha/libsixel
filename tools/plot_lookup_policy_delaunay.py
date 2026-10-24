#!/usr/bin/env python3
"""Measure, validate and plot the byte/native-float Delaunay comparison."""

from __future__ import annotations

import argparse
import csv
import datetime
import hashlib
import itertools
import json
import math
import os
import platform
import re
import shlex
import statistics
import subprocess
from pathlib import Path


POLICIES = ("vptree", "delaunay", "5bit", "6bit")
REPRESENTATIONS = ("8bit", "float32")
IMAGES = ("snake", "egret", "vimperator3")
DIFFUSIONS = ("fs", "none")
COLORS = (32, 64, 128, 256)
PREFIX = "lookup-policy-delaunay"


def digest(path):
    """Hash a recorded input or executable."""
    return hashlib.sha256(path.read_bytes()).hexdigest()


def capture(command, **kwargs):
    """Run a command and retain diagnostics on failure."""
    return subprocess.run(command, check=True, capture_output=True,
                          text=True, **kwargs).stdout.strip()


def build_record(build):
    """Read actual build flags, including the isolated float arithmetic gate."""
    result = {"configure_arguments": capture([str(build / "config.status"),
                                              "--config"])}
    for line in (build / "src/Makefile").read_text().splitlines():
        match = re.match(r"^(CC|CFLAGS|CPPFLAGS|FLOAT_LOOKUP_CFLAGS) = (.*)$", line)
        if match:
            result[match[1].lower()] = match[2]
    result["compiler_version"] = capture(shlex.split(result["cc"]) + ["--version"])
    return result


def measure(args):
    """Record matched policies from one clean, fixed implementation snapshot."""
    source = args.source_dir.resolve()
    build = args.build_dir.resolve()
    output = args.directory.resolve()
    raw = args.raw_dir.resolve()
    if capture(["git", "status", "--porcelain"], cwd=source):
        raise ValueError("measurements require a clean source snapshot")
    configured_source = next(line.removeprefix("top_srcdir = ")
                             for line in (build / "Makefile").read_text().splitlines()
                             if line.startswith("top_srcdir = "))
    if (build / configured_source).resolve() != source:
        raise ValueError("build directory belongs to a different source checkout")
    output.mkdir(parents=True, exist_ok=True)
    raw.mkdir(parents=True, exist_ok=True)
    exe = build / "converters/.libs/img2sixel"
    if not exe.exists():
        exe = build / "converters/img2sixel"
    libraries = sorted((build / "src/.libs").glob("libsixel.*.dylib"))
    libraries += sorted((build / "src/.libs").glob("libsixel.so.*"))
    env = {k: v for k, v in os.environ.items() if not k.startswith("SIXEL_")}
    # Match the recorded payload to its own shared library, not an install.
    env["DYLD_LIBRARY_PATH"] = str(build / "src/.libs")
    env["LD_LIBRARY_PATH"] = str(build / "src/.libs")
    env["LC_ALL"] = "C"
    env["TZ"] = "UTC"
    converted = raw / "egret.png"
    subprocess.run(["magick", str(source / "images/egret.jpg"), "-depth", "8",
                    "PNG24:" + str(converted)], check=True)
    inputs = {"snake": source / "images/snake.png", "egret": converted,
              "vimperator3": source / "images/vimperator3.png"}
    common = [str(exe), "--threading-policy=greedy:global_budget=1", "--precision-policy=auto", "--quality=full",
              "--loaders=libpng!", "--quantize-model=kmeans:seed=1",
              "--merge-policy=ward", "-Wgamma", "--gpu-policy=off"]
    payload = {
        "schema_version": 1, "study": PREFIX,
        "generated_at_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "source": {"revision": capture(["git", "rev-parse", "HEAD"], cwd=source),
                   "tree": capture(["git", "rev-parse", "HEAD^{tree}"], cwd=source),
                   "parent_revision": capture(["git", "rev-parse", "HEAD^"], cwd=source),
                   "tracked_worktree_state_at_start": "clean",
                   "snapshot_kind": args.snapshot_kind},
        "host": {"platform": platform.platform(), "processor": platform.processor()},
        "build": build_record(build),
        "programs": {"img2sixel_sha256": digest(exe),
                     "libraries": {p.name: digest(p) for p in libraries},
                     "hyperfine": capture(["hyperfine", "--version"]),
                     "image_conversion": capture(["magick", "--version"]),
                     "measurement_script_sha256": digest(Path(__file__))},
        "protocol": {"policies": POLICIES, "representations": REPRESENTATIONS,
                     "colors": COLORS, "diffusions": DIFFUSIONS,
                     "warmups": 2, "runs": 7, "hyperfine_shell": "none (-N)",
                     "policy_order_rotated_each_round": False,
                     "sixel_environment_removed": True},
        "inputs": {}, "results": [], "output_comparisons": [],
    }
    if platform.system() == "Darwin":
        payload["host"]["cpu"] = capture(["sysctl", "-n", "machdep.cpu.brand_string"])
        payload["host"]["memory_bytes"] = int(capture(["sysctl", "-n", "hw.memsize"]))
    for name, path in inputs.items():
        original = "images/egret.jpg" if name == "egret" else f"images/{name}.png"
        payload["inputs"][name] = {
            "source_path": original, "source_sha256": digest(source / original),
            "measured_sha256": digest(path),
            "dimensions": capture(["magick", "identify", "-format", "%wx%h", str(path)]),
            "conversion": "magick {input} -depth 8 PNG24:{output}" if name == "egret" else None,
        }
    for rep, image, diffusion, colors in itertools.product(
            REPRESENTATIONS, IMAGES, DIFFUSIONS, COLORS):
        case = f"{rep}-{image}-{diffusion}-{colors}"
        flags = ["--cms-engine=none"] if rep == "8bit" else ["-Xoklab"]
        command = common + flags + [f"--diffusion={diffusion}", "-p", str(colors),
                                    str(inputs[image])]
        hashes, sizes, classes = {}, {}, {}
        for policy in POLICIES:
            # Untimed output/class probes confirm the effective representation.
            result = subprocess.run(command + [f"--lookup-policy={policy}"],
                env=dict(env, SIXEL_TRACE_TOPIC="lookup_contract"),
                check=True, capture_output=True)
            expected = f"lookup/{policy}.{rep}"
            witness = f"LSXLUT2|phase=palette-apply|selected={expected}|"
            if witness not in result.stderr.decode():
                raise ValueError(f"{case}: expected class {expected}: {result.stderr!r}")
            classes[policy] = expected
            (raw / f"{case}-{policy}.trace").write_bytes(result.stderr)
            if policy in ("vptree", "delaunay"):
                hashes[policy] = hashlib.sha256(result.stdout).hexdigest()
                sizes[policy] = len(result.stdout)
        if hashes["vptree"] != hashes["delaunay"]:
            raise ValueError(f"{case}: SIXEL output differs")
        payload["output_comparisons"].append({
            "case": case, "sha256": hashes, "bytes": sizes})
        template = shlex.join(command + ["--lookup-policy={policy}", "-o", os.devnull])
        with (raw / f"{case}.log").open("w") as log:
            subprocess.run(["hyperfine", "-N", "--warmup", "2", "--runs", "7",
                "--export-json", str(raw / f"{case}.json"), "-L", "policy",
                ",".join(POLICIES), template], env=env, check=True,
                stdout=log, stderr=subprocess.STDOUT)
        records = json.loads((raw / f"{case}.json").read_text())["results"]
        for policy, record in zip(POLICIES, records, strict=True):
            portable = record["command"].replace(str(exe), "{img2sixel}")
            portable = portable.replace(str(inputs[image]), "{input}")
            payload["results"].append({
                "case": case, "representation": rep, "image": image,
                "diffusion": diffusion, "colors": colors, "policy": policy,
                "selected_class": classes[policy], "command": portable,
                **{key: record[key] for key in
                   ("mean", "stddev", "median", "min", "max", "times", "exit_codes")},
            })
        # Keep partial results outside the source tree until it is rechecked.
        (raw / "run-progress.json").write_text(json.dumps(payload, indent=2) + "\n")
        print(case, flush=True)
    if capture(["git", "status", "--porcelain"], cwd=source):
        raise ValueError("source snapshot changed during measurement")
    if capture(["git", "rev-parse", "HEAD"], cwd=source) != payload["source"]["revision"]:
        raise ValueError("source revision changed during measurement")
    if digest(exe) != payload["programs"]["img2sixel_sha256"]:
        raise ValueError("measured executable changed during the run")
    for path in libraries:
        if digest(path) != payload["programs"]["libraries"][path.name]:
            raise ValueError("measured library changed during the run")
    (output / f"{PREFIX}-run.json").write_text(json.dumps(payload, indent=2) + "\n")
    render(output)


def load_validated(directory):
    """Check the complete matrix, samples, class witnesses and output equality."""
    data = json.loads((directory / f"{PREFIX}-run.json").read_text())
    if data.get("schema_version") != 1 or data.get("study") != PREFIX:
        raise ValueError("unsupported Delaunay measurement schema")
    source, protocol = data["source"], data["protocol"]
    if source["tracked_worktree_state_at_start"] != "clean":
        raise ValueError("checked-in measurements require a clean source snapshot")
    for field in ("revision", "tree", "parent_revision"):
        if not re.fullmatch(r"[0-9a-f]{40}", source[field]):
            raise ValueError(f"missing source {field}")
    if source["snapshot_kind"] not in ("repository", "local-measurement"):
        raise ValueError("unknown source snapshot kind")
    for field, values in (("policies", POLICIES), ("representations", REPRESENTATIONS),
                          ("colors", COLORS), ("diffusions", DIFFUSIONS)):
        if protocol[field] != list(values):
            raise ValueError(f"unexpected protocol {field}")
    if protocol["runs"] != 7 or protocol["warmups"] != 2:
        raise ValueError("unexpected hyperfine sampling protocol")
    if protocol["sixel_environment_removed"] is not True:
        raise ValueError("missing SIXEL environment isolation")
    expected = set(itertools.product(REPRESENTATIONS, IMAGES, DIFFUSIONS, COLORS, POLICIES))
    actual = set()
    for row in data["results"]:
        key = tuple(row[k] for k in ("representation", "image", "diffusion", "colors", "policy"))
        if key in actual:
            raise ValueError(f"duplicate result: {key}")
        actual.add(key)
        rep, image, diffusion, colors, policy = key
        if row["case"] != f"{rep}-{image}-{diffusion}-{colors}":
            raise ValueError("case identity differs from dimensions")
        if row["selected_class"] != f"lookup/{policy}.{rep}":
            raise ValueError("effective lookup class differs from requested representation")
        samples = row["times"]
        if len(samples) != 7 or any(not math.isfinite(t) or t <= 0 for t in samples):
            raise ValueError("missing or invalid timing samples")
        if row["exit_codes"] != [0] * 7:
            raise ValueError("failed benchmark command")
        values = {"mean": statistics.mean(samples), "stddev": statistics.stdev(samples),
                  "median": statistics.median(samples), "min": min(samples), "max": max(samples)}
        if any(not math.isclose(row[k], v, rel_tol=1e-10, abs_tol=1e-12)
               for k, v in values.items()):
            raise ValueError("summary statistics differ from raw samples")
        command = row["command"]
        required = [f"--lookup-policy={policy}", f"--diffusion={diffusion}",
                    "--threading-policy=greedy:global_budget=1", "--gpu-policy=off", "--loaders=libpng!",
                    "--precision-policy=auto", "--quality=full",
                    "--quantize-model=kmeans:seed=1", "--merge-policy=ward", "-Wgamma"]
        required += ["--cms-engine=none"] if rep == "8bit" else ["-Xoklab"]
        arguments = shlex.split(command)
        if any(flag not in arguments for flag in required):
            raise ValueError("command differs from the comparison protocol")
        if "-p" not in arguments or arguments[arguments.index("-p") + 1] != str(colors):
            raise ValueError("command palette size differs from its result")
    if actual != expected:
        raise ValueError(f"incomplete Delaunay matrix: {len(actual)} of {len(expected)}")
    comparisons = {row["case"]: row for row in data["output_comparisons"]}
    if len(comparisons) != 48 or len(data["output_comparisons"]) != 48:
        raise ValueError("missing or duplicate output comparisons")
    if set(comparisons) != {row["case"] for row in data["results"]}:
        raise ValueError("output comparison cases differ from timing cases")
    for row in comparisons.values():
        if row["sha256"]["vptree"] != row["sha256"]["delaunay"]:
            raise ValueError("output hashes differ")
        if not re.fullmatch(r"[0-9a-f]{64}", row["sha256"]["vptree"]):
            raise ValueError("invalid output hash")
        if row["bytes"]["vptree"] != row["bytes"]["delaunay"] or row["bytes"]["vptree"] <= 0:
            raise ValueError("output sizes differ or are empty")
    return data


def csv_rows(data):
    """Derive a portable, auditable summary table from the retained samples."""
    for row in data["results"]:
        yield {"revision": data["source"]["revision"],
               **{k: row[k] for k in ("case", "representation", "image", "diffusion",
                                      "colors", "policy", "selected_class")},
               "runs": len(row["times"]),
               **{k + "_seconds": row[k] for k in ("mean", "stddev", "median", "min", "max")},
               "command": row["command"]}


def validate_artifacts(directory):
    """Require the CSV to agree exactly with the source JSON and both plots."""
    data = load_validated(directory)
    with (directory / f"{PREFIX}-speed.csv").open(newline="") as handle:
        actual = list(csv.DictReader(handle))
    expected = [{k: str(v) for k, v in row.items()} for row in csv_rows(data)]
    if actual != expected:
        raise ValueError("Delaunay CSV differs from retained raw samples")
    for rep in REPRESENTATIONS:
        path = directory / f"{PREFIX}-{rep}-speed.png"
        if not path.is_file() or path.stat().st_size == 0:
            raise ValueError(f"missing plot: {path}")


def render(directory):
    """Plot means and sample standard deviations in separate representation panels."""
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt

    data = load_validated(directory)
    rows = list(csv_rows(data))
    with (directory / f"{PREFIX}-speed.csv").open("w", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(rows[0]), lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)
    styles = {"vptree": ("#56B4E9", "X", ":"), "delaunay": ("#009E73", "D", "-"),
              "5bit": ("#D55E00", "s", "--"), "6bit": ("#0072B2", "^", "-.")}
    for rep in REPRESENTATIONS:
        fig, axes = plt.subplots(2, 3, figsize=(12, 7), sharex=True, sharey=True)
        for row_index, diffusion in enumerate(DIFFUSIONS):
            for column, image in enumerate(IMAGES):
                ax = axes[row_index, column]
                for policy in POLICIES:
                    points = sorted((r for r in data["results"] if
                        (r["representation"], r["diffusion"], r["image"], r["policy"])
                        == (rep, diffusion, image, policy)), key=lambda r: r["colors"])
                    color, marker, line = styles[policy]
                    ax.errorbar(COLORS, [r["mean"] * 1000 for r in points],
                        yerr=[r["stddev"] * 1000 for r in points], label=policy,
                        color=color, marker=marker, linestyle=line, capsize=3,
                        linewidth=2 if policy == "delaunay" else 1.5, markersize=5)
                ax.set_title(f"{image} / {'Floyd–Steinberg' if diffusion == 'fs' else 'No diffusion'}", fontsize=11)
                ax.set_xticks(COLORS)
                ax.grid(color="#dddddd", linewidth=0.6)
                if column == 0:
                    ax.set_ylabel("Mean elapsed time (ms)")
                if row_index == 1:
                    ax.set_xlabel("Palette size K")
        # Shared limits must include every panel and its complete error bars.
        # Setting a limit inside the loop would disable subsequent autoscaling.
        maximum = max((r["mean"] + r["stddev"]) * 1000
                      for r in data["results"] if r["representation"] == rep)
        axes[0, 0].set_ylim(0, maximum * 1.08)
        label = "8bit lookup · CMS disabled" if rep == "8bit" else "float32 lookup · default CMS, Oklab clustering"
        fig.suptitle("Delaunay comparison: " + label, fontsize=15, y=0.98)
        handles, labels = axes[0, 0].get_legend_handles_labels()
        fig.legend(handles, labels, loc="upper center", bbox_to_anchor=(0.5, 0.935),
                   ncol=4, frameon=False)
        fig.text(0.5, 0.025, "One thread; 2 warmups + 7 fresh processes; bars = sample SD; lower is better",
                 ha="center", fontsize=10, color="#555555")
        fig.tight_layout(rect=(0, 0.06, 1, 0.88))
        fig.savefig(directory / f"{PREFIX}-{rep}-speed.png", dpi=160)
        plt.close(fig)
    validate_artifacts(directory)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("measure", "plot", "validate"))
    parser.add_argument("directory", type=Path)
    parser.add_argument("--source-dir", type=Path)
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--raw-dir", type=Path)
    parser.add_argument("--snapshot-kind", choices=("repository", "local-measurement"), default="repository")
    args = parser.parse_args()
    if args.action == "measure":
        if any(p is None for p in (args.source_dir, args.build_dir, args.raw_dir)):
            parser.error("measure requires --source-dir, --build-dir and --raw-dir")
        measure(args)
    elif args.action == "plot":
        render(args.directory)
    else:
        validate_artifacts(args.directory)
        print("validated 192 Delaunay comparison timings and 48 exact outputs")


if __name__ == "__main__":
    main()
