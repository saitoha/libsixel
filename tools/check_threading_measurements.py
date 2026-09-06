#!/usr/bin/env python3
"""Validate checked-in thread-budget and timeline measurement artifacts."""

from __future__ import annotations

import argparse
import csv
import json
from pathlib import Path
from typing import Dict, Iterable, List, Sequence, Tuple


THREADS = tuple(range(1, 13))
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
    "decoder-size-comparison.csv",
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
        if threads == 1:
            if row["planner_mode"] != "serial" or dither != 0 or encode != 0:
                raise ValueError("one-thread planner contract changed")
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
    row4 = rows[3]
    if int(row4["observed_parallel_dither_threads"]) != 2:
        raise ValueError("four-thread run did not create two dither workers")
    if int(row4["observed_encode_pool_threads"]) < 2:
        raise ValueError("four-thread run did not exercise two encode workers")


def validate_decoder(path: Path) -> None:
    """Validate parallel scan plus ordered paint timeline structure."""
    records = load_jsonl(path)
    scan = paired_intervals(records, "decoder", "scan", "start", "finish")
    paint = paired_intervals(records, "decoder", "paint", "start", "finish")
    if len(scan) != 8 or len(paint) != 8:
        raise ValueError("decoder timeline does not contain eight scan/paint spans")
    if min(start for start, _ in paint) < max(end for _, end in scan):
        raise ValueError("decoder paint started before the validation scan joined")
    for previous, current in zip(paint, paint[1:]):
        if current[0] < previous[1]:
            raise ValueError("decoder paint spans are no longer serialized")


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
    if int(metadata["schema_version"]) != 2:
        raise ValueError("threading metadata schema is not version 2")
    revision = str(metadata["source"]["revision"])
    if metadata["source"]["tracked_worktree_state_at_start"] != "clean":
        raise ValueError("threading artifacts were not recorded from clean source")
    decoder_input = metadata["inputs"]["decoder"]
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
    for _, _, _, log_name in DECODER_SIZES:
        validate_decoder(root / log_name)
    validate_decoder_sizes(
        root / "decoder-size-comparison.csv",
        root,
        revision,
    )
    validate_animation(root / "animation-thread4.jsonl")
    print("threading measurement artifacts are consistent")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
