#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""Plot quality metrics versus palette size using render commands + lsqa.

This helper evaluates one image or an image set by sweeping color counts.
For every (image, colors) pair it runs:

  img2sixel -p <colors> <image> [| optional postprocessors]
    -> lsqa <image> -

Then it extracts requested metrics, writes a CSV table, and renders a PNG
line chart where x-axis is color count and y-axis is metric value.
"""

from __future__ import annotations

import argparse
import concurrent.futures
import csv
import json
import math
import os
import shlex
import shutil
import statistics
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Dict, List, Sequence, Tuple

import matplotlib.pyplot as plt
from matplotlib import ticker


METRIC_ALIASES = {
    "MS-SSIM": "MS-SSIM",
    "MS_SSIM": "MS-SSIM",
    "SSIM": "MS-SSIM",
}

DEFAULT_COMMAND_TEMPLATE = "{img2sixel} -p {ncolors}"


def split_command_template(template: str) -> List[str]:
    """Split a command template while preserving pipe separators."""
    lexer = shlex.shlex(template, posix=True, punctuation_chars="|")
    lexer.commenters = ""
    lexer.whitespace_split = True
    try:
        return list(lexer)
    except ValueError as exc:
        raise ValueError(f"Invalid render template: {template}") from exc


def format_command_pipeline(pipeline: Sequence[Sequence[str]]) -> str:
    """Return a shell-like string for diagnostics only."""
    return " | ".join(shlex.join(stage) for stage in pipeline)


def parse_list_argument(raw: str) -> List[str]:
    """Parse comma-separated tokens into a cleaned list."""
    values = [token.strip() for token in raw.split(",")]
    values = [token for token in values if token]
    if not values:
        raise ValueError("Empty list is not allowed.")
    return values


def parse_colors(raw: str) -> List[int]:
    """Parse and validate color counts."""
    colors = [int(token) for token in parse_list_argument(raw)]
    for value in colors:
        if value <= 0:
            raise ValueError(f"Color count must be positive: {value}")
    dedup_sorted = sorted(set(colors))
    return dedup_sorted


def parse_color_range(minimum: int, maximum: int, step: int) -> List[int]:
    """Build color counts from min/max/step."""
    if step == 0:
        raise ValueError("Color step must not be zero.")
    if minimum <= 0 or maximum <= 0:
        raise ValueError("Color min/max must be positive.")
    if step > 0 and minimum > maximum:
        raise ValueError("colors-min must be <= colors-max when step is positive.")
    if step < 0 and minimum < maximum:
        raise ValueError("colors-min must be >= colors-max when step is negative.")

    stop = maximum + (1 if step > 0 else -1)
    values = list(range(minimum, stop, step))
    if not values:
        raise ValueError("Color range is empty.")
    return values


def normalize_metric_name(name: str) -> str:
    """Normalize metric aliases accepted by lsqa."""
    metric = name.strip()
    canonical = METRIC_ALIASES.get(metric.upper(), metric)
    return canonical


def parse_metrics(raw: str) -> List[str]:
    """Parse and normalize metric names."""
    metrics = [normalize_metric_name(token) for token in parse_list_argument(raw)]
    deduped: List[str] = []
    seen = set()
    for name in metrics:
        if name not in seen:
            seen.add(name)
            deduped.append(name)
    return deduped


def read_images_from_file(path: Path) -> List[str]:
    """Read image paths from a text file."""
    images: List[str] = []
    with path.open("r", encoding="utf-8") as handle:
        for line in handle:
            stripped = line.strip()
            if not stripped or stripped.startswith("#"):
                continue
            images.append(stripped)
    return images


def resolve_binary(explicit: str | None,
                   env_name: str,
                   command_name: str,
                   candidates: Sequence[Path]) -> str:
    """Resolve a runnable executable path."""
    search: List[str] = []

    if explicit:
        search.append(explicit)

    env_value = os.environ.get(env_name)
    if env_value:
        search.append(env_value)

    for candidate in candidates:
        search.append(str(candidate))

    path_hit = shutil.which(command_name)
    if path_hit:
        search.append(path_hit)

    for item in search:
        if not item:
            continue
        direct = Path(item)
        if direct.is_file() and os.access(direct, os.X_OK):
            return str(direct)
        which_hit = shutil.which(item)
        if which_hit:
            return which_hit

    tried = ", ".join(search) if search else "(none)"
    raise FileNotFoundError(
        f"Failed to find executable for {command_name}. Tried: {tried}"
    )


def run_command(cmd: Sequence[str],
                stdin_data: bytes | None = None,
                env: Dict[str, str] | None = None) -> bytes:
    """Run a subprocess and return stdout bytes."""
    proc = subprocess.run(
        list(cmd),
        input=stdin_data,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
        env=env,
        check=False,
    )
    if proc.returncode != 0:
        stderr_text = proc.stderr.decode("utf-8", errors="replace").strip()
        cmd_text = " ".join(cmd)
        raise RuntimeError(
            f"Command failed ({proc.returncode}): {cmd_text}\n{stderr_text}"
        )
    return proc.stdout


def run_command_pipeline(pipeline: Sequence[Sequence[str]],
                         env: Dict[str, str] | None = None) -> bytes:
    """Run a simple pipe-separated command pipeline and return stdout bytes."""
    if not pipeline:
        raise ValueError("Command pipeline must not be empty.")

    if len(pipeline) == 1:
        return run_command(pipeline[0], env=env)

    processes: List[subprocess.Popen[bytes]] = []
    previous_stdout = None
    with tempfile.TemporaryFile() as stderr_file:
        try:
            for cmd in pipeline:
                stdin = previous_stdout
                proc = subprocess.Popen(
                    list(cmd),
                    stdin=stdin,
                    stdout=subprocess.PIPE,
                    stderr=stderr_file,
                    env=env,
                )
                if previous_stdout is not None:
                    previous_stdout.close()
                previous_stdout = proc.stdout
                processes.append(proc)
        except Exception:
            if previous_stdout is not None:
                previous_stdout.close()
            for proc in processes:
                proc.kill()
            for proc in processes:
                proc.wait()
            raise

        output, _stderr = processes[-1].communicate()
        for proc in processes[:-1]:
            proc.wait()

        failed = [proc.returncode for proc in processes if proc.returncode != 0]
        if failed:
            stderr_file.seek(0)
            stderr_text = stderr_file.read().decode(
                "utf-8",
                errors="replace",
            ).strip()
            cmd_text = format_command_pipeline(pipeline)
            first_failure = failed[0]
            raise RuntimeError(
                f"Command failed ({first_failure}): {cmd_text}\n{stderr_text}"
            )

    return output


def build_render_pipeline(template: str,
                          image_path: str,
                          ncolors: int,
                          img2sixel_path: str | None,
                          sixel2png_path: str | None) -> List[List[str]]:
    """Build the render command pipeline from the user template.

    Supported placeholders:
      {ncolors}   requested color count
      {input}     input image path
      {image}     alias of {input}
      {img2sixel} resolved img2sixel binary path
      {sixel2png} resolved sixel2png binary path

    When the template omits {input}/{image}, append the image path to the
    first stage so pipelines such as "img2sixel | sixel2png -D" keep the same
    implicit-input behavior as a single render command.
    """
    mapping = {
        "ncolors": str(ncolors),
        "input": image_path,
        "image": image_path,
    }
    if img2sixel_path is not None:
        mapping["img2sixel"] = img2sixel_path
    if sixel2png_path is not None:
        mapping["sixel2png"] = sixel2png_path

    raw_parts = split_command_template(template)
    if not raw_parts:
        raise ValueError("Render template must not be empty.")

    pipeline: List[List[str]] = [[]]
    for token in raw_parts:
        if token == "|":
            if not pipeline[-1]:
                raise ValueError("Pipe cannot appear before a command.")
            pipeline.append([])
            continue
        if "|" in token:
            raise ValueError("Only simple pipe separators are supported.")
        try:
            rendered = token.format_map(mapping)
        except KeyError as exc:
            raise ValueError(
                f"Unknown placeholder in render template: {exc.args[0]}"
            ) from exc
        pipeline[-1].append(rendered)

    if not pipeline[-1]:
        raise ValueError("Pipe cannot appear after a command.")

    if img2sixel_path is not None:
        for stage in pipeline:
            if stage[0] == "img2sixel":
                stage[0] = img2sixel_path
    if sixel2png_path is not None:
        for stage in pipeline:
            if stage[0] == "sixel2png":
                stage[0] = sixel2png_path

    if "{input}" not in template and "{image}" not in template:
        pipeline[0].append(image_path)

    return pipeline


def evaluate_one_case(image_path: str,
                      colors: int,
                      render_template: str,
                      img2sixel_path: str | None,
                      sixel2png_path: str | None,
                      lsqa_path: str,
                      lsqa_opts: Sequence[str]) -> Dict[str, float]:
    """Evaluate all lsqa metrics for one image at one color count."""
    render_pipeline = build_render_pipeline(
        template=render_template,
        image_path=image_path,
        ncolors=colors,
        img2sixel_path=img2sixel_path,
        sixel2png_path=sixel2png_path,
    )
    candidate_bytes = run_command_pipeline(render_pipeline)
    with tempfile.TemporaryDirectory(prefix="lsqa-curve-") as tmp_dir:
        lsqa_env = os.environ.copy()
        lsqa_env["LSQA_PREFIX"] = str(Path(tmp_dir) / "curve")
        lsqa_env["LSQA_VERBOSE"] = "0"
        metrics_json = run_command(
            [lsqa_path, *lsqa_opts, image_path, "-"],
            stdin_data=candidate_bytes,
            env=lsqa_env,
        )

    try:
        decoded = metrics_json.decode("utf-8", errors="strict")
        payload = json.loads(decoded)
    except Exception as exc:
        msg = metrics_json.decode("utf-8", errors="replace")
        raise RuntimeError(f"lsqa output is not valid JSON:\n{msg}") from exc

    if not isinstance(payload, dict):
        raise RuntimeError("lsqa JSON payload is not an object.")

    # Newer lsqa payloads wrap the metric dictionary under "quality".
    if "quality" in payload:
        quality = payload.get("quality")
        if not isinstance(quality, dict):
            raise RuntimeError("lsqa JSON field 'quality' is not an object.")
        return quality

    return payload


def template_needs_command(template: str,
                           placeholder: str,
                           command_name: str) -> bool:
    """Return True when template uses a command that can be auto-resolved."""
    if placeholder in template:
        return True
    try:
        parts = split_command_template(template)
    except Exception:
        return False
    if not parts:
        return False
    command_position = True
    for token in parts:
        if token == "|":
            command_position = True
            continue
        if command_position and token == command_name:
            return True
        command_position = False
    return False


def template_needs_img2sixel(template: str) -> bool:
    """Return True when template uses img2sixel auto resolution."""
    return template_needs_command(template, "{img2sixel}", "img2sixel")


def template_needs_sixel2png(template: str) -> bool:
    """Return True when template uses sixel2png auto resolution."""
    return template_needs_command(template, "{sixel2png}", "sixel2png")


def collect_command_templates(args: argparse.Namespace) -> List[Tuple[str, str]]:
    """Collect command templates from --command1 ... --command16 options."""
    commands: List[Tuple[str, str]] = []
    for index in range(1, 17):
        value = getattr(args, f"command{index}")
        if value:
            commands.append((f"command{index}", value))

    if commands:
        return commands

    # Default path when commandN options are not provided.
    return [("command1", DEFAULT_COMMAND_TEMPLATE)]


def command_index(command_name: str) -> int:
    """Return numeric index from command name like command7."""
    if command_name.startswith("command"):
        suffix = command_name[len("command"):]
        if suffix.isdigit():
            return int(suffix)
    return 0


def aggregate_values(values: Sequence[float], mode: str) -> float:
    """Aggregate non-empty values with the requested reducer."""
    if mode == "mean":
        return statistics.fmean(values)
    if mode == "median":
        return statistics.median(values)
    if mode == "min":
        return min(values)
    if mode == "max":
        return max(values)
    raise ValueError(f"Unknown aggregate mode: {mode}")


def write_csv(path: Path,
              rows: Sequence[Dict[str, object]],
              metrics: Sequence[str]) -> None:
    """Write evaluation rows as a wide CSV table."""
    fieldnames = ["kind", "command", "template", "image", "colors", *metrics]
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        for row in rows:
            writer.writerow(row)


def can_use_log2_axis(colors: Sequence[int]) -> bool:
    """Return True if all color counts are powers of two."""
    for color in colors:
        if color <= 0 or (color & (color - 1)) != 0:
            return False
    return True


def sorted_xy(rows: Sequence[Dict[str, object]],
              metric: str) -> tuple[List[int], List[float]]:
    """Extract sorted x/y vectors from row dictionaries."""
    points = []
    for row in rows:
        value = row.get(metric)
        if value is None:
            continue
        if isinstance(value, float) and math.isnan(value):
            continue
        points.append((int(row["colors"]), float(value)))
    points.sort(key=lambda item: item[0])
    x = [p[0] for p in points]
    y = [p[1] for p in points]
    return x, y


def plot_metrics(path: Path,
                 rows_image: Sequence[Dict[str, object]],
                 rows_aggregate: Sequence[Dict[str, object]],
                 command_templates: Sequence[Tuple[str, str]],
                 metrics: Sequence[str],
                 colors: Sequence[int],
                 title: str,
                 show_per_image: bool,
                 aggregate_mode: str) -> None:
    """Render a PNG chart for requested metrics."""
    n_axes = len(metrics)
    figure, axes = plt.subplots(
        n_axes, 1, figsize=(10, 3.6 * n_axes), sharex=True
    )
    if n_axes == 1:
        axes = [axes]

    images = sorted({str(row["image"]) for row in rows_image})
    commands = [name for name, _template in command_templates]
    command_labels = {
        name: template for name, template in command_templates
    }

    # Prefer aggregate rows for image-set comparison.  When aggregate rows are
    # absent and multiple images exist, fall back to the first image for the
    # main command lines while optional per-image traces can still be shown.
    if rows_aggregate:
        rows_main = list(rows_aggregate)
    elif len(images) <= 1:
        rows_main = list(rows_image)
    else:
        rows_main = [
            row for row in rows_image if str(row["image"]) == images[0]
        ]

    use_log2 = can_use_log2_axis(colors)

    for axis, metric in zip(axes, metrics):
        if show_per_image and len(images) > 1:
            for command_name in commands:
                for image in images:
                    rows_trace = [
                        row for row in rows_image
                        if str(row["command"]) == command_name
                        and str(row["image"]) == image
                    ]
                    x_trace, y_trace = sorted_xy(rows_trace, metric)
                    if x_trace:
                        axis.plot(
                            x_trace,
                            y_trace,
                            linewidth=0.9,
                            alpha=0.20,
                            linestyle="--",
                        )

        for command_name in commands:
            rows_command = [
                row for row in rows_main if str(row["command"]) == command_name
            ]
            x_cmd, y_cmd = sorted_xy(rows_command, metric)
            if not x_cmd:
                continue
            command_label = command_labels.get(command_name, command_name)
            if rows_aggregate and len(images) > 1:
                label = f"{command_label} ({aggregate_mode})"
            else:
                label = command_label
            axis.plot(
                x_cmd,
                y_cmd,
                linewidth=2.0,
                label=label,
            )

        axis.set_ylabel(metric)
        axis.grid(True, alpha=0.25)
        if use_log2:
            axis.set_xscale("log", base=2)
            axis.xaxis.set_major_formatter(ticker.ScalarFormatter())
            axis.set_xticks(colors)

        handles, labels = axis.get_legend_handles_labels()
        if handles:
            axis.legend(loc="best", fontsize=8)

    axes[-1].set_xlabel("Number of Colors")
    if title:
        figure.suptitle(title)
        figure.tight_layout(rect=[0.0, 0.0, 1.0, 0.97])
    else:
        figure.tight_layout()
    figure.savefig(path, dpi=140)
    plt.close(figure)


def main() -> int:
    parser = argparse.ArgumentParser(
        description=(
            "Sweep palette sizes and plot lsqa metrics against color count."
        )
    )
    parser.add_argument(
        "images",
        nargs="*",
        help="Input image paths. For an image set, pass multiple paths.",
    )
    parser.add_argument(
        "--images-file",
        help="Text file containing image paths (one path per line).",
    )
    parser.add_argument(
        "--colors",
        default=None,
        help=(
            "Comma-separated color counts. Mutually exclusive with"
            " --colors-min/--colors-max/--colors-step."
        ),
    )
    parser.add_argument(
        "--colors-min",
        type=int,
        help="Minimum color count (range mode).",
    )
    parser.add_argument(
        "--colors-max",
        type=int,
        help="Maximum color count (range mode).",
    )
    parser.add_argument(
        "--colors-step",
        type=int,
        help="Step size for color count range mode.",
    )
    parser.add_argument(
        "--metrics",
        default="MS-SSIM",
        help="Comma-separated lsqa metric keys (default: MS-SSIM).",
    )
    parser.add_argument(
        "--aggregate",
        choices=["mean", "median", "min", "max"],
        default="mean",
        help="Aggregation used for image set summary line.",
    )
    parser.add_argument(
        "--no-aggregate",
        action="store_true",
        help="Do not compute aggregate rows for image sets.",
    )
    parser.add_argument(
        "--hide-per-image",
        action="store_true",
        help="Plot only aggregate lines.",
    )
    parser.add_argument(
        "--img2sixel",
        help="Path to img2sixel binary (auto-detected if omitted).",
    )
    parser.add_argument(
        "--sixel2png",
        help="Path to sixel2png binary (auto-detected if omitted).",
    )
    for index in range(1, 17):
        parser.add_argument(
            f"--command{index}",
            dest=f"command{index}",
            help=(
                f"Render command template #{index}. "
                "Placeholders: {ncolors}, {input}/{image}, {img2sixel}, "
                "{sixel2png}. "
                "Use | to add postprocessing stages."
            ),
        )
    parser.add_argument(
        "--lsqa",
        help="Path to lsqa binary (auto-detected if omitted).",
    )
    parser.add_argument(
        "--lsqa-opt",
        action="append",
        default=[],
        help="Extra option passed to lsqa (repeatable).",
    )
    parser.add_argument(
        "--output-csv",
        default="quality_curve.csv",
        help="Output CSV path (default: quality_curve.csv).",
    )
    parser.add_argument(
        "--output-plot",
        default="quality_curve.png",
        help="Output PNG path (default: quality_curve.png).",
    )
    parser.add_argument(
        "--title",
        default="",
        help="Optional chart title.",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Print progress logs to stderr.",
    )
    parser.add_argument(
        "--jobs",
        type=int,
        default=0,
        help=(
            "Worker count for parallel evaluation."
            " Default is auto (CPU count). Use 1 for serial."
        ),
    )
    args = parser.parse_args()

    image_paths: List[str] = list(args.images)
    if args.images_file:
        image_paths.extend(read_images_from_file(Path(args.images_file)))

    if not image_paths:
        parser.error("At least one image is required.")

    for path in image_paths:
        if not Path(path).is_file():
            parser.error(f"Image not found: {path}")

    if (
        args.colors_min is not None
        or args.colors_max is not None
        or args.colors_step is not None
    ):
        if args.colors is not None:
            parser.error(
                "--colors and --colors-min/--colors-max/--colors-step"
                " cannot be used together."
            )
        if (
            args.colors_min is None
            or args.colors_max is None
            or args.colors_step is None
        ):
            parser.error(
                "Range mode requires --colors-min, --colors-max,"
                " and --colors-step."
            )
        try:
            colors = parse_color_range(
                args.colors_min,
                args.colors_max,
                args.colors_step,
            )
        except ValueError as exc:
            parser.error(str(exc))
    elif args.colors is not None:
        try:
            colors = parse_colors(args.colors)
        except ValueError as exc:
            parser.error(str(exc))
    else:
        colors = parse_colors("8,16,32,64,128,256")

    metrics = parse_metrics(args.metrics)
    command_templates = collect_command_templates(args)

    repo_root = Path(__file__).resolve().parent.parent
    img2sixel_path: str | None = None
    sixel2png_path: str | None = None
    if any(
        template_needs_img2sixel(template)
        for _name, template in command_templates
    ):
        img2sixel_path = resolve_binary(
            args.img2sixel,
            "IMG2SIXEL_PATH",
            "img2sixel",
            (
                repo_root / "builddir" / "converters" / "img2sixel",
                repo_root / "build-master" / "converters" / "img2sixel",
                repo_root / "build-autotools-current" / "converters" / "img2sixel",
            ),
        )
    if any(
        template_needs_sixel2png(template)
        for _name, template in command_templates
    ):
        sixel2png_path = resolve_binary(
            args.sixel2png,
            "SIXEL2PNG_PATH",
            "sixel2png",
            (
                repo_root / "builddir" / "converters" / "sixel2png",
                repo_root / "build-master" / "converters" / "sixel2png",
                repo_root / "build-autotools-current" / "converters" / "sixel2png",
            ),
        )
    lsqa_path = resolve_binary(
        args.lsqa,
        "LSQA_PATH",
        "lsqa",
        (
            repo_root / "builddir" / "assessment" / "lsqa",
            repo_root / "build-master" / "assessment" / "lsqa",
            repo_root / "build-autotools-current" / "assessment" / "lsqa",
        ),
    )

    tasks: List[Tuple[str, str, str, int]] = []
    for command_name, command_template in command_templates:
        for image in image_paths:
            for color in colors:
                tasks.append((command_name, command_template, image, color))

    total = len(tasks)
    if args.jobs < 0:
        parser.error("--jobs must be >= 0.")
    if args.jobs == 0:
        worker_count = os.cpu_count() or 1
    else:
        worker_count = args.jobs
    if worker_count > total:
        worker_count = total
    if worker_count <= 0:
        worker_count = 1

    rows_image: List[Dict[str, object]] = []
    progress = 0
    if worker_count == 1:
        for command_name, command_template, image, color in tasks:
            progress += 1
            if args.verbose:
                print(
                    f"[{progress}/{total}] {command_name}:"
                    f" {Path(image).name} @ {color} colors",
                    file=sys.stderr,
                )
            payload = evaluate_one_case(
                image_path=image,
                colors=color,
                render_template=command_template,
                img2sixel_path=img2sixel_path,
                sixel2png_path=sixel2png_path,
                lsqa_path=lsqa_path,
                lsqa_opts=args.lsqa_opt,
            )
            row: Dict[str, object] = {
                "kind": "image",
                "command": command_name,
                "template": command_template,
                "image": image,
                "colors": color,
            }
            for metric in metrics:
                if metric not in payload:
                    raise RuntimeError(
                        f"Metric key not found in lsqa output: {metric}"
                    )
                value = payload[metric]
                row[metric] = float(value) if value is not None else float("nan")
            rows_image.append(row)
    else:
        with concurrent.futures.ThreadPoolExecutor(
            max_workers=worker_count
        ) as executor:
            future_map: Dict[
                concurrent.futures.Future[Dict[str, float]],
                Tuple[str, str, str, int],
            ] = {}
            for command_name, command_template, image, color in tasks:
                future = executor.submit(
                    evaluate_one_case,
                    image_path=image,
                    colors=color,
                    render_template=command_template,
                    img2sixel_path=img2sixel_path,
                    sixel2png_path=sixel2png_path,
                    lsqa_path=lsqa_path,
                    lsqa_opts=args.lsqa_opt,
                )
                future_map[future] = (command_name, command_template, image, color)

            for future in concurrent.futures.as_completed(future_map):
                command_name, command_template, image, color = future_map[future]
                progress += 1
                if args.verbose:
                    print(
                        f"[{progress}/{total}] {command_name}:"
                        f" {Path(image).name} @ {color} colors",
                        file=sys.stderr,
                    )
                payload = future.result()
                row = {
                    "kind": "image",
                    "command": command_name,
                    "template": command_template,
                    "image": image,
                    "colors": color,
                }
                for metric in metrics:
                    if metric not in payload:
                        raise RuntimeError(
                            f"Metric key not found in lsqa output: {metric}"
                        )
                    value = payload[metric]
                    row[metric] = float(value) if value is not None else float("nan")
                rows_image.append(row)

    rows_image.sort(
        key=lambda row: (
            command_index(str(row["command"])),
            str(row["image"]),
            int(row["colors"]),
        )
    )

    rows_aggregate: List[Dict[str, object]] = []
    if not args.no_aggregate and len(image_paths) >= 2:
        for command_name, command_template in command_templates:
            for color in colors:
                row = {
                    "kind": "aggregate",
                    "command": command_name,
                    "template": command_template,
                    "image": f"aggregate:{args.aggregate}",
                    "colors": color,
                }
                subset = [
                    r
                    for r in rows_image
                    if str(r["command"]) == command_name
                    and int(r["colors"]) == color
                ]
                for metric in metrics:
                    values = [
                        float(r[metric])
                        for r in subset
                        if not math.isnan(float(r[metric]))
                    ]
                    row[metric] = (
                        aggregate_values(values, args.aggregate)
                        if values
                        else float("nan")
                    )
                rows_aggregate.append(row)

    all_rows = rows_image + rows_aggregate
    csv_path = Path(args.output_csv)
    write_csv(csv_path, all_rows, metrics)

    plot_path = Path(args.output_plot)
    plot_metrics(
        path=plot_path,
        rows_image=rows_image,
        rows_aggregate=rows_aggregate,
        command_templates=command_templates,
        metrics=metrics,
        colors=colors,
        title=args.title,
        show_per_image=not args.hide_per_image,
        aggregate_mode=args.aggregate,
    )

    print(f"Wrote: {csv_path}", file=sys.stderr)
    print(f"Wrote: {plot_path}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
