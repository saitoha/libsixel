#!/usr/bin/env python3
"""Measure thread-budget allocation and record representative timelines."""

from __future__ import annotations

import argparse
import csv
import datetime
import json
import os
import platform
import re
import shlex
import shutil
import statistics
import subprocess
import tempfile
from pathlib import Path
from typing import Dict, Iterable, List, Sequence, Tuple

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt

from plot_lookup_policy_speed import (
    display_path,
    file_sha256,
    make_command_environment,
    program_record,
    read_build_configuration,
)


DEFAULT_THREADS = tuple(range(1, 13))
DEFAULT_DECODER_SCALING_THREADS = tuple(range(1, 17))
DEFAULT_DECODER_WARMUPS = 2
DEFAULT_DECODER_REPEATS = 9
DECODER_SIZES = (
    ("900x675", 900, 675),
    ("1920x1080", 1920, 1080),
)
PLANNER_PATTERN = re.compile(
    r"band_height=(\d+) overlap=(\d+) threads: dither=(\d+) encode=(\d+)"
)
MODE_PATTERN = re.compile(r"bands=(\d+) queue=(\d+) mode=(serial|pipeline)")


def parse_threads(value: str) -> Tuple[int, ...]:
    """Parse an ordered list of unique positive thread counts."""
    values = tuple(int(item) for item in value.split(","))
    if not values or any(item < 1 for item in values):
        raise ValueError("thread counts must be positive")
    if len(values) != len(set(values)):
        raise ValueError("thread counts must not contain duplicates")
    return values


def run_checked(command: Sequence[str], env: Dict[str, str]) \
        -> subprocess.CompletedProcess[bytes]:
    """Run one command and include its diagnostic on failure."""
    proc = subprocess.run(
        list(command),
        stdout=subprocess.DEVNULL,
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
    return proc


def encoder_policy_arguments(threads: int) -> List[str]:
    """Return the controlled choices shared by static encoder fixtures."""
    return [
        f"--threads={threads}",
        "--precision=8bit",
        "--quality=full",
        "--sampling-policy=full-frame",
        "--binning-policy=hard",
        "--quantize-model=kmeans:seed=1",
        "--merge-policy=ward",
        "-Xoklab",
        "-Wgamma",
        "--diffusion=fs:scan=raster",
        "--gpu-policy=off",
        "--lookup-policy=6bit:shared_instance=1",
        "-p",
        "256",
    ]


def encoder_command(img2sixel: str,
                    input_path: Path,
                    threads: int,
                    output_path: Path,
                    log_path: Path) -> List[str]:
    """Build the controlled static encoder command."""
    return [
        img2sixel,
        *encoder_policy_arguments(threads),
        "-v",
        "-J",
        str(log_path),
        "-o",
        str(output_path),
        str(input_path),
    ]


def decoder_fixture_command(img2sixel: str,
                            input_path: Path,
                            output_path: Path,
                            width: int,
                            height: int) -> List[str]:
    """Build a deterministic near-display-sized decoder fixture."""
    return [
        img2sixel,
        *encoder_policy_arguments(1),
        "-w",
        str(width),
        "-h",
        str(height),
        "-o",
        str(output_path),
        str(input_path),
    ]


def animation_command(img2sixel: str,
                      input_path: Path,
                      threads: int,
                      output_path: Path,
                      log_path: Path) -> List[str]:
    """Build a finite two-frame animation command."""
    command = encoder_command(
        img2sixel,
        input_path,
        threads,
        output_path,
        log_path,
    )
    command[command.index("-p"):command.index("-p") + 2] = ["-p", "16"]
    command[command.index("-v"):command.index("-v") + 1] = ["-g", "-w", "1200"]
    return command


def decoder_command(sixel2png: str,
                    input_path: Path,
                    threads: int,
                    log_path: Path) -> List[str]:
    """Build the controlled decoder command."""
    return [
        sixel2png,
        f"--threads={threads}",
        "--gpu-policy=off",
        "-J",
        str(log_path),
        "-i",
        str(input_path),
        "-o",
        os.devnull,
    ]


def load_records(path: Path) -> List[Dict[str, object]]:
    """Read one JSONL timeline."""
    records: List[Dict[str, object]] = []
    with path.open("r", encoding="utf-8") as handle:
        for line in handle:
            if line.strip():
                records.append(json.loads(line))
    if not records:
        raise RuntimeError(f"timeline is empty: {path}")
    return records


def event_intervals(records: Iterable[Dict[str, object]],
                    start_event: str,
                    finish_event: str,
                    worker: str,
                    role: str) -> List[Tuple[float, float]]:
    """Pair matching worker events by session, thread, and job."""
    starts: Dict[Tuple[int, int, int], List[float]] = {}
    intervals: List[Tuple[float, float]] = []
    for record in sorted(records, key=lambda item: float(item["ts"])):
        if record.get("worker") != worker or record.get("role") != role:
            continue
        key = (
            int(record.get("session_id", 0)),
            int(record.get("thread", -1)),
            int(record.get("job", -1)),
        )
        event = str(record.get("event", ""))
        if event == start_event:
            starts.setdefault(key, []).append(float(record["ts"]))
        elif event == finish_event and starts.get(key):
            intervals.append((starts[key].pop(0), float(record["ts"])))
    return intervals


def phase_wall_seconds(records: Sequence[Dict[str, object]],
                       worker: str,
                       role: str) -> float:
    """Return the wall interval enclosing one paired timeline phase."""
    intervals = event_intervals(
        records,
        "start",
        "finish",
        worker,
        role,
    )
    if not intervals:
        raise RuntimeError(f"timeline omitted {worker}/{role} phase")
    return max(end for _, end in intervals) - min(
        start for start, _ in intervals
    )


def optional_phase_wall_seconds(records: Sequence[Dict[str, object]],
                                worker: str,
                                role: str) -> object:
    """Return a phase wall interval, or an empty CSV field when absent."""
    intervals = event_intervals(
        records,
        "start",
        "finish",
        worker,
        role,
    )
    if not intervals:
        return ""
    return max(end for _, end in intervals) - min(
        start for start, _ in intervals
    )


def decoder_measurement_row(identifier: str,
                            width: int,
                            height: int,
                            revision: str,
                            sixel_path: Path,
                            log_path: Path,
                            command: Sequence[str],
                            replacements: Dict[str, str]) \
        -> Dict[str, object]:
    """Summarize one decoder timeline without treating it as a benchmark."""
    records = load_records(log_path)
    decoder_seconds = phase_wall_seconds(records, "io", "decoder")
    scan_seconds = phase_wall_seconds(records, "decoder", "scan")
    paint_seconds = phase_wall_seconds(records, "decoder", "paint")
    return {
        "revision": revision,
        "fixture": identifier,
        "width": width,
        "height": height,
        "pixels": width * height,
        "sixel_bytes": sixel_path.stat().st_size,
        "sixel_sha256": file_sha256(sixel_path),
        "scan_wall_seconds": scan_seconds,
        "paint_wall_seconds": paint_seconds,
        "decoder_wall_seconds": decoder_seconds,
        "png_wall_seconds": phase_wall_seconds(records, "png", "io"),
        "total_wall_seconds": (
            max(float(record["ts"]) for record in records)
            - min(float(record["ts"]) for record in records)
        ),
        "scan_fraction_of_decoder": scan_seconds / decoder_seconds,
        "paint_fraction_of_decoder": paint_seconds / decoder_seconds,
        "command": command_template(command, replacements),
    }


def dither_intervals(records: Sequence[Dict[str, object]]) \
        -> List[Tuple[float, float]]:
    """Return parallel bands or the serial producer's row-ready window."""
    intervals = event_intervals(records, "start", "finish", "dither", "worker")
    if intervals:
        return intervals
    row_times = [
        float(record["ts"])
        for record in records
        if record.get("worker") == "dither"
        and record.get("role") == "producer"
        and record.get("event") == "row_ready"
    ]
    if len(row_times) > 1:
        return [(min(row_times), max(row_times))]
    return []


def intervals_overlap(left: Sequence[Tuple[float, float]],
                      right: Sequence[Tuple[float, float]]) -> bool:
    """Return whether any two half-open activity intervals overlap."""
    return any(max(a0, b0) < min(a1, b1)
               for a0, a1 in left for b0, b1 in right)


def parse_planner(diagnostic: str) -> Dict[str, object]:
    """Extract the planner pipeline summary from verbose stderr."""
    mode_matches = MODE_PATTERN.findall(diagnostic)
    planner_matches = PLANNER_PATTERN.findall(diagnostic)
    if len(mode_matches) != 1 or len(planner_matches) != 1:
        raise RuntimeError("verbose output did not contain one pipeline plan")
    bands, queue, mode = mode_matches[0]
    band_height, overlap, dither, encode = planner_matches[0]
    return {
        "planner_mode": mode,
        "bands": int(bands),
        "queue_depth": int(queue),
        "band_height": int(band_height),
        "overlap_rows": int(overlap),
        "planned_dither_threads": int(dither),
        "planned_encode_threads": int(encode),
    }


def unique_threads(records: Sequence[Dict[str, object]],
                   worker: str,
                   role: str,
                   event: str) -> int:
    """Count OS thread identifiers for one raw timeline event."""
    return len({
        int(record.get("thread", -1))
        for record in records
        if record.get("worker") == worker
        and record.get("role") == role
        and record.get("event") == event
    })


def command_template(command: Sequence[str],
                     replacements: Dict[str, str]) -> str:
    """Replace machine-specific arguments in one provenance command."""
    rendered: List[str] = []
    for argument in command:
        rendered.append(replacements.get(argument, argument))
    return shlex.join(rendered)


def write_csv(path: Path, rows: Sequence[Dict[str, object]]) -> None:
    """Write records with stable column order."""
    if not rows:
        raise ValueError("cannot write an empty CSV")
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(
            handle, fieldnames=list(rows[0]), lineterminator="\n"
        )
        writer.writeheader()
        writer.writerows(rows)


def plot_budget(path: Path, rows: Sequence[Dict[str, object]]) -> None:
    """Plot planned overlap allocation and observed pool membership."""
    threads = [int(row["threads"]) for row in rows]
    dither = [int(row["planned_dither_threads"]) for row in rows]
    encode = [int(row["planned_encode_threads"]) for row in rows]
    observed_dither = [
        int(row["observed_parallel_dither_threads"]) for row in rows
    ]
    observed_encode = [
        int(row["observed_encode_pool_threads"]) for row in rows
    ]
    tail_capacity = [
        int(row["threads"])
        if str(row["tail_grow_requested"]) == "yes"
        else int(row["observed_encode_pool_threads"])
        for row in rows
    ]

    figure, (plan_ax, observed_ax) = plt.subplots(
        2,
        1,
        figsize=(9.2, 7.0),
        sharex=True,
        gridspec_kw={"height_ratios": (1.1, 1.0)},
    )
    plan_ax.bar(threads, dither, color="#55A868", label="dither budget")
    plan_ax.bar(
        threads,
        encode,
        bottom=dither,
        color="#4C72B0",
        label="encode budget",
    )
    plan_ax.annotate(
        "2: planner proposes 1+1, but the runtime pipeline gate\n"
        "requires two encode workers and serializes the stages",
        xy=(2, 2),
        xytext=(3.1, 4.2),
        arrowprops={"arrowstyle": "->", "color": "#444444"},
        fontsize=8.5,
    )
    plan_ax.set_ylabel("Planned stage workers")
    plan_ax.set_title("Encoder thread budget during dither/encode overlap")
    plan_ax.grid(True, axis="y", color="#D9D9D9", linewidth=0.7)
    plan_ax.legend(frameon=False, ncol=2, loc="upper left")

    observed_ax.plot(
        threads,
        observed_dither,
        color="#55A868",
        marker="o",
        linewidth=1.8,
        label="parallel dither pool",
    )
    observed_ax.plot(
        threads,
        observed_encode,
        color="#4C72B0",
        marker="s",
        linewidth=1.8,
        label="encode workers observed",
    )
    observed_ax.plot(
        threads,
        tail_capacity,
        color="#4C72B0",
        linestyle="--",
        linewidth=1.2,
        label="post-dither encode capacity",
    )
    for row in rows:
        if str(row["dither_encode_overlap"]) == "yes":
            observed_ax.axvspan(
                int(row["threads"]) - 0.42,
                int(row["threads"]) + 0.42,
                color="#DDDDDD",
                alpha=0.22,
                linewidth=0,
            )
    observed_ax.set_xlabel("Configured worker budget (--threads)")
    observed_ax.set_ylabel("Distinct pool threads")
    observed_ax.set_xticks(threads)
    observed_ax.grid(True, color="#D9D9D9", linewidth=0.7)
    observed_ax.legend(frameon=False, loc="upper left")
    figure.text(
        0.99,
        0.01,
        "Grey columns had measured dither/encode overlap. Writer and caller "
        "threads are excluded.",
        horizontalalignment="right",
        fontsize=8,
        color="#555555",
    )
    figure.tight_layout(rect=(0.0, 0.035, 1.0, 1.0))
    figure.savefig(path, dpi=160)
    plt.close(figure)


def inclusive_quartiles(values: Sequence[float]) -> Tuple[float, float]:
    """Return inclusive first and third quartiles for repeated samples."""
    quartiles = statistics.quantiles(values, n=4, method="inclusive")
    return quartiles[0], quartiles[2]


def plot_decoder_scaling(path: Path,
                         rows: Sequence[Dict[str, object]],
                         logical_cpus: int,
                         warmups: int,
                         repeats: int) -> None:
    """Plot repeated Full HD decoder timings against the worker budget."""
    threads = sorted({int(row["threads"]) for row in rows})
    fields = (
        ("decoder_wall_seconds", "decoder", "#4C72B0", "o"),
        ("scan_wall_seconds", "validation scan", "#8CBFDB", "s"),
        ("paint_wall_seconds", "direct paint", "#225C8D", "^"),
    )
    figure, axis = plt.subplots(figsize=(10.2, 5.4))

    for field, label, color, marker in fields:
        x_values: List[int] = []
        medians: List[float] = []
        lows: List[float] = []
        highs: List[float] = []
        for workers in threads:
            samples = [
                float(row[field]) * 1000.0
                for row in rows
                if int(row["threads"]) == workers and row[field] != ""
            ]
            if not samples:
                continue
            low, high = inclusive_quartiles(samples)
            x_values.append(workers)
            medians.append(statistics.median(samples))
            lows.append(low)
            highs.append(high)
        axis.plot(
            x_values,
            medians,
            color=color,
            marker=marker,
            linewidth=1.9,
            label=f"{label} median",
        )
        axis.fill_between(
            x_values,
            lows,
            highs,
            color=color,
            alpha=0.15,
            linewidth=0,
        )

    serial = statistics.median([
        float(row["decoder_wall_seconds"]) * 1000.0
        for row in rows
        if int(row["threads"]) == 1
    ])
    axis.plot(
        threads,
        [serial / workers for workers in threads],
        color="#777777",
        linestyle="--",
        linewidth=1.1,
        label="ideal decoder time from 1-worker median",
    )
    if logical_cpus in threads:
        axis.axvline(
            logical_cpus,
            color="#888888",
            linestyle=":",
            linewidth=1.0,
        )
        axis.annotate(
            f"host reports {logical_cpus} logical CPUs",
            xy=(logical_cpus, axis.get_ylim()[1]),
            xytext=(-5, -6),
            textcoords="offset points",
            horizontalalignment="right",
            verticalalignment="top",
            fontsize=8,
            color="#555555",
        )
    axis.set_title("Full HD decoder time by configured worker budget")
    axis.set_xlabel("Configured decoder worker budget (--threads)")
    axis.set_ylabel("Wall interval (ms, lower is better)")
    axis.set_xticks(threads)
    axis.set_ylim(bottom=0)
    axis.grid(True, color="#D9D9D9", linewidth=0.7)
    axis.legend(frameon=False, ncol=2)
    figure.text(
        0.99,
        0.01,
        f"{repeats} timed runs after {warmups} warm-ups. Lines show medians; "
        "shading shows IQR. Workers are not pinned to physical cores.",
        horizontalalignment="right",
        fontsize=8,
        color="#555555",
    )
    figure.tight_layout(rect=(0.0, 0.04, 1.0, 1.0))
    figure.savefig(path, dpi=160)
    plt.close(figure)


def measure_encoder(args: argparse.Namespace,
                    source_root: Path,
                    input_path: Path,
                    work_dir: Path,
                    env: Dict[str, str],
                    threads_values: Sequence[int]) \
        -> Tuple[List[Dict[str, object]], List[str]]:
    """Run the static budget sweep and retain representative logs."""
    rows: List[Dict[str, object]] = []
    commands: List[str] = []
    retained = set(args.timeline_threads)
    for threads in threads_values:
        log_path = work_dir / f"encoder-thread{threads}.jsonl"
        output_path = work_dir / f"encoder-thread{threads}.six"
        command = encoder_command(
            args.img2sixel,
            input_path,
            threads,
            output_path,
            log_path,
        )
        print(f"[encoder {threads}/{max(threads_values)}]", flush=True)
        proc = run_checked(command, env)
        diagnostic = proc.stderr.decode("utf-8", errors="replace")
        records = load_records(log_path)
        plan = parse_planner(diagnostic)
        encode_intervals = event_intervals(
            records,
            "worker_start",
            "worker_done",
            "encode",
            "worker",
        )
        row: Dict[str, object] = {
            "revision": args.revision,
            "threads": threads,
            **plan,
            "observed_parallel_dither_threads": unique_threads(
                records, "dither", "worker", "start"
            ),
            "observed_encode_pool_threads": unique_threads(
                records, "encode", "worker", "worker_start"
            ),
            "observed_writer_threads": unique_threads(
                records, "encode", "writer", "writer_start"
            ),
            "tail_grow_requested": "yes" if any(
                record.get("worker") == "encode"
                and record.get("role") == "controller"
                and record.get("event") == "grow_workers"
                for record in records
            ) else "no",
            "dither_encode_overlap": "yes" if intervals_overlap(
                dither_intervals(records), encode_intervals
            ) else "no",
            "elapsed_seconds": max(float(record["ts"]) for record in records),
            "command": command_template(
                command,
                {
                    args.img2sixel: "{img2sixel}",
                    str(input_path): "{input}",
                    str(output_path): "{output}",
                    str(log_path): "{log}",
                },
            ),
        }
        rows.append(row)
        commands.append(str(row["command"]))
        if threads in retained:
            shutil.copyfile(
                log_path,
                args.output_dir / f"encoder-thread{threads}.jsonl",
            )
    return rows, commands


def measure_decoder_scaling(args: argparse.Namespace,
                            sixel_path: Path,
                            work_dir: Path,
                            env: Dict[str, str]) \
        -> Tuple[List[Dict[str, object]], List[str]]:
    """Measure repeated Full HD decoder phases for each worker budget."""
    rows: List[Dict[str, object]] = []
    commands: Dict[int, str] = {}
    sixel_bytes = sixel_path.stat().st_size
    sixel_hash = file_sha256(sixel_path)
    total_runs = args.decoder_warmups + args.decoder_repeats
    for run_index in range(total_runs):
        ordered_threads = args.decoder_scaling_threads
        if run_index % 2:
            ordered_threads = tuple(reversed(ordered_threads))
        for threads in ordered_threads:
            log_path = work_dir / (
                f"decoder-scaling-thread{threads}-run{run_index + 1}.jsonl"
            )
            command = decoder_command(
                args.sixel2png,
                sixel_path,
                threads,
                log_path,
            )
            commands[threads] = command_template(
                command,
                {
                    args.sixel2png: "{sixel2png}",
                    f"--threads={threads}": "--threads={threads}",
                    str(sixel_path): "{decoder_sixel}",
                    str(log_path): "{log}",
                    os.devnull: "{null}",
                },
            )
            run_checked(command, env)
            if run_index < args.decoder_warmups:
                continue
            records = load_records(log_path)
            rows.append({
                "revision": args.revision,
                "fixture": "1920x1080",
                "width": 1920,
                "height": 1080,
                "pixels": 1920 * 1080,
                "sixel_bytes": sixel_bytes,
                "sixel_sha256": sixel_hash,
                "threads": threads,
                "sample": run_index - args.decoder_warmups + 1,
                "decoder_wall_seconds": phase_wall_seconds(
                    records, "io", "decoder"
                ),
                "scan_wall_seconds": optional_phase_wall_seconds(
                    records, "decoder", "scan"
                ),
                "paint_wall_seconds": optional_phase_wall_seconds(
                    records, "decoder", "paint"
                ),
                "png_wall_seconds": phase_wall_seconds(
                    records, "png", "io"
                ),
                "total_wall_seconds": (
                    max(float(record["ts"]) for record in records)
                    - min(float(record["ts"]) for record in records)
                ),
                "command": command_template(
                    command,
                    {
                        args.sixel2png: "{sixel2png}",
                        str(sixel_path): "{decoder_sixel}",
                        str(log_path): "{log}",
                        os.devnull: "{null}",
                    },
                ),
            })
        print(
            f"[decoder scaling run {run_index + 1}/{total_runs}]",
            flush=True,
        )
    rows.sort(key=lambda row: (int(row["threads"]), int(row["sample"])))
    return rows, [commands[threads] for threads in args.decoder_scaling_threads]


def measure_auxiliary_timelines(args: argparse.Namespace,
                                input_path: Path,
                                animation_input: Path,
                                work_dir: Path,
                                env: Dict[str, str]) \
        -> Tuple[
            Dict[str, object],
            List[Dict[str, object]],
            List[Dict[str, object]],
        ]:
    """Record decoder and finite-animation representative timelines."""
    decoder_rows: List[Dict[str, object]] = []
    decoder_scaling_rows: List[Dict[str, object]] = []
    decoder_scaling_commands: List[str] = []
    fixture_commands: List[str] = []
    decoder_commands: List[str] = []
    for identifier, width, height in DECODER_SIZES:
        decoder_sixel = work_dir / f"decoder-{identifier}.six"
        fixture = decoder_fixture_command(
            args.img2sixel,
            input_path,
            decoder_sixel,
            width,
            height,
        )
        print(f"[decoder fixture {identifier}]", flush=True)
        run_checked(fixture, env)

        if identifier == "1920x1080":
            decoder_log = args.output_dir / "decoder-thread8.jsonl"
        else:
            decoder_log = (
                args.output_dir / f"decoder-thread8-{identifier}.jsonl"
            )
        decoder = decoder_command(
            args.sixel2png,
            decoder_sixel,
            8,
            decoder_log,
        )
        print(f"[decoder {identifier}, 8 workers]", flush=True)
        run_checked(decoder, env)

        fixture_template = command_template(
            fixture,
            {
                args.img2sixel: "{img2sixel}",
                str(input_path): "{input}",
                str(decoder_sixel): "{decoder_sixel}",
            },
        )
        decoder_template = command_template(
            decoder,
            {
                args.sixel2png: "{sixel2png}",
                str(decoder_sixel): "{decoder_sixel}",
                str(decoder_log): "{log}",
                os.devnull: "{null}",
            },
        )
        fixture_commands.append(fixture_template)
        decoder_commands.append(decoder_template)
        decoder_rows.append(decoder_measurement_row(
            identifier,
            width,
            height,
            args.revision,
            decoder_sixel,
            decoder_log,
            decoder,
            {
                args.sixel2png: "{sixel2png}",
                str(decoder_sixel): "{decoder_sixel}",
                str(decoder_log): "{log}",
                os.devnull: "{null}",
            },
        ))
        if identifier == "1920x1080":
            decoder_scaling_rows, decoder_scaling_commands = (
                measure_decoder_scaling(
                    args,
                    decoder_sixel,
                    work_dir,
                    env,
                )
            )

    animation_log = args.output_dir / "animation-thread4.jsonl"
    animation_output = work_dir / "animation-thread4.six"
    animation = animation_command(
        args.img2sixel,
        animation_input,
        4,
        animation_output,
        animation_log,
    )
    print("[animation 4]", flush=True)
    run_checked(animation, env)
    return ({
        "decoder_fixtures": fixture_commands,
        "decoders": decoder_commands,
        "decoder_scaling": decoder_scaling_commands,
        "animation": command_template(
            animation,
            {
                args.img2sixel: "{img2sixel}",
                str(animation_input): "{animation_input}",
                str(animation_output): "{output}",
                str(animation_log): "{log}",
            },
        ),
    }, decoder_rows, decoder_scaling_rows)


def write_metadata(args: argparse.Namespace,
                   source_root: Path,
                   input_path: Path,
                   animation_input: Path,
                   encoder_commands: Sequence[str],
                   auxiliary_commands: Dict[str, object]) -> None:
    """Write revision, host, inputs, tools, and protocol provenance."""
    payload = {
        "schema_version": 3,
        "generated_at_utc": datetime.datetime.now(
            datetime.timezone.utc
        ).isoformat(),
        "source": {
            "revision": args.revision,
            "tracked_worktree_state_at_start": args.source_state,
        },
        "build": read_build_configuration(args.build_dir),
        "host": {
            "platform": platform.platform(),
            "processor": platform.processor(),
            "logical_cpu_count": os.cpu_count(),
            "python": platform.python_version(),
            "matplotlib": matplotlib.__version__,
        },
        "inputs": {
            "static": {
                "path": display_path(input_path, source_root),
                "sha256": file_sha256(input_path),
            },
            "animation": {
                "path": display_path(animation_input, source_root),
                "sha256": file_sha256(animation_input),
                "contract": "finite two-frame GIF without a loop extension",
            },
            "decoder": {
                "path": display_path(input_path, source_root),
                "sha256": file_sha256(input_path),
                "encoded_rasters": [
                    {
                        "id": identifier,
                        "width": width,
                        "height": height,
                    }
                    for identifier, width, height in DECODER_SIZES
                ],
                "contract": "dedicated size-controlled decoder fixtures",
            },
        },
        "programs": {
            "img2sixel": program_record(args.img2sixel, source_root),
            "sixel2png": program_record(args.sixel2png, source_root),
            "timeline": program_record(
                str(source_root / "tools" / "timeline.py"), source_root
            ),
        },
        "protocol": {
            "thread_counts": list(args.threads),
            "timeline_thread_counts": list(args.timeline_threads),
            "sixel_environment_removed": args.clean_sixel_environment,
            "encoder_commands": list(encoder_commands),
            "decoder_fixture_commands": auxiliary_commands[
                "decoder_fixtures"
            ],
            "decoder_commands": auxiliary_commands["decoders"],
            "decoder_scaling": {
                "thread_counts": list(args.decoder_scaling_threads),
                "warmup_runs_per_thread": args.decoder_warmups,
                "timed_runs_per_thread": args.decoder_repeats,
                "commands": auxiliary_commands["decoder_scaling"],
                "summary": "median with inclusive interquartile range",
            },
            "animation_command": auxiliary_commands["animation"],
            "timeline_commands": [
                "{python} tools/timeline.py --sort-order start "
                "--frame-mode off {log} --output {png}",
                "{python} tools/timeline.py --sort-order start "
                "--frame-mode on {animation_log} --output {png}",
            ],
            "timing_interpretation": (
                "Timelines and size points are single diagnostic runs. The "
                "decoder scaling sweep uses repeated instrumented samples "
                "without exclusive host or CPU affinity."
            ),
        },
    }
    with (args.output_dir / "threading-run.json").open(
            "w", encoding="utf-8") as handle:
        json.dump(payload, handle, indent=2, sort_keys=True)
        handle.write("\n")


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--img2sixel", required=True)
    parser.add_argument("--sixel2png", required=True)
    parser.add_argument("--build-dir", type=Path, required=True)
    parser.add_argument("--input", type=Path, required=True)
    parser.add_argument("--animation-input", type=Path, required=True)
    parser.add_argument("--threads", default=",".join(
        str(value) for value in DEFAULT_THREADS
    ))
    parser.add_argument("--timeline-threads", default="2,4,8")
    parser.add_argument("--decoder-scaling-threads", default=",".join(
        str(value) for value in DEFAULT_DECODER_SCALING_THREADS
    ))
    parser.add_argument(
        "--decoder-warmups", type=int, default=DEFAULT_DECODER_WARMUPS
    )
    parser.add_argument(
        "--decoder-repeats", type=int, default=DEFAULT_DECODER_REPEATS
    )
    parser.add_argument("--revision", default="unknown")
    parser.add_argument("--source-state", default="unknown")
    parser.add_argument("--clean-sixel-environment", action="store_true")
    parser.add_argument("--output-dir", type=Path, required=True)
    return parser.parse_args()


def main() -> int:
    """Run the measurements and write all non-timeline-plot artifacts."""
    args = parse_args()
    source_root = Path(__file__).resolve().parent.parent
    args.input = args.input.resolve()
    args.animation_input = args.animation_input.resolve()
    args.build_dir = args.build_dir.resolve()
    args.output_dir = args.output_dir.resolve()
    args.threads = parse_threads(args.threads)
    args.timeline_threads = parse_threads(args.timeline_threads)
    args.decoder_scaling_threads = parse_threads(
        args.decoder_scaling_threads
    )
    if args.decoder_warmups < 0 or args.decoder_repeats < 2:
        raise ValueError("decoder scaling needs warmups >= 0 and repeats >= 2")
    if not set(args.timeline_threads).issubset(args.threads):
        raise ValueError("timeline thread counts must be part of the sweep")
    if not args.input.is_file() or not args.animation_input.is_file():
        raise FileNotFoundError("measurement input is missing")
    args.output_dir.mkdir(parents=True, exist_ok=True)
    env = make_command_environment(args.clean_sixel_environment)

    with tempfile.TemporaryDirectory(prefix="libsixel-threading-") as temp:
        work_dir = Path(temp)
        rows, encoder_commands = measure_encoder(
            args,
            source_root,
            args.input,
            work_dir,
            env,
            args.threads,
        )
        (auxiliary_commands,
         decoder_rows,
         decoder_scaling_rows) = measure_auxiliary_timelines(
            args,
            args.input,
            args.animation_input,
            work_dir,
            env,
        )

    write_csv(args.output_dir / "thread-budget.csv", rows)
    write_csv(
        args.output_dir / "decoder-size-comparison.csv",
        decoder_rows,
    )
    write_csv(
        args.output_dir / "decoder-thread-scaling.csv",
        decoder_scaling_rows,
    )
    plot_budget(args.output_dir / "thread-budget.png", rows)
    plot_decoder_scaling(
        args.output_dir / "decoder-thread-scaling.png",
        decoder_scaling_rows,
        os.cpu_count() or 0,
        args.decoder_warmups,
        args.decoder_repeats,
    )
    write_metadata(
        args,
        source_root,
        args.input,
        args.animation_input,
        encoder_commands,
        auxiliary_commands,
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
