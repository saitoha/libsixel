#!/usr/bin/env python3
"""Measure shared lookup instances and Metal-assisted palette application."""

from __future__ import annotations

import argparse
import csv
import datetime
import hashlib
import json
import os
import platform
import shlex
import statistics
import subprocess
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, List, Sequence

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt
from matplotlib import ticker

from plot_lookup_policy_speed import (
    display_path,
    file_sha256,
    make_command_environment,
    percentile,
    program_record,
    read_build_configuration,
    resolve_img2sixel,
    resolve_metadata_program,
    run_once,
)


DEFAULT_COLORS = "8,16,32,64,128,256"
SHARED_POLICIES = ("5bit", "6bit", "certlut")
METAL_POLICIES = ("none", "eytzinger")
POLICY_COLORS = {
    "none": "#4D4D4D",
    "5bit": "#D55E00",
    "6bit": "#0072B2",
    "certlut": "#009E73",
    "eytzinger": "#CC79A7",
}


@dataclass(frozen=True)
class Variant:
    """Describe one execution mode in an acceleration comparison."""

    label: str
    policy: str
    threads: int
    gpu_policy: str
    shared_instance: int | None


def parse_colors(raw: str) -> List[int]:
    """Parse a unique, non-empty list of positive palette sizes."""
    values = [int(value.strip()) for value in raw.split(",") if value.strip()]
    if not values or any(value <= 0 for value in values):
        raise ValueError("Palette sizes must be positive.")
    if len(values) != len(set(values)):
        raise ValueError("Palette sizes must not be repeated.")
    return values


def shared_variants(threads: int) -> List[Variant]:
    """Return private and shared lookup instances at one thread count."""
    variants: List[Variant] = []
    for policy in SHARED_POLICIES:
        variants.append(
            Variant(f"{policy} S0", policy, threads, "off", 0)
        )
        variants.append(
            Variant(f"{policy} S1", policy, threads, "off", 1)
        )
    return variants


def metal_variants() -> List[Variant]:
    """Return CPU and forced-Metal modes for GPU-supported policies."""
    variants: List[Variant] = []
    for policy in METAL_POLICIES:
        variants.append(Variant(f"CPU {policy}", policy, 1, "off", None))
        variants.append(
            Variant(f"Metal {policy}", policy, 1, "force", None)
        )
    return variants


def make_command(img2sixel: str,
                 input_image: Path,
                 colors: int,
                 variant: Variant,
                 output: str | None) -> List[str]:
    """Build one controlled end-to-end acceleration command."""
    lookup_policy = variant.policy
    if variant.shared_instance is not None:
        lookup_policy += f":shared_instance={variant.shared_instance}"
    command = [
        img2sixel,
        f"--threads={variant.threads}",
        "--precision=8bit",
        "--quality=full",
        "--loaders=libpng!",
        "--quantize-model=kmeans:merge=ward:seed=1",
        "-Xoklab",
        "-Wgamma",
        "--diffusion=none:band_overwrap=0",
        f"--gpu-policy={variant.gpu_policy}",
        f"--lookup-policy={lookup_policy}",
        "-p",
        str(colors),
    ]
    if output is not None:
        command.extend(["-o", output])
    command.append(str(input_image))
    return command


def command_template(command: Sequence[str],
                     img2sixel: str,
                     input_image: Path) -> str:
    """Replace host paths in one command with auditable placeholders."""
    text = shlex.join(command)
    text = text.replace(img2sixel, "{img2sixel}", 1)
    return text.replace(str(input_image), "{input}", 1)


def measure_comparison(name: str,
                       img2sixel: str,
                       input_image: Path,
                       input_label: str,
                       colors: Sequence[int],
                       variants: Sequence[Variant],
                       warmups: int,
                       runs: int,
                       revision: str,
                       command_env: Dict[str, str]) -> List[Dict[str, object]]:
    """Measure variants in rotated order and return complete statistics."""
    rows: List[Dict[str, object]] = []
    for color_count in colors:
        commands = {
            variant.label: make_command(
                img2sixel,
                input_image,
                color_count,
                variant,
                os.devnull,
            )
            for variant in variants
        }
        for warmup in range(warmups):
            offset = warmup % len(variants)
            order = list(variants[offset:]) + list(variants[:offset])
            for variant in order:
                run_once(commands[variant.label], command_env)

        samples: Dict[str, List[float]] = {
            variant.label: [] for variant in variants
        }
        for run_index in range(runs):
            offset = run_index % len(variants)
            order = list(variants[offset:]) + list(variants[:offset])
            for variant in order:
                samples[variant.label].append(
                    run_once(commands[variant.label], command_env)
                )

        for variant in variants:
            values = samples[variant.label]
            rows.append(
                {
                    "revision": revision,
                    "platform": platform.platform(),
                    "comparison": name,
                    "input": input_label,
                    "colors": color_count,
                    "variant": variant.label,
                    "policy": variant.policy,
                    "threads": variant.threads,
                    "gpu_policy": variant.gpu_policy,
                    "shared_instance": (
                        "" if variant.shared_instance is None
                        else variant.shared_instance
                    ),
                    "runs": runs,
                    "median_seconds": statistics.median(values),
                    "q1_seconds": percentile(values, 0.25),
                    "q3_seconds": percentile(values, 0.75),
                    "min_seconds": min(values),
                    "max_seconds": max(values),
                    "command": command_template(
                        commands[variant.label], img2sixel, input_image
                    ),
                }
            )
    return rows


def write_csv(path: Path, rows: Sequence[Dict[str, object]]) -> None:
    """Write one acceleration comparison with full command provenance."""
    fieldnames = [
        "revision",
        "platform",
        "comparison",
        "input",
        "colors",
        "variant",
        "policy",
        "threads",
        "gpu_policy",
        "shared_instance",
        "runs",
        "median_seconds",
        "q1_seconds",
        "q3_seconds",
        "min_seconds",
        "max_seconds",
        "command",
    ]
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def plot(path: Path,
         rows: Sequence[Dict[str, object]],
         colors: Sequence[int],
         variants: Sequence[Variant],
         title: str,
         runs: int) -> None:
    """Plot median end-to-end latency and interquartile ranges."""
    figure, runtime_ax = plt.subplots(figsize=(9.0, 4.8))
    for variant in variants:
        variant_rows = [
            row for row in rows if row["variant"] == variant.label
        ]
        variant_rows.sort(key=lambda row: int(row["colors"]))
        median_ms = [
            float(row["median_seconds"]) * 1000.0 for row in variant_rows
        ]
        q1_ms = [float(row["q1_seconds"]) * 1000.0 for row in variant_rows]
        q3_ms = [float(row["q3_seconds"]) * 1000.0 for row in variant_rows]
        lower = [median - q1 for median, q1 in zip(median_ms, q1_ms)]
        upper = [q3 - median for median, q3 in zip(median_ms, q3_ms)]
        accelerated = (
            variant.gpu_policy == "force" or variant.shared_instance == 1
        )
        runtime_ax.errorbar(
            colors,
            median_ms,
            yerr=[lower, upper],
            color=POLICY_COLORS[variant.policy],
            linestyle="-" if accelerated else "--",
            marker="o" if accelerated else "s",
            linewidth=1.8,
            markersize=5.5,
            capsize=2.5,
            label=variant.label,
        )

    runtime_ax.set_title(title)
    runtime_ax.set_xlabel("Palette size K")
    runtime_ax.set_ylabel("Median elapsed time (ms)")
    runtime_ax.yaxis.set_major_formatter(ticker.StrMethodFormatter("{x:.0f}"))
    runtime_ax.set_xticks(colors)
    runtime_ax.grid(True, color="#D9D9D9", linewidth=0.7)
    runtime_ax.legend(ncol=2, frameon=False)
    figure.text(
        0.99,
        0.01,
        f"Fresh process per sample; median of {runs}; bars show IQR",
        horizontalalignment="right",
        verticalalignment="bottom",
        fontsize=8,
        color="#555555",
    )
    figure.tight_layout(rect=(0.0, 0.035, 1.0, 1.0))
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=160)
    plt.close(figure)


def run_for_bytes(command: Sequence[str], env: Dict[str, str]) -> bytes:
    """Run one command and return its SIXEL bytes for equivalence checks."""
    proc = subprocess.run(
        list(command),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=env,
        check=False,
    )
    if proc.returncode != 0:
        diagnostic = proc.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(
            f"Command failed ({proc.returncode}): {shlex.join(command)}\n"
            f"{diagnostic}"
        )
    return proc.stdout


def verify_metal_outputs(img2sixel: str,
                         input_image: Path,
                         colors: int,
                         command_env: Dict[str, str]) -> Dict[str, object]:
    """Require forced Metal and compare its bytes with each CPU policy."""
    records: Dict[str, object] = {}
    for policy in METAL_POLICIES:
        cpu = Variant(f"CPU {policy}", policy, 1, "off", None)
        metal = Variant(f"Metal {policy}", policy, 1, "force", None)
        cpu_command = make_command(img2sixel, input_image, colors, cpu, None)
        metal_command = make_command(
            img2sixel, input_image, colors, metal, None
        )
        cpu_bytes = run_for_bytes(cpu_command, command_env)
        metal_bytes = run_for_bytes(metal_command, command_env)
        if cpu_bytes != metal_bytes:
            raise RuntimeError(
                f"CPU and forced-Metal output differ for {policy} at K={colors}."
            )
        records[policy] = {
            "colors": colors,
            "byte_identical": True,
            "output_size": len(cpu_bytes),
            "sha256": hashlib.sha256(cpu_bytes).hexdigest(),
        }
    return records


def metal_device_record() -> Dict[str, object]:
    """Record the macOS display inventory used for Metal identification."""
    command = ["/usr/sbin/system_profiler", "SPDisplaysDataType", "-json"]
    proc = subprocess.run(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        check=False,
        text=True,
    )
    if proc.returncode != 0:
        return {"query": shlex.join(command), "available": False}
    try:
        payload = json.loads(proc.stdout)
    except json.JSONDecodeError:
        return {"query": shlex.join(command), "available": False}
    return {
        "query": shlex.join(command),
        "available": True,
        "inventory": payload.get("SPDisplaysDataType", []),
    }


def write_metadata(path: Path,
                   source_root: Path,
                   build_dir: Path,
                   input_image: Path,
                   input_label: str,
                   img2sixel: str,
                   lsqa: str | None,
                   colors: Sequence[int],
                   shared_threads: int,
                   warmups: int,
                   runs: int,
                   revision: str,
                   source_state: str,
                   clean_sixel_environment: bool,
                   equivalence: Dict[str, object]) -> None:
    """Write provenance for both acceleration comparisons."""
    programs = {"img2sixel": program_record(img2sixel, source_root)}
    if lsqa is not None:
        programs["lsqa"] = program_record(lsqa, source_root)
    payload = {
        "schema_version": 1,
        "generated_at_utc": datetime.datetime.now(
            datetime.timezone.utc
        ).isoformat(),
        "source": {
            "revision": revision,
            "tracked_worktree_state_at_start": source_state,
        },
        "build": read_build_configuration(build_dir),
        "host": {
            "platform": platform.platform(),
            "processor": platform.processor(),
            "python": platform.python_version(),
            "matplotlib": matplotlib.__version__,
            "metal_device": metal_device_record(),
        },
        "input": {
            "path": input_label,
            "sha256": file_sha256(input_image),
        },
        "programs": programs,
        "protocol": {
            "colors": list(colors),
            "warmups": warmups,
            "runs": runs,
            "fresh_process_per_sample": True,
            "policy_order_rotated_each_round": True,
            "sixel_environment_removed": clean_sixel_environment,
            "shared_instance": {
                "threads": shared_threads,
                "policies": list(SHARED_POLICIES),
                "values": [0, 1],
            },
            "metal": {
                "threads": 1,
                "policies": list(METAL_POLICIES),
                "cpu_gpu_policies": ["off", "force"],
                "force_success_required": True,
                "output_equivalence": equivalence,
            },
        },
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        json.dump(payload, handle, indent=2, sort_keys=True)
        handle.write("\n")


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("--colors", default=DEFAULT_COLORS)
    parser.add_argument("--shared-threads", type=int, default=8)
    parser.add_argument("--warmups", type=int, default=2)
    parser.add_argument("--runs", type=int, default=9)
    parser.add_argument("--img2sixel")
    parser.add_argument("--lsqa")
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--revision", default="unknown")
    parser.add_argument("--source-state", default="unknown")
    parser.add_argument("--clean-sixel-environment", action="store_true")
    parser.add_argument("--output-dir", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    """Run both focused acceleration comparisons."""
    args = parse_args()
    source_root = Path(__file__).resolve().parent.parent
    input_image = args.input.resolve()
    input_label = display_path(input_image, source_root)
    colors = parse_colors(args.colors)
    if args.shared_threads < 2:
        raise ValueError("Shared-instance measurement needs at least 2 threads.")
    if args.warmups < 0 or args.runs < 1:
        raise ValueError("Warmups must be non-negative and runs must be positive.")
    if not input_image.is_file():
        raise FileNotFoundError(f"Input image does not exist: {input_image}")

    img2sixel = resolve_img2sixel(args.img2sixel, source_root)
    command_env = make_command_environment(args.clean_sixel_environment)
    shared_modes = shared_variants(args.shared_threads)
    metal_modes = metal_variants()
    shared_rows = measure_comparison(
        "shared_instance",
        img2sixel,
        input_image,
        input_label,
        colors,
        shared_modes,
        args.warmups,
        args.runs,
        args.revision,
        command_env,
    )
    metal_rows = measure_comparison(
        "metal",
        img2sixel,
        input_image,
        input_label,
        colors,
        metal_modes,
        args.warmups,
        args.runs,
        args.revision,
        command_env,
    )
    equivalence = verify_metal_outputs(
        img2sixel, input_image, max(colors), command_env
    )

    output_dir = args.output_dir
    write_csv(output_dir / "lookup-policy-shared-speed.csv", shared_rows)
    write_csv(output_dir / "lookup-policy-metal-speed.csv", metal_rows)
    plot(
        output_dir / "lookup-policy-shared-speed.png",
        shared_rows,
        colors,
        shared_modes,
        f"Shared lookup-instance runtime on {input_image.name}",
        args.runs,
    )
    plot(
        output_dir / "lookup-policy-metal-speed.png",
        metal_rows,
        colors,
        metal_modes,
        f"CPU and forced-Metal runtime on {input_image.name}",
        args.runs,
    )
    lsqa = resolve_metadata_program(
        args.lsqa, source_root, "assessment/lsqa"
    )
    write_metadata(
        output_dir / "lookup-policy-acceleration-run.json",
        source_root,
        (args.build_dir or source_root).resolve(),
        input_image,
        input_label,
        img2sixel,
        lsqa,
        colors,
        args.shared_threads,
        args.warmups,
        args.runs,
        args.revision,
        args.source_state,
        args.clean_sixel_environment,
        equivalence,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
