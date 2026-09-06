#!/usr/bin/env python3
"""Validate checked-in thread-budget and timeline measurement artifacts."""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path
from typing import Dict, Iterable, List, Sequence, Tuple


THREADS = tuple(range(1, 13))
ENCODER_SCALING_THREADS = tuple(range(1, 17))
DECODER_SCALING_THREADS = tuple(range(1, 17))
DECODER_SIZES = (
    ("900x675", 900, 675, "decoder-thread8-900x675.jsonl"),
    ("1920x1080", 1920, 1080, "decoder-thread8.jsonl"),
)
REQUIRED_FILES = (
    "thread-budget.csv",
    "thread-budget.png",
    "threading-run.json",
    "encoder-thread2.jsonl",
    "encoder-thread2-timeline.png",
    "encoder-thread4.jsonl",
    "encoder-thread4-timeline.png",
    "encoder-thread8.jsonl",
    "encoder-thread8-timeline.png",
    "encoder-thread-scaling.csv",
    "encoder-thread-scaling.png",
    "decoder-size-comparison.csv",
    "decoder-thread-scaling.csv",
    "decoder-thread-scaling.png",
    "decoder-thread8-900x675.jsonl",
    "decoder-thread8.jsonl",
    "decoder-thread8-timeline.png",
    "animation-thread4.jsonl",
    "animation-thread4-timeline.png",
)


def load_jsonl(path: Path) -> List[Dict[str, object]]:
    """Load a non-empty JSONL artifact."""
    records: List[Dict[str, object]] = []
    with path.open("r", encoding="utf-8") as handle:
        for line in handle:
            if line.strip():
                records.append(json.loads(line))
    if not records:
        raise ValueError(f"timeline is empty: {path.name}")
    return records


def count(records: Iterable[Dict[str, object]],
          worker: str,
          role: str,
          event: str) -> int:
    """Count one timeline event kind."""
    return sum(
        1 for record in records
        if record.get("worker") == worker
        and record.get("role") == role
        and record.get("event") == event
    )


def paired_intervals(records: Sequence[Dict[str, object]],
                     worker: str,
                     role: str,
                     start_event: str,
                     finish_event: str) -> List[Tuple[float, float]]:
    """Pair spans by session, thread, and job and reject missing boundaries."""
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
        elif event == finish_event:
            if not starts.get(key):
                raise ValueError(
                    f"unmatched {finish_event} for {worker}/{role} {key}"
                )
            intervals.append((starts[key].pop(0), float(record["ts"])))
    if any(values for values in starts.values()):
        raise ValueError(f"unmatched {start_event} for {worker}/{role}")
    return intervals


def validate_png(path: Path) -> None:
    """Check the PNG signature and a non-trivial size."""
    if path.stat().st_size < 1024:
        raise ValueError(f"PNG is unexpectedly small: {path.name}")
    with path.open("rb") as handle:
        if handle.read(8) != b"\x89PNG\r\n\x1a\n":
            raise ValueError(f"invalid PNG signature: {path.name}")


def validate_budget(path: Path, revision: str) -> None:
    """Validate the current encoder allocation contract and observations."""
    with path.open("r", encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    if [int(row["threads"]) for row in rows] != list(THREADS):
        raise ValueError("thread-budget.csv does not cover threads 1 through 12")
    for row in rows:
        threads = int(row["threads"])
        dither = int(row["planned_dither_threads"])
        encode = int(row["planned_encode_threads"])
        if row["revision"] != revision:
            raise ValueError("CSV and metadata revisions differ")
        if (row["fixture"] != "1920x1080"
                or int(row["width"]) != 1920
                or int(row["height"]) != 1080
                or int(row["pixels"]) != 1920 * 1080):
            raise ValueError("encoder budget sweep is not Full HD")
        if "-w 1920 -h 1080" not in row["command"]:
            raise ValueError("encoder budget command is not Full HD")
        if (float(row["encoder_wall_seconds"]) <= 0.0
                or float(row["palette_wall_seconds"]) <= 0.0
                or float(row["dither_encode_wall_seconds"]) <= 0.0):
            raise ValueError("encoder budget phase timing is missing")
        if int(row["encoder_abort_count"]) != 0:
            raise ValueError("encoder budget run contains an abort")
        if (int(row["dither_start_count"])
                != int(row["dither_finish_count"])
                or int(row["encode_worker_start_count"])
                != int(row["encode_worker_finish_count"])
                or int(row["writer_start_count"])
                != int(row["writer_finish_count"])):
            raise ValueError("encoder budget spans are unpaired")
        if threads == 1:
            if row["planner_mode"] != "serial" or dither != 0 or encode != 0:
                raise ValueError("one-thread planner contract changed")
            if (int(row["observed_parallel_dither_threads"]) != 0
                    or int(row["observed_encode_pool_threads"]) != 0
                    or int(row["observed_writer_threads"]) != 0):
                raise ValueError("one-thread encoder unexpectedly used pools")
            continue
        if dither + encode != threads:
            raise ValueError(f"planner budget does not sum at threads={threads}")
        if threads == 2 and row["dither_encode_overlap"] != "no":
            raise ValueError("two-thread runtime unexpectedly overlapped stages")
        if threads >= 3 and row["dither_encode_overlap"] != "yes":
            raise ValueError(f"pipeline overlap missing at threads={threads}")
        if threads >= 3 and encode < 2:
            raise ValueError("active pipeline has fewer than two encode workers")
        expected_grow = "yes" if threads >= 3 else "no"
        if row["tail_grow_requested"] != expected_grow:
            raise ValueError(f"unexpected tail-grow event at threads={threads}")
        observed_encode = int(row["observed_encode_pool_threads"])
        if observed_encode < encode or observed_encode > threads:
            raise ValueError(
                f"observed encode workers violate budget at threads={threads}"
            )
        if int(row["observed_writer_threads"]) != 1:
            raise ValueError("encoder ordered writer was not observed")
    row4 = rows[3]
    if int(row4["observed_parallel_dither_threads"]) != 2:
        raise ValueError("four-thread run did not create two dither workers")
    if int(row4["observed_encode_pool_threads"]) < 2:
        raise ValueError("four-thread run did not exercise two encode workers")


def validate_decoder(path: Path) -> None:
    """Validate scan completion, paint barrier, and parallel paint spans."""
    records = load_jsonl(path)
    scan = paired_intervals(records, "decoder", "scan", "start", "finish")
    paint = paired_intervals(records, "decoder", "paint", "start", "finish")
    ready = [
        float(record["ts"])
        for record in records
        if record.get("worker") == "decoder"
        and record.get("role") == "paint"
        and record.get("event") == "ready"
    ]
    if len(scan) != 8 or len(paint) != 8:
        raise ValueError("decoder timeline does not contain eight scan/paint spans")
    if len(ready) != 8:
        raise ValueError("decoder timeline does not contain eight ready workers")
    if min(ready) < max(end for _, end in scan):
        raise ValueError("decoder paint worker became ready before scan joined")
    if min(start for start, _ in paint) < max(end for _, end in scan):
        raise ValueError("decoder paint started before the validation scan joined")
    if min(start for start, _ in paint) < max(ready):
        raise ValueError("decoder paint started before every worker was ready")
    overlaps = any(
        left[0] < right[1] and right[0] < left[1]
        for index, left in enumerate(paint)
        for right in paint[index + 1:]
    )
    if not overlaps:
        raise ValueError("decoder paint spans did not overlap")


def phase_wall_seconds(records: Sequence[Dict[str, object]],
                       worker: str,
                       role: str) -> float:
    """Return the wall interval enclosing one paired timeline phase."""
    intervals = paired_intervals(
        records,
        worker,
        role,
        "start",
        "finish",
    )
    if not intervals:
        raise ValueError(f"timeline omitted {worker}/{role} phase")
    return max(end for _, end in intervals) - min(
        start for start, _ in intervals
    )


def validate_decoder_sizes(path: Path, root: Path, revision: str) -> None:
    """Validate size comparison rows against their retained timelines."""
    with path.open("r", encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    if [row["fixture"] for row in rows] != [
        item[0] for item in DECODER_SIZES
    ]:
        raise ValueError("decoder size comparison fixtures changed")
    for row, (identifier, width, height, log_name) in zip(
            rows, DECODER_SIZES):
        records = load_jsonl(root / log_name)
        decoder_seconds = phase_wall_seconds(records, "io", "decoder")
        scan_seconds = phase_wall_seconds(records, "decoder", "scan")
        paint_seconds = phase_wall_seconds(records, "decoder", "paint")
        if row["revision"] != revision:
            raise ValueError("decoder size CSV and metadata revisions differ")
        if int(row["width"]) != width or int(row["height"]) != height:
            raise ValueError(f"decoder dimensions changed for {identifier}")
        if int(row["pixels"]) != width * height:
            raise ValueError(f"decoder pixel count changed for {identifier}")
        checks = (
            ("decoder_wall_seconds", decoder_seconds),
            ("scan_wall_seconds", scan_seconds),
            ("paint_wall_seconds", paint_seconds),
            ("scan_fraction_of_decoder", scan_seconds / decoder_seconds),
            ("paint_fraction_of_decoder", paint_seconds / decoder_seconds),
        )
        for field, expected in checks:
            if abs(float(row[field]) - expected) > 1e-12:
                raise ValueError(
                    f"decoder size CSV field {field} is stale for {identifier}"
                )


def validate_decoder_scaling(path: Path,
                             revision: str,
                             threads_values: Sequence[int],
                             warmups: int,
                             repeats: int) -> None:
    """Validate the complete scheduled decoder scaling experiment."""
    with path.open("r", encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    total_rounds = warmups + repeats
    expected_rows = len(threads_values) * total_rounds
    if len(rows) != expected_rows:
        raise ValueError("decoder scaling CSV has an unexpected row count")
    width = len(threads_values)
    for round_index in range(1, total_rounds + 1):
        first = (round_index - 1) * width
        scheduled = rows[first:first + width]
        traversal = "ascending" if round_index % 2 else "descending"
        expected_threads = list(threads_values)
        if traversal == "descending":
            expected_threads.reverse()
        phase = "warmup" if round_index <= warmups else "timed"
        sample = round_index if phase == "warmup" else round_index - warmups
        if [int(row["threads"]) for row in scheduled] != expected_threads:
            raise ValueError(f"decoder scaling schedule changed at round={round_index}")
        for position, row in enumerate(scheduled, start=1):
            if int(row["round"]) != round_index:
                raise ValueError("decoder scaling round is stale")
            if row["phase"] != phase or int(row["sample"]) != sample:
                raise ValueError("decoder scaling warmup/sample phase is stale")
            if row["traversal"] != traversal:
                raise ValueError("decoder scaling traversal is stale")
            if int(row["schedule_position"]) != position:
                raise ValueError("decoder scaling schedule position is stale")

    sixel_hashes = {row["sixel_sha256"] for row in rows}
    sixel_sizes = {row["sixel_bytes"] for row in rows}
    if len(sixel_hashes) != 1 or len(sixel_sizes) != 1:
        raise ValueError("decoder scaling fixture changed during the sweep")
    for row in rows:
        threads = int(row["threads"])
        if row["revision"] != revision:
            raise ValueError("decoder scaling revision is stale")
        if row["fixture"] != "1920x1080":
            raise ValueError("decoder scaling fixture changed")
        if int(row["width"]) != 1920 or int(row["height"]) != 1080:
            raise ValueError("decoder scaling dimensions changed")
        decoder = float(row["decoder_wall_seconds"])
        png = float(row["png_wall_seconds"])
        total = float(row["total_wall_seconds"])
        if decoder <= 0.0 or png <= 0.0 or total <= 0.0:
            raise ValueError("decoder scaling contains non-positive time")
        if threads == 1:
            if row["scan_wall_seconds"] or row["paint_wall_seconds"]:
                raise ValueError("serial decoder unexpectedly has phases")
            if row["observed_path"] != "serial":
                raise ValueError("serial decoder path observation is stale")
            expected_counts = (0, 0, 0, 0, 0, 0)
            if (row["barrier_ordered"] != "not-applicable"
                    or row["paint_spans_overlap"] != "not-applicable"):
                raise ValueError("serial decoder unexpectedly used a barrier")
        else:
            if (float(row["scan_wall_seconds"]) <= 0.0
                    or float(row["paint_wall_seconds"]) <= 0.0):
                raise ValueError("parallel decoder phase timing is missing")
            if row["observed_path"] != "parallel-direct":
                raise ValueError("decoder scaling fell back from direct paint")
            expected_counts = (threads, threads, threads, threads, threads, 0)
            if (row["barrier_ordered"] != "yes"
                    or row["paint_spans_overlap"] != "yes"):
                raise ValueError("parallel decoder barrier observation is stale")
        observed_counts = tuple(int(row[name]) for name in (
            "scan_start_count",
            "scan_finish_count",
            "paint_ready_count",
            "paint_start_count",
            "paint_finish_count",
            "decoder_abort_count",
        ))
        if observed_counts != expected_counts:
            raise ValueError("decoder scaling span counts are stale")
        if f"--threads={threads}" not in row["command"]:
            raise ValueError("decoder scaling command is stale")


def validate_encoder_scaling(path: Path,
                             revision: str,
                             threads_values: Sequence[int],
                             warmups: int,
                             repeats: int) -> None:
    """Validate the complete scheduled img2sixel scaling experiment."""
    with path.open("r", encoding="utf-8", newline="") as handle:
        rows = list(csv.DictReader(handle))
    total_rounds = warmups + repeats
    expected_rows = len(threads_values) * total_rounds
    if len(rows) != expected_rows:
        raise ValueError("encoder scaling CSV has an unexpected row count")
    width = len(threads_values)
    for round_index in range(1, total_rounds + 1):
        first = (round_index - 1) * width
        scheduled = rows[first:first + width]
        traversal = "ascending" if round_index % 2 else "descending"
        expected_threads = list(threads_values)
        if traversal == "descending":
            expected_threads.reverse()
        phase = "warmup" if round_index <= warmups else "timed"
        sample = round_index if phase == "warmup" else round_index - warmups
        if [int(row["threads"]) for row in scheduled] != expected_threads:
            raise ValueError(
                f"encoder scaling schedule changed at round={round_index}"
            )
        for position, row in enumerate(scheduled, start=1):
            if int(row["round"]) != round_index:
                raise ValueError("encoder scaling round is stale")
            if row["phase"] != phase or int(row["sample"]) != sample:
                raise ValueError("encoder scaling warmup/sample phase is stale")
            if row["traversal"] != traversal:
                raise ValueError("encoder scaling traversal is stale")
            if int(row["schedule_position"]) != position:
                raise ValueError("encoder scaling schedule position is stale")

    for threads in threads_values:
        thread_rows = [
            row for row in rows if int(row["threads"]) == threads
        ]
        if len({row["sixel_sha256"] for row in thread_rows}) != 1:
            raise ValueError("encoder output changed within one worker count")
        if len({row["sixel_bytes"] for row in thread_rows}) != 1:
            raise ValueError("encoder output size changed within one worker count")
    for row in rows:
        threads = int(row["threads"])
        dither = int(row["planned_dither_threads"])
        encode = int(row["planned_encode_threads"])
        if row["revision"] != revision:
            raise ValueError("encoder scaling revision is stale")
        if (row["fixture"] != "1920x1080"
                or int(row["width"]) != 1920
                or int(row["height"]) != 1080
                or int(row["pixels"]) != 1920 * 1080):
            raise ValueError("encoder scaling fixture is not Full HD")
        if "-w 1920 -h 1080" not in row["command"]:
            raise ValueError("encoder scaling command is not Full HD")
        if f"--threads={threads}" not in row["command"]:
            raise ValueError("encoder scaling command worker count is stale")
        for field in (
                "encoder_wall_seconds",
                "palette_wall_seconds",
                "dither_encode_wall_seconds",
                "total_wall_seconds"):
            if float(row[field]) <= 0.0:
                raise ValueError("encoder scaling contains non-positive time")
        if int(row["sixel_bytes"]) <= 0:
            raise ValueError("encoder scaling output is empty")
        if int(row["encoder_abort_count"]) != 0:
            raise ValueError("encoder scaling contains an abort")
        counts = tuple(int(row[name]) for name in (
            "dither_start_count",
            "dither_finish_count",
            "encode_worker_start_count",
            "encode_worker_finish_count",
            "writer_start_count",
            "writer_finish_count",
        ))
        if counts[0] != counts[1] or counts[2] != counts[3]:
            raise ValueError("encoder scaling worker spans are unpaired")
        if counts[4] != counts[5]:
            raise ValueError("encoder scaling writer spans are unpaired")
        if threads == 1:
            if row["planner_mode"] != "serial" or dither != 0 or encode != 0:
                raise ValueError("serial encoder scaling path changed")
            if counts != (0, 0, 0, 0, 0, 0):
                raise ValueError("serial encoder unexpectedly used pools")
            if (row["dither_encode_overlap"] != "no"
                    or row["tail_grow_requested"] != "no"):
                raise ValueError("serial encoder pipeline state changed")
            continue
        if row["planner_mode"] != "pipeline" or dither + encode != threads:
            raise ValueError("encoder scaling planner budget is stale")
        if int(row["observed_encode_pool_threads"]) < encode:
            raise ValueError("encoder scaling encode pool was not observed")
        if int(row["observed_writer_threads"]) != 1:
            raise ValueError("encoder scaling writer was not observed")
        expected_overlap = "yes" if threads >= 3 else "no"
        expected_grow = "yes" if threads >= 3 else "no"
        if row["dither_encode_overlap"] != expected_overlap:
            raise ValueError("encoder scaling overlap state is stale")
        if row["tail_grow_requested"] != expected_grow:
            raise ValueError("encoder scaling tail growth state is stale")
        if (threads >= 4
                and int(row["observed_parallel_dither_threads"]) != dither):
            raise ValueError("encoder scaling dither pool was not observed")


def validate_animation(path: Path) -> None:
    """Validate two finite, non-overlapping frame encode intervals."""
    records = load_jsonl(path)
    frames = sorted({
        int(record.get("frame_no", -1))
        for record in records
        if int(record.get("frame_no", -1)) >= 0
    })
    if frames != [0, 1]:
        raise ValueError("animation timeline is not the finite two-frame run")
    intervals = paired_intervals(
        records, "encode", "worker", "start", "finish"
    )
    if len(intervals) != 2 or intervals[1][0] < intervals[0][1]:
        raise ValueError("animation frame encoding is not serialized")


def validate_encoder(path: Path) -> None:
    """Validate paired encoder band and writer diagnostics."""
    records = load_jsonl(path)
    if count(records, "encode", "worker", "worker_start") != count(
            records, "encode", "worker", "worker_done"):
        raise ValueError(f"encoder band events are unpaired: {path.name}")
    if count(records, "encode", "writer", "writer_start") != count(
            records, "encode", "writer", "writer_stop"):
        raise ValueError(f"encoder writer events are unpaired: {path.name}")


def parse_args() -> argparse.Namespace:
    """Parse command-line arguments."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("directory", type=Path)
    return parser.parse_args()


def main() -> int:
    """Validate the complete artifact set."""
    args = parse_args()
    root = args.directory.resolve()
    for name in REQUIRED_FILES:
        path = root / name
        if not path.is_file():
            raise FileNotFoundError(f"missing threading artifact: {name}")
        if path.suffix == ".png":
            validate_png(path)
    with (root / "threading-run.json").open("r", encoding="utf-8") as handle:
        metadata = json.load(handle)
    if int(metadata["schema_version"]) != 5:
        raise ValueError("threading metadata schema is not version 5")
    revision = str(metadata["source"]["revision"])
    if metadata["source"]["tracked_worktree_state_at_start"] != "clean":
        raise ValueError("threading artifacts were not recorded from clean source")
    runtime = metadata["runtime"]
    library = runtime["libsixel"]
    load_probes = runtime["load_probes"]
    if not runtime["dependencies_unchanged_during_measurement"]:
        raise ValueError("measurement dependencies changed during the run")
    if set(load_probes) != {"img2sixel", "sixel2png"}:
        raise ValueError("converter load-probe coverage is incomplete")
    for name, load_probe in load_probes.items():
        if not load_probe["verified"]:
            raise ValueError(f"{name} libsixel load was not verified")
        if (load_probe["loaded_path"] != library["path"]
                or load_probe["loaded_sha256"] != library["sha256"]):
            raise ValueError(f"{name} libsixel provenance is inconsistent")
        if str(Path(load_probe["loaded_path"]).parent) != (
                load_probe["search_directory"]):
            raise ValueError(f"{name} libsixel search path is inconsistent")
    for name in ("img2sixel", "sixel2png", "generator", "checker", "timeline"):
        record = metadata["programs"][name]
        if not record["launcher_sha256"] or not record["payload_sha256"]:
            raise ValueError(f"program provenance is incomplete for {name}")
    decoder_input = metadata["inputs"]["decoder"]
    encoder_input = metadata["inputs"]["static"]
    if (int(encoder_input["processed_width"]) != 1920
            or int(encoder_input["processed_height"]) != 1080):
        raise ValueError("encoder processed dimensions changed")
    raster_sizes = [
        (item["id"], int(item["width"]), int(item["height"]))
        for item in decoder_input["encoded_rasters"]
    ]
    expected_sizes = [item[:3] for item in DECODER_SIZES]
    if raster_sizes != expected_sizes:
        raise ValueError("decoder metadata does not cover the controlled sizes")
    fixture_commands = metadata["protocol"]["decoder_fixture_commands"]
    if len(fixture_commands) != len(DECODER_SIZES):
        raise ValueError("decoder fixture command count changed")
    for command, (_, width, height, _) in zip(
            fixture_commands, DECODER_SIZES):
        if f"-w {width} -h {height}" not in str(command):
            raise ValueError("decoder fixture command dimensions changed")
    validate_budget(root / "thread-budget.csv", revision)
    for threads in (2, 4, 8):
        validate_encoder(root / f"encoder-thread{threads}.jsonl")
    encoder_scaling = metadata["protocol"]["encoder_scaling"]
    encoder_threads = tuple(
        int(value) for value in encoder_scaling["thread_counts"]
    )
    if encoder_threads != ENCODER_SCALING_THREADS:
        raise ValueError("encoder scaling does not cover threads 1 through 16")
    encoder_warmups = int(encoder_scaling["warmup_runs_per_thread"])
    encoder_repeats = int(encoder_scaling["timed_runs_per_thread"])
    if encoder_warmups != 2 or encoder_repeats != 9:
        raise ValueError("encoder scaling repetition protocol changed")
    if encoder_scaling["worker_count_order"] != (
            "alternating ascending and descending rounds"):
        raise ValueError("encoder scaling schedule protocol changed")
    if ("--threads={threads}" not in encoder_scaling["command"]
            or "-w 1920 -h 1080" not in encoder_scaling["command"]):
        raise ValueError("encoder scaling command template changed")
    validate_encoder_scaling(
        root / "encoder-thread-scaling.csv",
        revision,
        encoder_threads,
        encoder_warmups,
        encoder_repeats,
    )
    for _, _, _, log_name in DECODER_SIZES:
        validate_decoder(root / log_name)
    validate_decoder_sizes(
        root / "decoder-size-comparison.csv",
        root,
        revision,
    )
    scaling = metadata["protocol"]["decoder_scaling"]
    scaling_threads = tuple(int(value) for value in scaling["thread_counts"])
    if scaling_threads != DECODER_SCALING_THREADS:
        raise ValueError("decoder scaling does not cover threads 1 through 16")
    warmups = int(scaling["warmup_runs_per_thread"])
    if warmups != 2:
        raise ValueError("decoder scaling warmup protocol changed")
    if scaling["worker_count_order"] != (
            "alternating ascending and descending rounds"):
        raise ValueError("decoder scaling schedule protocol changed")
    if "--threads={threads}" not in scaling["command"]:
        raise ValueError("decoder scaling command template changed")
    repeats = int(scaling["timed_runs_per_thread"])
    if repeats != 9:
        raise ValueError("decoder scaling repeat protocol changed")
    validate_decoder_scaling(
        root / "decoder-thread-scaling.csv",
        revision,
        scaling_threads,
        warmups,
        repeats,
    )
    validate_animation(root / "animation-thread4.jsonl")
    print("threading measurement artifacts are consistent")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
