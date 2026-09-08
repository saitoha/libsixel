#!/usr/bin/env python3
"""Measure pixel-format precision transitions and resize quality."""

from __future__ import annotations

import argparse
import csv
import datetime
import hashlib
import json
import os
import platform
import re
import shlex
import subprocess
import tempfile
from pathlib import Path
from typing import Dict, List, Sequence, Tuple

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


CHECKER_WIDTH = 600
CHECKER_HEIGHT = 450
OUTPUT_WIDTH = 300
OUTPUT_HEIGHT = 225
PALETTE_COLORS = 256
PATH_CASES: Tuple[Tuple[str, str, str, str, str, str], ...] = (
    ("gamma-8bit", "Gamma, requested 8-bit", "8bit", "gamma", "gamma", "plain"),
    ("gamma-float32", "Gamma, requested float32", "float32", "gamma", "gamma", "plain"),
    ("working-linear-8bit", "Linear, requested 8-bit", "8bit", "linear", "linear", "plain"),
    ("working-linear-float32", "Linear, requested float32", "float32", "linear", "linear", "plain"),
    ("working-oklab-8bit", "Oklab, requested 8-bit", "8bit", "oklab", "oklab", "plain"),
    ("working-oklab-float32", "Oklab, requested float32", "float32", "oklab", "oklab", "plain"),
    ("cms-gamma-byte", "CMS gamma, prefer 8-bit", "8bit", "gamma", "gamma", "cms-gamma-byte"),
    ("cms-gamma-float", "CMS gamma, retain float32", "8bit", "gamma", "gamma", "cms-gamma-float"),
    ("cms-linear-byte", "CMS linear via RGB888", "8bit", "linear", "linear", "cms-linear-byte"),
    ("cms-linear-float", "CMS linear float32", "8bit", "linear", "linear", "cms-linear-float"),
    ("cms-oklab-8bit", "CMS Oklab, requested 8-bit", "8bit", "oklab", "oklab", "cms-oklab-float"),
    ("cms-oklab-float32", "CMS Oklab, requested float32", "float32", "oklab", "oklab", "cms-oklab-float"),
)
RESIZE_CASES: Tuple[Tuple[str, str, str], ...] = (
    ("preserve", "RGB888 preserve", "preserve"),
    ("auto", "automatic linear-f32", ""),
    ("float", "float32 work path", "float"),
)
CMS_INPUT = Path("images/measurements/palette-pipeline/a98-gamut-600x450.png")
PLAIN_INPUT = Path("images/snake.png")


def file_sha256(path: Path) -> str:
    """Return one file's SHA-256 digest."""
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for block in iter(lambda: handle.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def program_record(path: Path) -> Dict[str, str]:
    """Describe one measured executable."""
    proc = subprocess.run(
        [str(path), "--version"],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        check=False,
    )
    first_line = proc.stdout.decode("utf-8", errors="replace").splitlines()
    return {
        "path": str(path),
        "sha256": file_sha256(path),
        "version": first_line[0] if first_line else "unknown",
    }


def clean_environment() -> Dict[str, str]:
    """Return a deterministic command environment without inherited policy."""
    environment = {
        key: value
        for key, value in os.environ.items()
        if not key.startswith("SIXEL_") and not key.startswith("LSQA_")
    }
    environment["LC_ALL"] = "C"
    environment["TZ"] = "UTC"
    return environment


def write_checker_fixture(path: Path) -> None:
    """Write a one-pixel black-and-white checker as binary PPM."""
    pixels = bytearray(CHECKER_WIDTH * CHECKER_HEIGHT * 3)
    offset = 0
    for y in range(CHECKER_HEIGHT):
        for x in range(CHECKER_WIDTH):
            value = 255 if (x + y) & 1 else 0
            pixels[offset:offset + 3] = bytes((value, value, value))
            offset += 3
    path.write_bytes(
        f"P6\n{CHECKER_WIDTH} {CHECKER_HEIGHT}\n255\n".encode("ascii")
        + pixels
    )


def write_resize_reference(path: Path) -> None:
    """Write the analytical linear-light average of black and white."""
    linear = 0.5
    srgb = 1.055 * linear ** (1.0 / 2.4) - 0.055
    value = round(255.0 * srgb)
    pixels = bytes((value, value, value)) * (OUTPUT_WIDTH * OUTPUT_HEIGHT)
    path.write_bytes(
        f"P6\n{OUTPUT_WIDTH} {OUTPUT_HEIGHT}\n255\n".encode("ascii")
        + pixels
    )


def loader_option(kind: str) -> str:
    """Return the controlled loader option for one path case."""
    choices = {
        "plain": "builtin!",
        "cms-gamma-byte": (
            "builtin:cms_engine=builtin:cms_target=gamma:prefer_8bit=1!"
        ),
        "cms-gamma-float": (
            "builtin:cms_engine=builtin:cms_target=gamma:prefer_8bit=0!"
        ),
        "cms-linear-byte": (
            "builtin:cms_engine=builtin:cms_target=linear:prefer_8bit=1!"
        ),
        "cms-linear-float": (
            "builtin:cms_engine=builtin:cms_target=linear:prefer_8bit=0!"
        ),
        "cms-oklab-float": (
            "builtin:cms_engine=builtin:cms_target=oklab:prefer_8bit=0!"
        ),
    }
    return choices[kind]


def encoder_command(img2sixel: Path,
                    input_path: Path,
                    output_path: Path,
                    precision: str,
                    clustering: str,
                    working: str,
                    loader_kind: str) -> List[str]:
    """Build one fully controlled encoder command."""
    command = [
        str(img2sixel),
        "--threads=1",
        f"--precision={precision}",
        "--quality=full",
        f"--loaders={loader_option(loader_kind)}",
        "--sampling-policy=full-frame",
        "--binning-policy=hard",
        "--quantize-model=kmeans:seed=1:binbits=6",
        "-Fnone",
        "-aoff",
        f"-X{clustering}",
        f"-W{working}",
        "--diffusion=none",
        "--gpu-policy=off",
        "--lookup-policy=none",
        "--palette-type=rgb",
        "-p",
        str(PALETTE_COLORS),
        "-o",
        str(output_path),
        str(input_path),
    ]
    if loader_kind != "plain":
        command[4:4] = ["--cms-engine=builtin"]
    return command


def resize_command(img2sixel: Path,
                   input_path: Path,
                   output_path: Path,
                   override: str) -> List[str]:
    """Build one controlled resize-precision command."""
    command = encoder_command(
        img2sixel,
        input_path,
        output_path,
        "8bit",
        "gamma",
        "gamma",
        "plain",
    )
    resize_options = ["-w50%", "-rbilinear"]
    if override:
        resize_options.append(f"-jauto:resize_precision={override}")
    command[-1:-1] = resize_options
    return command


def run_preflight(command: Sequence[str],
                  environment: Dict[str, str]) -> Dict[str, str]:
    """Run a command once and parse stable effective-format diagnostics."""
    diagnostic_command = list(command[:-1]) + ["-v", command[-1]]
    trace_environment = environment.copy()
    trace_environment["SIXEL_TRACE_TOPIC"] = (
        "loader_contract,palette_contract,runtime_contract"
    )
    proc = subprocess.run(
        diagnostic_command,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.PIPE,
        env=trace_environment,
        check=False,
    )
    diagnostic = proc.stderr.decode("utf-8", errors="replace")
    if proc.returncode != 0:
        raise RuntimeError(
            f"Encoder preflight failed ({proc.returncode}):\n{diagnostic}"
        )
    format_match = re.findall(
        r"formats: source=(\S+) work=(\S+) scale_out=(\S+)",
        diagnostic,
    )
    resize_match = re.findall(r"resize: mode=(\d+) input=(\S+)", diagnostic)
    retries = [
        int(value)
        for value in re.findall(r"quantizer_retries=(\d+)", diagnostic)
    ]
    loader_match = re.findall(
        r"LSXLDR1\|cms_target=([^|]+)\|prefer_8bit=(\d+)"
        r"\|pixelformat=(\S+)",
        diagnostic,
    )
    if len(format_match) != 1 or len(resize_match) != 1:
        raise RuntimeError(
            "Encoder preflight did not expose one format and resize contract."
        )
    if not retries or any(value != 0 for value in retries):
        raise RuntimeError("Encoder preflight observed quantizer fallback.")
    source, work, scale_out = format_match[0]
    resize_mode, resize_input = resize_match[0]
    loader_target = ""
    loader_prefer_8bit = ""
    loader_pixelformat = ""
    if loader_match:
        loader_target, loader_prefer_8bit, loader_pixelformat = loader_match[-1]
    return {
        "source_format": source,
        "work_format": work,
        "scale_output_format": scale_out,
        "resize_mode": resize_mode,
        "resize_input_format": resize_input,
        "loader_cms_target": loader_target,
        "loader_prefer_8bit": loader_prefer_8bit,
        "loader_pixelformat": loader_pixelformat,
    }


def parse_quality(output: bytes) -> Dict[str, float]:
    """Parse the JSON object emitted by lsqa."""
    payload = json.loads(output.decode("utf-8"))
    quality = payload.get("quality")
    if not isinstance(quality, dict):
        raise RuntimeError("lsqa output has no quality object.")
    return {str(key): float(value) for key, value in quality.items()}


def assess(lsqa: Path,
           reference: Path,
           target: Path,
           environment: Dict[str, str]) -> Dict[str, float]:
    """Measure one quantized output in float32 Oklab comparison space."""
    command = [
        str(lsqa),
        "-Woklab",
        "-Pfloat32",
        str(reference),
        str(target),
    ]
    proc = subprocess.run(
        command,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=environment,
        check=False,
    )
    if proc.returncode != 0:
        diagnostic = proc.stderr.decode("utf-8", errors="replace")
        raise RuntimeError(f"lsqa failed ({proc.returncode}):\n{diagnostic}")
    return parse_quality(proc.stdout)


def command_template(command: Sequence[str],
                     img2sixel: Path,
                     input_path: Path,
                     output_path: Path) -> str:
    """Return a relocatable command string for checked-in CSV data."""
    replacements = {
        str(img2sixel): "{img2sixel}",
        str(input_path): "{input}",
        str(output_path): "{output}",
    }
    tokens = [replacements.get(token, token) for token in command]
    return shlex.join(tokens)


def write_csv(path: Path, rows: Sequence[Dict[str, object]]) -> None:
    """Write stable field order for one measurement table."""
    if not rows:
        raise ValueError(f"Refusing to write an empty CSV: {path}")
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(
            handle,
            fieldnames=list(rows[0]),
            lineterminator="\n",
        )
        writer.writeheader()
        writer.writerows(rows)


def plot_resize_quality(rows: Sequence[Dict[str, object]], path: Path) -> None:
    """Plot spatial and pointwise quality for the three resize paths."""
    labels = [str(row["label"]) for row in rows]
    positions = list(range(len(rows)))
    ms_ssim = [float(row["ms_ssim"]) for row in rows]
    delta_e = [float(row["delta_e00_mean"]) for row in rows]
    colors = ("#0072B2", "#D55E00", "#D55E00")
    markers = ("o", "s", "D")
    fills = ("#0072B2", "none", "#D55E00")

    fig, axes = plt.subplots(1, 2, figsize=(11.8, 5.5))
    fig.suptitle(
        "Linear-light resize avoids the RGB8 gamma averaging error",
        x=0.08,
        y=0.98,
        ha="left",
        fontsize=16,
        fontweight="bold",
    )
    fig.text(
        0.08,
        0.925,
        "600×450 one-pixel checker → 300×225, bilinear, K=256, "
        "dither off, one thread; reference is linear 50% gray (sRGB 188)",
        ha="left",
        fontsize=9.5,
        color="#555555",
    )

    for axis, values, title, better in (
        (axes[0], ms_ssim, "MS-SSIM", "higher is better"),
        (axes[1], delta_e, "Mean ΔE00", "lower is better"),
    ):
        axis.grid(axis="y", color="#DDDDDD", linewidth=0.8, zorder=0)
        for index, value in enumerate(values):
            axis.scatter(
                positions[index],
                value,
                s=82,
                marker=markers[index],
                edgecolor=colors[index],
                facecolor=fills[index],
                linewidth=1.8,
                zorder=3,
            )
            axis.annotate(
                f"{value:.6f}" if title == "MS-SSIM" else f"{value:.3f}",
                (positions[index], value),
                xytext=(0, 9),
                textcoords="offset points",
                ha="center",
                fontsize=8.5,
                color="#333333",
            )
        axis.set_title(f"{title}  ·  {better}", loc="left", fontsize=11)
        axis.set_xticks(positions, labels, rotation=16, ha="right")
        axis.tick_params(axis="both", labelsize=9)
        axis.spines["top"].set_visible(False)
        axis.spines["right"].set_visible(False)
    axes[0].set_ylim(min(ms_ssim) - 0.01, 1.002)
    axes[1].set_ylim(0.0, max(delta_e) * 1.15)
    fig.text(
        0.08,
        0.025,
        "Blue circle: byte-preserving resize. Orange square/diamond: "
        "linear float32 resize, transient or retained.",
        fontsize=9,
        color="#555555",
    )
    fig.tight_layout(rect=(0.06, 0.08, 0.98, 0.88))
    fig.savefig(path, dpi=180, facecolor="white")
    plt.close(fig)


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--img2sixel", required=True)
    parser.add_argument("--lsqa", required=True)
    parser.add_argument("--revision", required=True)
    parser.add_argument("--source-state", choices=("clean", "dirty"), required=True)
    parser.add_argument("--output-directory", required=True)
    return parser.parse_args()


def main() -> int:
    """Measure transition contracts, quality, and write all artifacts."""
    args = parse_args()
    source_root = Path(__file__).resolve().parent.parent
    img2sixel = Path(args.img2sixel).resolve()
    lsqa = Path(args.lsqa).resolve()
    output_directory = Path(args.output_directory).resolve()
    output_directory.mkdir(parents=True, exist_ok=True)
    environment = clean_environment()
    path_rows: List[Dict[str, object]] = []
    resize_rows: List[Dict[str, object]] = []

    with tempfile.TemporaryDirectory(prefix="libsixel-pixelformat-precision-") as name:
        temporary = Path(name)
        checker = temporary / "checker.ppm"
        reference = temporary / "linear-reference.ppm"
        write_checker_fixture(checker)
        write_resize_reference(reference)

        for case_id, label, precision, clustering, working, loader_kind in PATH_CASES:
            input_path = source_root / (
                CMS_INPUT if loader_kind != "plain" else PLAIN_INPUT
            )
            output_path = temporary / f"{case_id}.six"
            command = encoder_command(
                img2sixel,
                input_path,
                output_path,
                precision,
                clustering,
                working,
                loader_kind,
            )
            contract = run_preflight(command, environment)
            path_rows.append(
                {
                    "case": case_id,
                    "label": label,
                    "requested_precision": precision,
                    "clustering_colorspace": clustering,
                    "working_colorspace": working,
                    "loader_kind": loader_kind,
                    **contract,
                    "output_sha256": file_sha256(output_path),
                    "sixel_bytes": output_path.stat().st_size,
                    "command": command_template(
                        command,
                        img2sixel,
                        input_path,
                        output_path,
                    ),
                }
            )

        for mode, label, override in RESIZE_CASES:
            output_path = temporary / f"resize-{mode}.six"
            command = resize_command(
                img2sixel,
                checker,
                output_path,
                override,
            )
            contract = run_preflight(command, environment)
            quality = assess(lsqa, reference, output_path, environment)
            resize_rows.append(
                {
                    "mode": mode,
                    "label": label,
                    **contract,
                    "ms_ssim": f"{quality['MS-SSIM']:.6f}",
                    "delta_e00_mean": f"{quality['Δ E00_mean']:.6f}",
                    "delta_chroma_mean": f"{quality['Δ Chroma_mean']:.6f}",
                    "gmsd": f"{quality['GMSD']:.6f}",
                    "psnr_y": f"{quality['PSNR_Y']:.6f}",
                    "output_sha256": file_sha256(output_path),
                    "sixel_bytes": output_path.stat().st_size,
                    "command": command_template(
                        command,
                        img2sixel,
                        checker,
                        output_path,
                    ),
                    "assessment_command": (
                        "{lsqa} -Woklab -Pfloat32 {reference} {output}"
                    ),
                }
            )

    path_csv = output_directory / "pixelformat-precision-paths.csv"
    resize_csv = output_directory / "pixelformat-precision-resize.csv"
    plot_path = output_directory / "pixelformat-precision-resize.png"
    metadata_path = output_directory / "pixelformat-precision-run.json"
    write_csv(path_csv, path_rows)
    write_csv(resize_csv, resize_rows)
    plot_resize_quality(resize_rows, plot_path)
    metadata = {
        "schema_version": 1,
        "recorded_at_utc": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "source": {
            "revision": args.revision,
            "tracked_worktree_state_at_start": args.source_state,
            "plain_input": str(PLAIN_INPUT),
            "plain_input_sha256": file_sha256(source_root / PLAIN_INPUT),
            "cms_input": str(CMS_INPUT),
            "cms_input_sha256": file_sha256(source_root / CMS_INPUT),
        },
        "protocol": {
            "checker_size": [CHECKER_WIDTH, CHECKER_HEIGHT],
            "output_size": [OUTPUT_WIDTH, OUTPUT_HEIGHT],
            "reference_srgb_tone": 188,
            "resampling": "bilinear",
            "colors": PALETTE_COLORS,
            "dither": "none",
            "threads": 1,
            "comparison_colorspace": "oklab",
            "comparison_precision": "float32",
            "path_cases": [list(record) for record in PATH_CASES],
            "resize_cases": [list(record) for record in RESIZE_CASES],
            "sixel_environment_removed": True,
        },
        "host": {
            "platform": platform.platform(),
            "machine": platform.machine(),
            "python": platform.python_version(),
        },
        "programs": {
            "img2sixel": program_record(img2sixel),
            "lsqa": program_record(lsqa),
        },
        "artifacts": {
            "paths_csv": path_csv.name,
            "resize_csv": resize_csv.name,
            "plot": plot_path.name,
        },
    }
    metadata_path.write_text(
        json.dumps(metadata, indent=2, ensure_ascii=False) + "\n",
        encoding="utf-8",
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
