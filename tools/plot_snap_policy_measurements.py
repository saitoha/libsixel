#!/usr/bin/env python3
"""Measure and plot the palette snap policy and its tuning controls."""

from __future__ import annotations

import argparse
import csv
import datetime
import json
import os
import platform
import shlex
import shutil
import statistics
import subprocess
import tempfile
from pathlib import Path
from typing import Dict, List, Sequence, Tuple

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
    require_work_format,
    resolve_img2sixel,
    run_once,
)


COLORS = (8, 16, 32, 64, 128, 256)
HIGH_K_COLORS = tuple(range(200, 257, 2))
HIGH_K_SPEED_COLORS = (200, 208, 216, 224, 232, 240, 248, 256)
CONTROL_COLORS = 64
COLORSPACES: Tuple[Tuple[str, str, str], ...] = (
    ("gamma", "sRGB gamma", "rgb-f32"),
    ("linear", "linear RGB", "linear-f32"),
    ("oklab", "OKLab", "oklab-f32"),
    ("cielab", "CIELAB", "cielab-f32"),
    ("din99d", "DIN99d", "din99d-f32"),
)
MAIN_CONFIGS: Tuple[
    Tuple[str, str, bool, str, str, float, float], ...
] = (
    ("off", "snap off", False, "none", "off", 0.0, 0.85),
    (
        "nearest-quarter",
        "nearest / all / rate 0.25",
        True,
        "nearest",
        "all",
        0.25,
        0.85,
    ),
    (
        "nearest-once",
        "nearest / once / rate 1",
        True,
        "nearest",
        "once",
        1.0,
        0.85,
    ),
    (
        "nearest-all",
        "nearest / all / rate 1",
        True,
        "nearest",
        "all",
        1.0,
        0.85,
    ),
    (
        "reversible-all",
        "reversible / all / rate 1",
        True,
        "reversible",
        "all",
        1.0,
        0.85,
    ),
)
RATES = (0.0, 0.25, 0.5, 0.75, 1.0)
TIMINGS = ("once", "polish", "merge", "resolve", "all")
CHANNEL_FACTORS = (0.0, 0.25, 0.5, 0.75, 1.0)
LAB_COLORSPACES = ("oklab", "cielab", "din99d")
CONFIG_STYLES = {
    "off": ("#4D4D4D", "o", "--"),
    "nearest-quarter": ("#E69F00", "D", "-."),
    "nearest-once": ("#0072B2", "s", "-"),
    "nearest-all": ("#D55E00", "^", "-"),
    "reversible-all": ("#009E73", "v", ":"),
}
SPACE_STYLES = {
    "gamma": ("#0072B2", "o", "-"),
    "linear": ("#E69F00", "s", "--"),
    "oklab": ("#009E73", "D", "-"),
    "cielab": ("#7A5195", "^", "-."),
    "din99d": ("#CC79A7", "v", ":"),
}
QUALITY_FIELDS = (
    "MS-SSIM",
    "Delta E00_mean",
    "Delta Chroma_mean",
)


def resolve_lsqa(explicit: str | None, source_root: Path) -> str:
    """Resolve the lsqa executable."""
    candidates: List[str] = []
    if explicit:
        candidates.append(explicit)
    env_value = os.environ.get("LSQA_PATH")
    if env_value:
        candidates.append(env_value)
    candidates.append(str(source_root / "assessment" / "lsqa"))
    path_value = shutil.which("lsqa")
    if path_value:
        candidates.append(path_value)
    for candidate in candidates:
        path = Path(candidate)
        if path.is_file() and os.access(path, os.X_OK):
            return str(path.resolve())
        resolved = shutil.which(candidate)
        if resolved:
            return resolved
    raise FileNotFoundError("Could not find an executable lsqa.")


def load_fixtures(path: Path, source_root: Path) -> List[Dict[str, object]]:
    """Load and validate the fixed measurement fixture manifest."""
    with path.open("r", encoding="utf-8") as handle:
        payload = json.load(handle)
    if payload.get("schema_version") != 1:
        raise ValueError("Unsupported snap fixture manifest schema.")
    fixtures = payload.get("fixtures")
    if not isinstance(fixtures, list) or not fixtures:
        raise ValueError("Snap fixture manifest has no fixtures.")
    records: List[Dict[str, object]] = []
    seen = set()
    for raw in fixtures:
        if not isinstance(raw, dict):
            raise ValueError("Snap fixture record is not an object.")
        fixture_id = str(raw.get("id", ""))
        if not fixture_id or fixture_id in seen:
            raise ValueError(f"Invalid or repeated fixture id: {fixture_id}")
        seen.add(fixture_id)
        fixture_path = source_root / str(raw.get("path", ""))
        if not fixture_path.is_file():
            raise FileNotFoundError(f"Missing snap fixture: {fixture_path}")
        record = dict(raw)
        record["resolved_path"] = fixture_path.resolve()
        record["sha256"] = file_sha256(fixture_path)
        records.append(record)
    if records[0]["id"] != "natural-snake":
        raise ValueError("The first snap fixture must be natural-snake.")
    return records


def config_records() -> List[Dict[str, object]]:
    """Return the presentation manifest for the main configurations."""
    return [
        {
            "config": config,
            "label": label,
            "enabled": enabled,
            "policy": policy,
            "timing": timing,
            "rate": rate,
            "channel_l": channel_l,
        }
        for config, label, enabled, policy, timing, rate, channel_l
        in MAIN_CONFIGS
    ]


def format_number(value: float) -> str:
    """Format one option value without locale or redundant zeroes."""
    return f"{value:.8g}"


def make_command(
        img2sixel: str,
        input_image: Path,
        colorspace: str,
        colors: int,
        enabled: bool,
        policy: str,
        timing: str,
        rate: float,
        channel_l: float,
        merge_policy: str,
        discard_output: bool,
        palette_path: Path | None = None,
) -> List[str]:
    """Build one completely controlled snap-policy command."""
    command = [
        img2sixel,
        "--threads=1",
        "--precision=float32",
        "--quality=full",
        "--loaders=builtin!",
        "--sampling-policy=full-frame",
        "--binning-policy=hard",
        "--quantize-model=kmeans:seed=1:binbits=6",
        f"--merge-policy={merge_policy}",
        "--cover-policy=off",
        f"-X{colorspace}",
        f"-W{colorspace}",
        "--diffusion=fs:scan=raster",
        "--gpu-policy=off",
        "--lookup-policy=none",
        "--palette-type=rgb",
        "-p",
        str(colors),
    ]
    if enabled:
        command.append(
            f"--snap-policy={policy}:timing={timing}:"
            f"rate={format_number(rate)}:"
            f"channel_l={format_number(channel_l)}"
        )
    else:
        command.append("--snap-policy=none")
    if palette_path is not None:
        command.append(f"--mapfile-output=act:{palette_path}")
    if discard_output:
        command.extend(("-o", os.devnull))
    command.append(str(input_image))
    return command


def command_template(
        command: Sequence[str],
        executable: str,
        input_image: Path,
        palette_path: Path | None = None,
        executable_token: str = "{img2sixel}",
) -> str:
    """Make one recorded command independent of checkout paths."""
    text = shlex.join(command)
    text = text.replace(executable, executable_token, 1)
    text = text.replace(str(input_image), "{input}", 1)
    text = text.replace(os.devnull, "{devnull}")
    if palette_path is not None:
        text = text.replace(str(palette_path), "{palette}", 1)
    return text


def palette_statistics(path: Path) -> Dict[str, object]:
    """Measure fixed-point failures in an Adobe Color Table export."""
    data = path.read_bytes()
    if len(data) != 772:
        raise ValueError(f"Unexpected ACT palette length: {len(data)}")
    entry_count = int.from_bytes(data[768:770], "big")
    if entry_count == 0:
        entry_count = 256
    if entry_count < 1 or entry_count > 256:
        raise ValueError(f"Unexpected ACT palette count: {entry_count}")
    channels = data[:entry_count * 3]
    drifts = []
    unsafe_entries = 0
    for offset in range(0, len(channels), 3):
        entry_unsafe = False
        for value in channels[offset:offset + 3]:
            percentage = (100 * value + 127) // 255
            decoded = (255 * percentage + 50) // 100
            drift = abs(decoded - value)
            drifts.append(drift)
            entry_unsafe = entry_unsafe or drift != 0
        unsafe_entries += int(entry_unsafe)
    unsafe_channels = sum(drift != 0 for drift in drifts)
    return {
        "palette_entries": entry_count,
        "palette_channels": len(drifts),
        "unsafe_entries": unsafe_entries,
        "unsafe_channels": unsafe_channels,
        "unsafe_channel_fraction": unsafe_channels / len(drifts),
        "mean_abs_roundtrip_drift": statistics.fmean(drifts),
        "max_abs_roundtrip_drift": max(drifts),
    }


def assess_stream(
        encoded: bytes,
        lsqa: str,
        input_image: Path,
        command_env: Dict[str, str],
        prefix: Path,
) -> Tuple[Dict[str, float], List[str]]:
    """Assess one SIXEL stream and return the requested metrics."""
    lsqa_env = command_env.copy()
    for name in list(lsqa_env):
        if name.startswith("LSQA_"):
            del lsqa_env[name]
    lsqa_env["LSQA_PREFIX"] = str(prefix)
    lsqa_env["LSQA_VERBOSE"] = "0"
    command = [lsqa, "--loaders=builtin!", str(input_image), "-"]
    proc = subprocess.run(
        command,
        input=encoded,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=lsqa_env,
        check=False,
    )
    if proc.returncode != 0:
        diagnostic = proc.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(f"lsqa failed ({proc.returncode}): {diagnostic}")
    payload = json.loads(proc.stdout.decode("utf-8", errors="strict"))
    quality = payload.get("quality", payload)
    return (
        {
            "MS-SSIM": float(quality["MS-SSIM"]),
            "Delta E00_mean": float(quality["Δ E00_mean"]),
            "Delta Chroma_mean": float(quality["Δ Chroma_mean"]),
        },
        command,
    )


def measure_quality_point(
        command: Sequence[str],
        img2sixel: str,
        lsqa: str,
        input_image: Path,
        command_env: Dict[str, str],
        palette_path: Path,
        prefix: Path,
) -> Dict[str, object]:
    """Encode, assess, and inspect one palette."""
    proc = subprocess.run(
        list(command),
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=command_env,
        check=False,
    )
    if proc.returncode != 0:
        diagnostic = proc.stderr.decode("utf-8", errors="replace").strip()
        raise RuntimeError(
            f"Command failed ({proc.returncode}): "
            f"{shlex.join(command)}\n{diagnostic}"
        )
    metrics, assessment = assess_stream(
        proc.stdout,
        lsqa,
        input_image,
        command_env,
        prefix,
    )
    return {
        **metrics,
        **palette_statistics(palette_path),
        "encoded_bytes": len(proc.stdout),
        "command": command_template(
            command,
            img2sixel,
            input_image,
            palette_path,
        ),
        "assessment_command": command_template(
            assessment,
            lsqa,
            input_image,
            executable_token="{lsqa}",
        ),
    }


def measure_main_quality(
        img2sixel: str,
        lsqa: str,
        fixture: Dict[str, object],
        revision: str,
        command_env: Dict[str, str],
        temporary: Path,
        colors_values: Sequence[int] = COLORS,
        measurement_name: str = "main",
) -> List[Dict[str, object]]:
    """Measure one palette-size and colorspace comparison grid."""
    rows: List[Dict[str, object]] = []
    input_image = Path(fixture["resolved_path"])
    records = config_records()
    for colors in colors_values:
        print(f"{measurement_name} quality: K={colors}", flush=True)
        for colorspace, space_label, _work_format in COLORSPACES:
            for record in records:
                config = str(record["config"])
                palette = temporary / (
                    f"{measurement_name}-{colors}-{colorspace}-{config}.act"
                )
                command = make_command(
                    img2sixel,
                    input_image,
                    colorspace,
                    colors,
                    bool(record["enabled"]),
                    str(record["policy"]),
                    str(record["timing"]),
                    float(record["rate"]),
                    float(record["channel_l"]),
                    "none",
                    False,
                    palette,
                )
                result = measure_quality_point(
                    command,
                    img2sixel,
                    lsqa,
                    input_image,
                    command_env,
                    palette,
                    temporary / (
                        f"lsqa-{measurement_name}-{colors}-{colorspace}-"
                        f"{config}"
                    ),
                )
                rows.append(
                    {
                        "revision": revision,
                        "platform": platform.platform(),
                        "fixture_id": fixture["id"],
                        "fixture_label": fixture["label"],
                        "colors": colors,
                        "colorspace": colorspace,
                        "colorspace_label": space_label,
                        **record,
                        **result,
                    }
                )
    return rows


def measure_speed(
        img2sixel: str,
        fixture: Dict[str, object],
        revision: str,
        command_env: Dict[str, str],
        warmups: int,
        runs: int,
        colors_values: Sequence[int] = COLORS,
        measurement_name: str = "speed",
) -> List[Dict[str, object]]:
    """Measure fresh-process latency with rotating configuration order."""
    rows: List[Dict[str, object]] = []
    input_image = Path(fixture["resolved_path"])
    records = config_records()
    for colors in colors_values:
        print(f"{measurement_name}: K={colors}", flush=True)
        points = [
            (colorspace, space_label, record)
            for colorspace, space_label, _work_format in COLORSPACES
            for record in records
        ]
        commands = {}
        for colorspace, _space_label, record in points:
            key = (colorspace, str(record["config"]))
            commands[key] = make_command(
                img2sixel,
                input_image,
                colorspace,
                colors,
                bool(record["enabled"]),
                str(record["policy"]),
                str(record["timing"]),
                float(record["rate"]),
                float(record["channel_l"]),
                "none",
                True,
            )
        samples = {
            (colorspace, str(record["config"])): []
            for colorspace, _space_label, record in points
        }
        for round_index in range(warmups + runs):
            offset = (round_index * 7) % len(points)
            order = points[offset:] + points[:offset]
            for colorspace, _space_label, record in order:
                key = (colorspace, str(record["config"]))
                elapsed = run_once(commands[key], command_env)
                if round_index >= warmups:
                    samples[key].append(elapsed)
        baselines = {
            colorspace: statistics.median(samples[(colorspace, "off")])
            for colorspace, _label, _format in COLORSPACES
        }
        for colorspace, space_label, record in points:
            config = str(record["config"])
            values = samples[(colorspace, config)]
            median = statistics.median(values)
            rows.append(
                {
                    "revision": revision,
                    "platform": platform.platform(),
                    "fixture_id": fixture["id"],
                    "fixture_label": fixture["label"],
                    "colors": colors,
                    "colorspace": colorspace,
                    "colorspace_label": space_label,
                    **record,
                    "runs": runs,
                    "median_seconds": median,
                    "q1_seconds": percentile(values, 0.25),
                    "q3_seconds": percentile(values, 0.75),
                    "min_seconds": min(values),
                    "max_seconds": max(values),
                    "time_ratio_vs_off": median / baselines[colorspace],
                    "command": command_template(
                        commands[(colorspace, config)],
                        img2sixel,
                        input_image,
                    ),
                }
            )
    return rows


def measure_control_point(
        img2sixel: str,
        lsqa: str,
        fixture: Dict[str, object],
        colorspace: str,
        enabled: bool,
        policy: str,
        timing: str,
        rate: float,
        channel_l: float,
        merge_policy: str,
        revision: str,
        command_env: Dict[str, str],
        temporary: Path,
        identity: str,
) -> Dict[str, object]:
    """Measure one tuning-control point."""
    input_image = Path(fixture["resolved_path"])
    palette = temporary / f"control-{identity}.act"
    command = make_command(
        img2sixel,
        input_image,
        colorspace,
        CONTROL_COLORS,
        enabled,
        policy,
        timing,
        rate,
        channel_l,
        merge_policy,
        False,
        palette,
    )
    result = measure_quality_point(
        command,
        img2sixel,
        lsqa,
        input_image,
        command_env,
        palette,
        temporary / f"lsqa-control-{identity}",
    )
    return {
        "revision": revision,
        "platform": platform.platform(),
        "fixture_id": fixture["id"],
        "fixture_label": fixture["label"],
        "fixture_class": fixture["class"],
        "colors": CONTROL_COLORS,
        "colorspace": colorspace,
        "policy": policy,
        "timing": timing,
        "rate": rate,
        "channel_l": channel_l,
        "merge_policy": merge_policy,
        **result,
    }


def measure_controls(
        img2sixel: str,
        lsqa: str,
        fixtures: Sequence[Dict[str, object]],
        revision: str,
        command_env: Dict[str, str],
        temporary: Path,
) -> List[Dict[str, object]]:
    """Measure rate, timing, and lightness-weight control sweeps."""
    rows: List[Dict[str, object]] = []
    for fixture in fixtures:
        print(f"rate sweep: {fixture['id']}", flush=True)
        for colorspace, _label, _format in COLORSPACES:
            for rate in RATES:
                enabled = rate != 0.0
                result = measure_control_point(
                    img2sixel,
                    lsqa,
                    fixture,
                    colorspace,
                    enabled,
                    "nearest" if enabled else "none",
                    "all" if enabled else "off",
                    rate,
                    0.85,
                    "none",
                    revision,
                    command_env,
                    temporary,
                    f"rate-{fixture['id']}-{colorspace}-{format_number(rate)}",
                )
                rows.append(
                    {
                        "sweep": "rate",
                        "control_value": format_number(rate),
                        **result,
                    }
                )
    primary = fixtures[0]
    print("timing sweep", flush=True)
    for colorspace, _label, _format in COLORSPACES:
        baseline = measure_control_point(
            img2sixel,
            lsqa,
            primary,
            colorspace,
            False,
            "none",
            "off",
            0.0,
            0.85,
            "ward",
            revision,
            command_env,
            temporary,
            f"timing-{colorspace}-off",
        )
        rows.append({"sweep": "timing", "control_value": "off", **baseline})
        for timing in TIMINGS:
            result = measure_control_point(
                img2sixel,
                lsqa,
                primary,
                colorspace,
                True,
                "nearest",
                timing,
                1.0,
                0.85,
                "ward",
                revision,
                command_env,
                temporary,
                f"timing-{colorspace}-{timing}",
            )
            rows.append(
                {"sweep": "timing", "control_value": timing, **result}
            )
    print("channel_l sweep", flush=True)
    for colorspace in LAB_COLORSPACES:
        baseline = measure_control_point(
            img2sixel,
            lsqa,
            primary,
            colorspace,
            False,
            "none",
            "off",
            0.0,
            0.85,
            "none",
            revision,
            command_env,
            temporary,
            f"channel-{colorspace}-off",
        )
        rows.append(
            {"sweep": "channel_l", "control_value": "off", **baseline}
        )
        for factor in CHANNEL_FACTORS:
            result = measure_control_point(
                img2sixel,
                lsqa,
                primary,
                colorspace,
                True,
                "nearest",
                "all",
                1.0,
                factor,
                "none",
                revision,
                command_env,
                temporary,
                f"channel-{colorspace}-{format_number(factor)}",
            )
            rows.append(
                {
                    "sweep": "channel_l",
                    "control_value": format_number(factor),
                    **result,
                }
            )
    return rows


def write_csv(
        path: Path,
        rows: Sequence[Dict[str, object]],
        fields: Sequence[str],
) -> None:
    """Write one stable CSV table."""
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fields, lineterminator="\n")
        writer.writeheader()
        writer.writerows(rows)


def main_quality_fields() -> List[str]:
    """Return the main quality and palette CSV field order."""
    return [
        "revision", "platform", "fixture_id", "fixture_label", "colors",
        "colorspace", "colorspace_label", "config", "label", "enabled",
        "policy", "timing", "rate", "channel_l", *QUALITY_FIELDS,
        "palette_entries", "palette_channels", "unsafe_entries",
        "unsafe_channels", "unsafe_channel_fraction",
        "mean_abs_roundtrip_drift", "max_abs_roundtrip_drift",
        "encoded_bytes", "command", "assessment_command",
    ]


def speed_fields() -> List[str]:
    """Return the speed CSV field order."""
    return [
        "revision", "platform", "fixture_id", "fixture_label", "colors",
        "colorspace", "colorspace_label", "config", "label", "enabled",
        "policy", "timing", "rate", "channel_l", "runs",
        "median_seconds", "q1_seconds", "q3_seconds", "min_seconds",
        "max_seconds", "time_ratio_vs_off", "command",
    ]


def control_fields() -> List[str]:
    """Return the tuning-control CSV field order."""
    return [
        "revision", "platform", "sweep", "control_value", "fixture_id",
        "fixture_label", "fixture_class", "colors", "colorspace", "policy",
        "timing", "rate", "channel_l", "merge_policy", *QUALITY_FIELDS,
        "palette_entries", "palette_channels", "unsafe_entries",
        "unsafe_channels", "unsafe_channel_fraction",
        "mean_abs_roundtrip_drift", "max_abs_roundtrip_drift",
        "encoded_bytes", "command", "assessment_command",
    ]


def setup_axis(axis: plt.Axes) -> None:
    """Apply shared quiet chart scaffolding."""
    axis.grid(True, color="#D9D9D9", linewidth=0.7)
    axis.spines["top"].set_visible(False)
    axis.spines["right"].set_visible(False)


def plot_quality(path: Path, rows: Sequence[Dict[str, object]]) -> None:
    """Plot MS-SSIM and Delta E00 across palette sizes."""
    figure, axes = plt.subplots(5, 2, figsize=(12.0, 15.0), sharex=True)
    labels = {record["config"]: record["label"] for record in config_records()}
    for row_index, (space, space_label, _format) in enumerate(COLORSPACES):
        for config, _label, _enabled, _policy, _timing, _rate, _factor in MAIN_CONFIGS:
            selected = sorted(
                (
                    row for row in rows
                    if row["colorspace"] == space and row["config"] == config
                ),
                key=lambda row: int(row["colors"]),
            )
            color, marker, linestyle = CONFIG_STYLES[config]
            x = [int(row["colors"]) for row in selected]
            axes[row_index, 0].plot(
                x,
                [float(row["MS-SSIM"]) for row in selected],
                color=color,
                marker=marker,
                linestyle=linestyle,
                linewidth=1.7,
                markersize=4.5,
                label=labels[config],
            )
            axes[row_index, 1].plot(
                x,
                [float(row["Delta E00_mean"]) for row in selected],
                color=color,
                marker=marker,
                linestyle=linestyle,
                linewidth=1.7,
                markersize=4.5,
            )
        axes[row_index, 0].set_ylabel(f"{space_label}\nMS-SSIM")
        axes[row_index, 1].set_ylabel("Mean Delta E00")
        for axis in axes[row_index]:
            setup_axis(axis)
            axis.set_xscale("log", base=2)
            axis.set_xticks(COLORS)
            axis.get_xaxis().set_major_formatter(ticker.ScalarFormatter())
    axes[0, 0].set_title("Spatial similarity (higher is better)")
    axes[0, 1].set_title("Mean color error (lower is better)")
    axes[-1, 0].set_xlabel("Palette size K")
    axes[-1, 1].set_xlabel("Palette size K")
    handles, legend_labels = axes[0, 0].get_legend_handles_labels()
    figure.legend(
        handles,
        legend_labels,
        loc="upper center",
        ncol=3,
        frameon=False,
        bbox_to_anchor=(0.5, 0.975),
    )
    figure.suptitle("Snap policy quality by palette size", y=0.998, fontsize=15)
    figure.text(
        0.5,
        0.012,
        "Natural photograph; matched -W/-X; float32 K-means; "
        "Floyd--Steinberg; one decoded SIXEL stream per point",
        ha="center",
        fontsize=8.5,
        color="#555555",
    )
    figure.tight_layout(rect=(0.0, 0.03, 1.0, 0.945))
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=170)
    plt.close(figure)


def plot_small_multiples(
        path: Path,
        rows: Sequence[Dict[str, object]],
        value: str,
        title: str,
        ylabel: str,
        note: str,
) -> None:
    """Plot a relative main metric as one panel per colorspace."""
    figure, axes = plt.subplots(2, 3, figsize=(12.0, 7.2), sharex=True)
    flat = list(axes.flat)
    for axis_index, (space, space_label, _format) in enumerate(COLORSPACES):
        axis = flat[axis_index]
        baseline = {
            int(row["colors"]): row
            for row in rows
            if row["colorspace"] == space and row["config"] == "off"
        }
        for config, label, _enabled, _policy, _timing, _rate, _factor in MAIN_CONFIGS:
            if config == "off" and value == "encoded_bytes":
                continue
            selected = sorted(
                (
                    row for row in rows
                    if row["colorspace"] == space and row["config"] == config
                ),
                key=lambda row: int(row["colors"]),
            )
            color, marker, linestyle = CONFIG_STYLES[config]
            values = []
            for row in selected:
                colors = int(row["colors"])
                if value == "encoded_bytes":
                    values.append(
                        float(row[value]) / float(baseline[colors][value])
                    )
                elif value == "unsafe_channel_fraction":
                    values.append(100.0 * float(row[value]))
                else:
                    values.append(float(row[value]))
            axis.plot(
                [int(row["colors"]) for row in selected],
                values,
                color=color,
                marker=marker,
                linestyle=linestyle,
                linewidth=1.7,
                markersize=4.5,
                label=label,
            )
        if value == "encoded_bytes":
            axis.axhline(1.0, color="#4D4D4D", linewidth=1.0, linestyle="--")
        elif value == "unsafe_channel_fraction":
            axis.axhline(0.0, color="#4D4D4D", linewidth=1.0)
        axis.set_title(space_label)
        axis.set_xscale("log", base=2)
        axis.set_xticks(COLORS)
        axis.get_xaxis().set_major_formatter(ticker.ScalarFormatter())
        axis.set_xlabel("Palette size K")
        axis.set_ylabel(ylabel)
        setup_axis(axis)
    flat[-1].axis("off")
    handles, labels = flat[0].get_legend_handles_labels()
    flat[-1].legend(handles, labels, loc="center", frameon=False)
    figure.suptitle(title, fontsize=15)
    figure.text(0.5, 0.012, note, ha="center", fontsize=8.5, color="#555555")
    figure.tight_layout(rect=(0.0, 0.035, 1.0, 0.955))
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=170)
    plt.close(figure)


def plot_speed(path: Path, rows: Sequence[Dict[str, object]], runs: int) -> None:
    """Plot end-to-end time ratios with interquartile bars."""
    figure, axes = plt.subplots(2, 3, figsize=(12.0, 7.2), sharex=True)
    flat = list(axes.flat)
    for axis_index, (space, space_label, _format) in enumerate(COLORSPACES):
        axis = flat[axis_index]
        baseline = {
            int(row["colors"]): float(row["median_seconds"])
            for row in rows
            if row["colorspace"] == space and row["config"] == "off"
        }
        for config, label, _enabled, _policy, _timing, _rate, _factor in MAIN_CONFIGS:
            if config == "off":
                continue
            selected = sorted(
                (
                    row for row in rows
                    if row["colorspace"] == space and row["config"] == config
                ),
                key=lambda row: int(row["colors"]),
            )
            ratios = [
                float(row["median_seconds"]) / baseline[int(row["colors"])]
                for row in selected
            ]
            lower = [
                ratio - float(row["q1_seconds"])
                / baseline[int(row["colors"])]
                for ratio, row in zip(ratios, selected)
            ]
            upper = [
                float(row["q3_seconds"])
                / baseline[int(row["colors"])] - ratio
                for ratio, row in zip(ratios, selected)
            ]
            color, marker, linestyle = CONFIG_STYLES[config]
            axis.errorbar(
                [int(row["colors"]) for row in selected],
                ratios,
                yerr=[lower, upper],
                color=color,
                marker=marker,
                linestyle=linestyle,
                linewidth=1.7,
                markersize=4.5,
                capsize=2.0,
                label=label,
            )
        axis.axhline(1.0, color="#4D4D4D", linewidth=1.0, linestyle="--")
        axis.set_title(space_label)
        axis.set_xscale("log", base=2)
        axis.set_xticks(COLORS)
        axis.get_xaxis().set_major_formatter(ticker.ScalarFormatter())
        axis.set_xlabel("Palette size K")
        axis.set_ylabel("Time / snap-off time")
        setup_axis(axis)
    flat[-1].axis("off")
    handles, labels = flat[0].get_legend_handles_labels()
    flat[-1].legend(handles, labels, loc="center", frameon=False)
    figure.suptitle("Snap policy end-to-end latency", fontsize=15)
    figure.text(
        0.5,
        0.012,
        f"Natural photograph; fresh process per sample; median of {runs}; "
        "bars show IQR; lower is better",
        ha="center",
        fontsize=8.5,
        color="#555555",
    )
    figure.tight_layout(rect=(0.0, 0.035, 1.0, 0.955))
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=170)
    plt.close(figure)


def plot_high_k_quality(
        path: Path,
        rows: Sequence[Dict[str, object]],
) -> None:
    """Plot small quality deltas in the 200--256 color interval."""
    figure, axes = plt.subplots(
        4,
        2,
        figsize=(12.0, 11.5),
        sharex=True,
        sharey="col",
    )
    labels = {record["config"]: record["label"] for record in config_records()}
    baselines = {
        (str(row["colorspace"]), int(row["colors"])): row
        for row in rows
        if row["config"] == "off"
    }
    enabled_configs = [
        config for config, _label, _enabled, _policy, _timing, _rate, _factor
        in MAIN_CONFIGS if config != "off"
    ]
    for row_index, config in enumerate(enabled_configs):
        for space, space_label, _format in COLORSPACES:
            selected = sorted(
                (
                    row for row in rows
                    if row["colorspace"] == space
                    and row["config"] == config
                ),
                key=lambda row: int(row["colors"]),
            )
            x = [int(row["colors"]) for row in selected]
            ms_ssim = [
                1000.0 * (
                    float(row["MS-SSIM"])
                    - float(baselines[(space, int(row["colors"]))]["MS-SSIM"])
                )
                for row in selected
            ]
            delta_e00 = [
                float(row["Delta E00_mean"])
                - float(
                    baselines[(space, int(row["colors"]))]["Delta E00_mean"]
                )
                for row in selected
            ]
            color, marker, linestyle = SPACE_STYLES[space]
            axes[row_index, 0].plot(
                x,
                ms_ssim,
                color=color,
                marker=marker,
                markevery=4,
                linestyle=linestyle,
                linewidth=1.6,
                markersize=4.5,
                label=space_label,
            )
            axes[row_index, 1].plot(
                x,
                delta_e00,
                color=color,
                marker=marker,
                markevery=4,
                linestyle=linestyle,
                linewidth=1.6,
                markersize=4.5,
            )
        axes[row_index, 0].set_ylabel(
            f"{labels[config]}\nDelta MS-SSIM x 1,000"
        )
        axes[row_index, 1].set_ylabel("Delta mean Delta E00")
        for axis in axes[row_index]:
            axis.axhline(0.0, color="#4D4D4D", linewidth=1.0)
            axis.set_xticks(HIGH_K_SPEED_COLORS)
            setup_axis(axis)
    axes[0, 0].set_title("Spatial similarity change (higher is better)")
    axes[0, 1].set_title("Mean color-error change (lower is better)")
    axes[-1, 0].set_xlabel("Palette size K")
    axes[-1, 1].set_xlabel("Palette size K")
    handles, legend_labels = axes[0, 0].get_legend_handles_labels()
    legend = figure.legend(
        handles,
        legend_labels,
        loc="upper center",
        ncol=5,
        frameon=False,
        bbox_to_anchor=(0.5, 0.965),
    )
    legend.set_in_layout(False)
    figure.suptitle(
        "Snap policy quality in the high-K interval",
        y=0.997,
        fontsize=15,
    )
    figure.text(
        0.5,
        0.012,
        "K=200--256 in steps of 2; each value is relative to snap off at "
        "the same K and color space",
        ha="center",
        fontsize=8.5,
        color="#555555",
    )
    figure.tight_layout(rect=(0.0, 0.035, 1.0, 0.92))
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=170)
    plt.close(figure)


def plot_high_k_size_speed(
        path: Path,
        quality_rows: Sequence[Dict[str, object]],
        speed_rows: Sequence[Dict[str, object]],
        runs: int,
) -> None:
    """Plot stream-size and latency deltas in the high-K interval."""
    figure, axes = plt.subplots(
        4,
        2,
        figsize=(12.0, 11.5),
        sharex="col",
        sharey="col",
    )
    labels = {record["config"]: record["label"] for record in config_records()}
    size_baselines = {
        (str(row["colorspace"]), int(row["colors"])): row
        for row in quality_rows
        if row["config"] == "off"
    }
    speed_baselines = {
        (str(row["colorspace"]), int(row["colors"])):
            float(row["median_seconds"])
        for row in speed_rows
        if row["config"] == "off"
    }
    enabled_configs = [
        config for config, _label, _enabled, _policy, _timing, _rate, _factor
        in MAIN_CONFIGS if config != "off"
    ]
    for row_index, config in enumerate(enabled_configs):
        for space, space_label, _format in COLORSPACES:
            selected_quality = sorted(
                (
                    row for row in quality_rows
                    if row["colorspace"] == space
                    and row["config"] == config
                ),
                key=lambda row: int(row["colors"]),
            )
            size_x = [int(row["colors"]) for row in selected_quality]
            size_delta = [
                100.0 * (
                    float(row["encoded_bytes"])
                    / float(
                        size_baselines[
                            (space, int(row["colors"]))
                        ]["encoded_bytes"]
                    )
                    - 1.0
                )
                for row in selected_quality
            ]
            selected_speed = sorted(
                (
                    row for row in speed_rows
                    if row["colorspace"] == space
                    and row["config"] == config
                ),
                key=lambda row: int(row["colors"]),
            )
            speed_x = [int(row["colors"]) for row in selected_speed]
            speed_delta = []
            speed_lower = []
            speed_upper = []
            for row in selected_speed:
                baseline = speed_baselines[(space, int(row["colors"]))]
                median = 100.0 * (float(row["median_seconds"]) / baseline - 1.0)
                q1 = 100.0 * (float(row["q1_seconds"]) / baseline - 1.0)
                q3 = 100.0 * (float(row["q3_seconds"]) / baseline - 1.0)
                speed_delta.append(median)
                speed_lower.append(median - q1)
                speed_upper.append(q3 - median)
            color, marker, linestyle = SPACE_STYLES[space]
            axes[row_index, 0].plot(
                size_x,
                size_delta,
                color=color,
                marker=marker,
                markevery=4,
                linestyle=linestyle,
                linewidth=1.6,
                markersize=4.5,
                label=space_label,
            )
            axes[row_index, 1].errorbar(
                speed_x,
                speed_delta,
                yerr=[speed_lower, speed_upper],
                color=color,
                marker=marker,
                linestyle=linestyle,
                linewidth=1.6,
                markersize=4.5,
                capsize=2.0,
            )
        axes[row_index, 0].set_ylabel(
            f"{labels[config]}\nEncoded bytes change (%)"
        )
        axes[row_index, 1].set_ylabel("Wall-time change (%)")
        for axis in axes[row_index]:
            axis.axhline(0.0, color="#4D4D4D", linewidth=1.0)
            axis.set_xticks(HIGH_K_SPEED_COLORS)
            setup_axis(axis)
    axes[0, 0].set_title("SIXEL stream-size change (lower is smaller)")
    axes[0, 1].set_title("End-to-end latency change (lower is faster)")
    axes[-1, 0].set_xlabel("Palette size K")
    axes[-1, 1].set_xlabel("Palette size K")
    handles, legend_labels = axes[0, 0].get_legend_handles_labels()
    legend = figure.legend(
        handles,
        legend_labels,
        loc="upper center",
        ncol=5,
        frameon=False,
        bbox_to_anchor=(0.5, 0.965),
    )
    legend.set_in_layout(False)
    figure.suptitle(
        "Snap policy size and speed in the high-K interval",
        y=0.997,
        fontsize=15,
    )
    figure.text(
        0.5,
        0.012,
        f"Size: K=200--256 in steps of 2; speed: K=200--256 in steps "
        f"of 8, median of {runs}, IQR bars; all values are relative to snap off",
        ha="center",
        fontsize=8.5,
        color="#555555",
    )
    figure.tight_layout(rect=(0.0, 0.035, 1.0, 0.92))
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=170)
    plt.close(figure)


def baseline_for(
        rows: Sequence[Dict[str, object]],
        sweep: str,
        fixture_id: str,
        colorspace: str,
) -> Dict[str, object]:
    """Return the unique disabled baseline for one control group."""
    baseline_value = "0" if sweep == "rate" else "off"
    matches = [
        row for row in rows
        if row["sweep"] == sweep
        and row["fixture_id"] == fixture_id
        and row["colorspace"] == colorspace
        and row["control_value"] == baseline_value
    ]
    if len(matches) != 1:
        raise ValueError(
            f"Expected one {sweep}/{fixture_id}/{colorspace} baseline."
        )
    return matches[0]


def plot_controls(
        path: Path,
        rows: Sequence[Dict[str, object]],
        fixtures: Sequence[Dict[str, object]],
) -> None:
    """Plot rate, fixture sensitivity, timing, and channel weight."""
    figure, axes = plt.subplots(2, 2, figsize=(13.0, 9.5))
    primary_id = str(fixtures[0]["id"])

    rate_axis = axes[0, 0]
    fixed_axis = axes[0, 1]
    for space, space_label, _format in COLORSPACES:
        selected = sorted(
            (
                row for row in rows
                if row["sweep"] == "rate"
                and row["fixture_id"] == primary_id
                and row["colorspace"] == space
            ),
            key=lambda row: float(row["control_value"]),
        )
        baseline = baseline_for(rows, "rate", primary_id, space)
        x = [float(row["control_value"]) for row in selected]
        color, marker, linestyle = SPACE_STYLES[space]
        rate_axis.plot(
            x,
            [
                1000.0
                * (float(row["MS-SSIM"]) - float(baseline["MS-SSIM"]))
                for row in selected
            ],
            color=color,
            marker=marker,
            linestyle=linestyle,
            linewidth=1.8,
            label=space_label,
        )
        fixed_axis.plot(
            x,
            [100.0 * float(row["unsafe_channel_fraction"]) for row in selected],
            color=color,
            marker=marker,
            linestyle=linestyle,
            linewidth=1.8,
            label=space_label,
        )
    rate_axis.axhline(0.0, color="#4D4D4D", linewidth=1.0)
    rate_axis.set_title("Approach rate: decoded-image quality")
    rate_axis.set_xlabel("Approach rate")
    rate_axis.set_ylabel("Delta MS-SSIM vs snap off (x 1,000)")
    fixed_axis.set_title("Approach rate: palette fixed points")
    fixed_axis.set_xlabel("Approach rate")
    fixed_axis.set_ylabel("Unsafe exported palette channels (%)")
    for axis in (rate_axis, fixed_axis):
        axis.set_xticks(RATES)
        setup_axis(axis)
    rate_axis.legend(frameon=False, ncol=2, fontsize=8)

    timing_axis = axes[1, 0]
    timing_order = ("off", *TIMINGS)
    for space, space_label, _format in COLORSPACES:
        selected = {
            str(row["control_value"]): row
            for row in rows
            if row["sweep"] == "timing"
            and row["fixture_id"] == primary_id
            and row["colorspace"] == space
        }
        baseline = selected["off"]
        color, marker, linestyle = SPACE_STYLES[space]
        timing_axis.plot(
            range(len(timing_order)),
            [
                1000.0
                * (
                    float(selected[value]["MS-SSIM"])
                    - float(baseline["MS-SSIM"])
                )
                for value in timing_order
            ],
            color=color,
            marker=marker,
            linestyle=linestyle,
            linewidth=1.8,
            label=space_label,
        )
    timing_axis.axhline(0.0, color="#4D4D4D", linewidth=1.0)
    timing_axis.set_title("Timing ladder at exact rate 1")
    timing_axis.set_xticks(range(len(timing_order)), timing_order, rotation=20)
    timing_axis.set_ylabel("Delta MS-SSIM vs snap off (x 1,000)")
    setup_axis(timing_axis)

    channel_axis = axes[1, 1]
    for space in LAB_COLORSPACES:
        space_label = next(
            label for name, label, _format in COLORSPACES if name == space
        )
        selected = sorted(
            (
                row for row in rows
                if row["sweep"] == "channel_l"
                and row["colorspace"] == space
                and row["control_value"] != "off"
            ),
            key=lambda row: float(row["control_value"]),
        )
        baseline = baseline_for(rows, "channel_l", primary_id, space)
        color, marker, linestyle = SPACE_STYLES[space]
        channel_axis.plot(
            [float(row["control_value"]) for row in selected],
            [
                1000.0
                * (float(row["MS-SSIM"]) - float(baseline["MS-SSIM"]))
                for row in selected
            ],
            color=color,
            marker=marker,
            linestyle=linestyle,
            linewidth=1.8,
            label=space_label,
        )
    channel_axis.axhline(0.0, color="#4D4D4D", linewidth=1.0)
    channel_axis.set_title("Lab-family lightness weight at exact rate 1")
    channel_axis.set_xlabel("channel_l")
    channel_axis.set_ylabel("Delta MS-SSIM vs snap off (x 1,000)")
    channel_axis.set_xticks(CHANNEL_FACTORS)
    setup_axis(channel_axis)
    channel_axis.legend(frameon=False, fontsize=8)

    figure.suptitle("Snap policy tuning controls at K=64", fontsize=15)
    figure.text(
        0.5,
        0.012,
        "Natural photograph; matched -W/-X; float32 K-means; "
        "Floyd--Steinberg; timing sweep uses Ward merge",
        ha="center",
        fontsize=8.5,
        color="#555555",
    )
    figure.tight_layout(rect=(0.0, 0.035, 1.0, 0.955))
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=170)
    plt.close(figure)


def plot_fixture_summary(
        path: Path,
        rows: Sequence[Dict[str, object]],
        fixtures: Sequence[Dict[str, object]],
) -> None:
    """Plot the best measured rate for every fixture and colorspace."""
    matrix: List[List[float]] = []
    best_rates: List[List[float]] = []
    for fixture in fixtures:
        delta_row = []
        rate_row = []
        fixture_id = str(fixture["id"])
        for space, _label, _format in COLORSPACES:
            baseline = baseline_for(rows, "rate", fixture_id, space)
            candidates = [
                row for row in rows
                if row["sweep"] == "rate"
                and row["fixture_id"] == fixture_id
                and row["colorspace"] == space
                and float(row["control_value"]) > 0.0
            ]
            best = max(candidates, key=lambda row: float(row["MS-SSIM"]))
            delta_row.append(
                1000.0
                * (float(best["MS-SSIM"]) - float(baseline["MS-SSIM"]))
            )
            rate_row.append(float(best["control_value"]))
        matrix.append(delta_row)
        best_rates.append(rate_row)
    maximum = max(abs(value) for row in matrix for value in row)
    maximum = max(maximum, 0.001)
    figure, axis = plt.subplots(figsize=(9.5, 5.8))
    image = axis.imshow(
        matrix,
        cmap="coolwarm_r",
        vmin=-maximum,
        vmax=maximum,
        aspect="auto",
    )
    axis.set_xticks(
        range(len(COLORSPACES)),
        [label for _name, label, _format in COLORSPACES],
    )
    axis.set_yticks(
        range(len(fixtures)),
        [str(fixture["label"]) for fixture in fixtures],
    )
    for row_index, delta_row in enumerate(matrix):
        for column_index, value in enumerate(delta_row):
            color = "white" if abs(value) > maximum * 0.55 else "#222222"
            axis.text(
                column_index,
                row_index,
                f"{value:+.2f}\nrate {best_rates[row_index][column_index]:.2g}",
                ha="center",
                va="center",
                fontsize=8,
                color=color,
            )
    colorbar = figure.colorbar(image, ax=axis, shrink=0.88)
    colorbar.set_label("Best Delta MS-SSIM vs snap off (x 1,000)")
    axis.set_title("Best measured partial or exact snap rate by fixture")
    figure.text(
        0.5,
        0.012,
        "K=64; candidate rates 0.25, 0.5, 0.75, and 1; "
        "cell labels show the selected rate",
        ha="center",
        fontsize=8.5,
        color="#555555",
    )
    figure.tight_layout(rect=(0.0, 0.035, 1.0, 1.0))
    path.parent.mkdir(parents=True, exist_ok=True)
    figure.savefig(path, dpi=170)
    plt.close(figure)


def write_metadata(
        path: Path,
        source_root: Path,
        manifest: Path,
        fixtures: Sequence[Dict[str, object]],
        img2sixel: str,
        lsqa: str,
        build_dir: Path,
        revision: str,
        source_state: str,
        mode: str,
        warmups: int,
        runs: int,
        main_count: int,
        speed_count: int,
        control_count: int,
        high_k_count: int,
        high_k_speed_count: int,
) -> None:
    """Record complete provenance and protocol metadata."""
    fixture_records = []
    for fixture in fixtures:
        fixture_records.append(
            {
                name: fixture[name]
                for name in (
                    "id", "label", "class", "path", "width", "height",
                    "sha256",
                )
            }
        )
    payload = {
        "schema_version": 2,
        "generated_at_utc": datetime.datetime.now(
            datetime.timezone.utc
        ).isoformat(),
        "measurement_mode": mode,
        "source": {
            "revision": revision,
            "tracked_worktree_state_at_start": source_state,
            "platform": platform.platform(),
            "machine": platform.machine(),
            "python": platform.python_version(),
            "manifest": display_path(manifest, source_root),
            "manifest_sha256": file_sha256(manifest),
        },
        "programs": {
            "img2sixel": program_record(img2sixel, source_root),
            "lsqa": program_record(lsqa, source_root),
        },
        "build": read_build_configuration(build_dir),
        "fixtures": fixture_records,
        "protocol": {
            "primary_fixture": "natural-snake",
            "colors": list(COLORS),
            "high_k_colors": list(HIGH_K_COLORS),
            "high_k_speed_colors": list(HIGH_K_SPEED_COLORS),
            "control_colors": CONTROL_COLORS,
            "colorspaces": [
                {
                    "name": name,
                    "label": label,
                    "working_format": work_format,
                }
                for name, label, work_format in COLORSPACES
            ],
            "configurations": config_records(),
            "threads": 1,
            "precision": "float32",
            "quality": "full",
            "loader": "builtin!",
            "sampling_policy": "full-frame",
            "binning_policy": "hard",
            "quantize_model": "kmeans:seed=1:binbits=6",
            "main_merge_policy": "none",
            "cover_policy": "off",
            "diffusion": "fs:scan=raster",
            "lookup_policy": "none",
            "gpu_policy": "off",
            "palette_type": "rgb",
            "matched_working_and_clustering_spaces": True,
            "rate_sweep": list(RATES),
            "timing_sweep": list(TIMINGS),
            "timing_merge_policy": "ward",
            "channel_l_sweep": list(CHANNEL_FACTORS),
            "channel_l_colorspaces": list(LAB_COLORSPACES),
            "speed_warmups": warmups,
            "speed_runs": runs,
            "speed_order_rotated_each_round": True,
            "sixel_environment_removed": True,
            "size_measurement": "quality-pass SIXEL stdout byte length",
            "fixed_point_measurement": (
                "ACT palette channels changed by one SIXEL percentage "
                "encode/decode round trip"
            ),
            "timing_measurement": (
                "uninstrumented fresh-process monotonic wall time"
            ),
        },
        "artifacts": {
            "main_row_count": main_count,
            "speed_row_count": speed_count,
            "control_row_count": control_count,
            "high_k_row_count": high_k_count,
            "high_k_speed_row_count": high_k_speed_count,
            "tables": [
                "snap-policy-quality.csv",
                "snap-policy-speed.csv",
                "snap-policy-controls.csv",
                "snap-policy-high-k.csv",
                "snap-policy-high-k-speed.csv",
            ],
            "plots": [
                "snap-policy-quality.png",
                "snap-policy-fixed-point.png",
                "snap-policy-size.png",
                "snap-policy-speed.png",
                "snap-policy-controls.png",
                "snap-policy-fixture-summary.png",
                "snap-policy-high-k-quality.png",
                "snap-policy-high-k-size-speed.png",
            ],
        },
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8") as handle:
        json.dump(payload, handle, indent=2, sort_keys=True)
        handle.write("\n")


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", required=True)
    parser.add_argument("--output-dir", required=True)
    parser.add_argument("--img2sixel")
    parser.add_argument("--lsqa")
    parser.add_argument("--build-dir", default=".")
    parser.add_argument("--revision", default="unknown")
    parser.add_argument("--source-state", choices=("clean", "dirty"), required=True)
    parser.add_argument(
        "--measurement-mode",
        choices=("durable", "exploratory"),
        required=True,
    )
    parser.add_argument("--clean-sixel-environment", action="store_true")
    parser.add_argument("--warmups", type=int, default=2)
    parser.add_argument("--runs", type=int, default=9)
    return parser.parse_args()


def main() -> int:
    """Run the complete measurement and plotting workflow."""
    args = parse_args()
    if args.warmups < 0 or args.runs < 1:
        raise ValueError("Warm-ups must be nonnegative and runs must be positive.")
    source_root = Path(__file__).resolve().parent.parent
    manifest = Path(args.manifest).resolve()
    output_dir = Path(args.output_dir).resolve()
    build_dir = Path(args.build_dir).resolve()
    fixtures = load_fixtures(manifest, source_root)
    img2sixel = resolve_img2sixel(args.img2sixel, source_root)
    lsqa = resolve_lsqa(args.lsqa, source_root)
    command_env = make_command_environment(args.clean_sixel_environment)
    primary = fixtures[0]

    for space, _label, work_format in COLORSPACES:
        preflight = make_command(
            img2sixel,
            Path(primary["resolved_path"]),
            space,
            CONTROL_COLORS,
            False,
            "off",
            "off",
            0.0,
            0.85,
            "none",
            True,
        )
        require_work_format(preflight, command_env, work_format)

    with tempfile.TemporaryDirectory(prefix="libsixel-snap-") as directory:
        temporary = Path(directory)
        main_rows = measure_main_quality(
            img2sixel,
            lsqa,
            primary,
            args.revision,
            command_env,
            temporary,
        )
        speed_rows = measure_speed(
            img2sixel,
            primary,
            args.revision,
            command_env,
            args.warmups,
            args.runs,
        )
        high_k_rows = measure_main_quality(
            img2sixel,
            lsqa,
            primary,
            args.revision,
            command_env,
            temporary,
            HIGH_K_COLORS,
            "high-K",
        )
        high_k_speed_rows = measure_speed(
            img2sixel,
            primary,
            args.revision,
            command_env,
            args.warmups,
            args.runs,
            HIGH_K_SPEED_COLORS,
            "high-K speed",
        )
        control_rows = measure_controls(
            img2sixel,
            lsqa,
            fixtures,
            args.revision,
            command_env,
            temporary,
        )

    quality_path = output_dir / "snap-policy-quality.csv"
    speed_path = output_dir / "snap-policy-speed.csv"
    controls_path = output_dir / "snap-policy-controls.csv"
    high_k_path = output_dir / "snap-policy-high-k.csv"
    high_k_speed_path = output_dir / "snap-policy-high-k-speed.csv"
    write_csv(quality_path, main_rows, main_quality_fields())
    write_csv(speed_path, speed_rows, speed_fields())
    write_csv(controls_path, control_rows, control_fields())
    write_csv(high_k_path, high_k_rows, main_quality_fields())
    write_csv(high_k_speed_path, high_k_speed_rows, speed_fields())
    plot_quality(output_dir / "snap-policy-quality.png", main_rows)
    plot_small_multiples(
        output_dir / "snap-policy-fixed-point.png",
        main_rows,
        "unsafe_channel_fraction",
        "Snap policy palette fixed-point failures",
        "Unsafe exported palette channels (%)",
        "ACT export inspected before SIXEL percentage conversion; zero is reversible",
    )
    plot_small_multiples(
        output_dir / "snap-policy-size.png",
        main_rows,
        "encoded_bytes",
        "Snap policy encoded SIXEL size",
        "Bytes / snap-off bytes",
        "Exact byte length of the stream used for quality assessment; lower is smaller",
    )
    plot_speed(output_dir / "snap-policy-speed.png", speed_rows, args.runs)
    plot_controls(output_dir / "snap-policy-controls.png", control_rows, fixtures)
    plot_fixture_summary(
        output_dir / "snap-policy-fixture-summary.png",
        control_rows,
        fixtures,
    )
    plot_high_k_quality(
        output_dir / "snap-policy-high-k-quality.png",
        high_k_rows,
    )
    plot_high_k_size_speed(
        output_dir / "snap-policy-high-k-size-speed.png",
        high_k_rows,
        high_k_speed_rows,
        args.runs,
    )
    write_metadata(
        output_dir / "snap-policy-run.json",
        source_root,
        manifest,
        fixtures,
        img2sixel,
        lsqa,
        build_dir,
        args.revision,
        args.source_state,
        args.measurement_mode,
        args.warmups,
        args.runs,
        len(main_rows),
        len(speed_rows),
        len(control_rows),
        len(high_k_rows),
        len(high_k_speed_rows),
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
