#!/usr/bin/env python3
"""Build a timeline chart from SIXEL_LOG_PATH output.

The logger writes JSON lines like:
{"ts":0.001,"thread":1234,"worker":"dither","event":"start",...}
Animated pipelines can also include frame metadata keys:
{"frame_no":3,"loop_no":0,"multiframe":1}

Usage:
  python tools/timeline.py /path/to/log --output timeline.png

If matplotlib is unavailable, the script prints a textual summary instead of
writing an image. """

import argparse
import json
from collections import defaultdict
from typing import Dict, List, Tuple


def _lighten(color: Tuple[float, float, float], factor: float) -> Tuple[float,
                                                                        float,
                                                                        float]:
    """Lighten a color towards white by the given factor."""

    return tuple(channel + (1.0 - channel) * factor for channel in color)


def _darken(color: Tuple[float, float, float],
            factor: float) -> Tuple[float, float, float]:
    """Darken a color towards black by the given factor."""

    return tuple(channel * (1.0 - factor) for channel in color)


class ParallelEvent:
    def __init__(self, record: Dict[str, object]):
        self.ts = float(record.get("ts", 0.0))
        self.thread = int(record.get("thread", -1))
        self.role = str(record.get("role", ""))
        self.worker = str(record.get("worker", ""))
        self.event = str(record.get("event", ""))
        self.job = int(record.get("job", -1))
        self.frame_no = int(record.get("frame_no", -1))
        self.loop_no = int(record.get("loop_no", -1))
        self.multiframe = int(record.get("multiframe", 0))


def load_events(path: str) -> List[ParallelEvent]:
    events: List[ParallelEvent] = []
    with open(path, "r", encoding="utf-8") as handle:
        for line in handle:
            line = line.strip()
            if not line:
                continue
            try:
                record = json.loads(line)
            except json.JSONDecodeError:
                continue
            events.append(ParallelEvent(record))
    return events


def summarize(events: List[ParallelEvent]) -> str:
    counts: Dict[Tuple[str, str], int] = defaultdict(int)
    for event in events:
        counts[(event.worker, event.event)] += 1
    lines: List[str] = []
    for (worker, name), count in sorted(counts.items()):
        lines.append(f"{worker}: {name} -> {count} events")
    return "\n".join(lines)


def _flow_sort_key(worker: str, role: str, thread: int) -> Tuple[int, str, int]:
    """Return a stable sort key that follows the pipeline flow.

    Dither comes first, followed by encode/writer roles, then everything
    else.  Within the same worker bucket we fall back to role and thread
    number to keep ordering deterministic without hiding any rows.
    """

    priority: Dict[str, int] = {
        "loader": 0,
        "scale": 1,
        "crop": 2,
        "colorspace": 3,
        "palette": 4,
        "dither": 5,
        "decoder": 6,
        "encode": 7,
        "io": 8,
        "pipeline": 9,
    }

    rank = priority.get(worker, 10)
    return (rank, role, thread)


def _start_sort_key(
    row: Tuple[str, int, int, int],
    first_ts: Dict[Tuple[str, int, int, int], float],
    primary_role: Dict[Tuple[str, int, int, int], str],
) -> Tuple[float, str, str, int, int, int]:
    """Sort rows by the first timestamp seen, then by identifiers."""

    return (
        first_ts.get(row, 0.0),
        row[0],
        primary_role.get(row, ""),
        row[1],
        row[2],
        row[3],
    )


def _row_key(worker: str,
             thread: int,
             job: int,
             frame_no: int,
             loop_no: int,
             split_by_frame: bool) -> Tuple[str, int, int, int]:
    """Choose a row key with optional frame grouping."""

    key_frame_no = -1
    key_loop_no = -1
    if split_by_frame and frame_no >= 0:
        key_frame_no = frame_no
        key_loop_no = loop_no
    if worker == "decoder" and job >= 0:
        return (worker, job, key_frame_no, key_loop_no)
    return (worker, thread, key_frame_no, key_loop_no)


def _resolve_frame_render_mode(events: List[ParallelEvent],
                               frame_mode: str) -> Tuple[bool, bool]:
    """Resolve frame rendering as (split_rows_by_frame, annotate_frames)."""

    unique_frames = {
        (event.loop_no, event.frame_no)
        for event in events
        if event.frame_no >= 0
    }
    has_multi_frames = len(unique_frames) > 1

    if frame_mode == "off":
        return (False, False)
    if frame_mode == "on":
        return (True, True)
    if frame_mode == "compact":
        return (False, True)
    return (has_multi_frames, has_multi_frames)


def _impute_missing_frame_metadata(events: List[ParallelEvent],
                                   frame_mode: str) -> None:
    """Fill missing frame metadata so frame-aware rendering stays coherent.

    Many modules still emit timeline events without frame metadata.  In
    frame-aware modes we infer missing values from nearby events so one chart
    does not mix tagged and untagged spans.
    """

    forward_thread_frame: Dict[int, Tuple[int, int, int]] = {}
    backward_thread_frame: Dict[int, Tuple[int, int, int]] = {}
    forward_global_frame: Tuple[int, int, int] = None
    backward_global_frame: Tuple[int, int, int] = None

    if frame_mode == "off":
        return
    if not any(event.frame_no >= 0 for event in events):
        return

    for event in events:
        if event.frame_no >= 0:
            forward_thread_frame[event.thread] = (
                event.frame_no,
                event.loop_no,
                event.multiframe,
            )
            forward_global_frame = (
                event.frame_no,
                event.loop_no,
                event.multiframe,
            )
            continue
        inferred = forward_thread_frame.get(event.thread)
        if inferred is None:
            inferred = forward_global_frame
        if inferred is not None:
            event.frame_no = inferred[0]
            event.loop_no = inferred[1]
            event.multiframe = inferred[2]

    for event in reversed(events):
        if event.frame_no >= 0:
            backward_thread_frame[event.thread] = (
                event.frame_no,
                event.loop_no,
                event.multiframe,
            )
            backward_global_frame = (
                event.frame_no,
                event.loop_no,
                event.multiframe,
            )
            continue
        inferred = backward_thread_frame.get(event.thread)
        if inferred is None:
            inferred = backward_global_frame
        if inferred is not None:
            event.frame_no = inferred[0]
            event.loop_no = inferred[1]
            event.multiframe = inferred[2]


def _update_row_frame_bounds(
    row_frame_bounds: Dict[Tuple[str, int, int, int], Tuple[int, int, int,
                                                            int]],
    row_key: Tuple[str, int, int, int],
    frame_no: int,
    loop_no: int,
) -> None:
    """Track (min_loop, min_frame, max_loop, max_frame) for each row."""

    current = row_frame_bounds.get(row_key)
    if frame_no < 0:
        return
    if current is None:
        row_frame_bounds[row_key] = (loop_no, frame_no, loop_no, frame_no)
        return

    min_loop, min_frame, max_loop, max_frame = current
    if (loop_no, frame_no) < (min_loop, min_frame):
        min_loop = loop_no
        min_frame = frame_no
    if (loop_no, frame_no) > (max_loop, max_frame):
        max_loop = loop_no
        max_frame = frame_no
    row_frame_bounds[row_key] = (min_loop, min_frame, max_loop, max_frame)


def _frame_variant_color(color: Tuple[float, float, float],
                         frame_no: int,
                         loop_no: int) -> Tuple[float, float, float]:
    """Apply a deterministic frame-specific tint while preserving role hue."""

    phase = 0
    if frame_no < 0:
        return color

    phase = (frame_no + loop_no * 11) % 6
    if phase < 3:
        return _lighten(color, 0.08 + 0.08 * phase)
    return _darken(color, 0.08 + 0.06 * (phase - 3))


def render(
    events: List[ParallelEvent],
    output: str,
    sort_order: str,
    lifetime_only: bool = False,
    start_time: float = None,
    end_time: float = None,
    frame_mode: str = "auto",
) -> None:
    events = sorted(events, key=lambda item: item.ts)
    _impute_missing_frame_metadata(events, frame_mode)
    visible_events = []
    for event in events:
        if start_time is not None and event.ts < start_time:
            continue
        if end_time is not None and event.ts > end_time:
            continue
        visible_events.append(event)
    summary_events = visible_events if (start_time is not None or
                                        end_time is not None) else events
    if not summary_events:
        print("No events found in the selected time window")
        return
    split_by_frame, annotate_frames = _resolve_frame_render_mode(
        summary_events,
        frame_mode)

    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib is not available; printing summary instead")
        print(summarize(summary_events))
        return

    spans: Dict[Tuple[str, str, int, int, int, int], Tuple[float, float]] = {}
    job_hint: Dict[Tuple[str, int, int, int], int] = {}
    first_ts: Dict[Tuple[str, int, int, int], float] = {}
    primary_role: Dict[Tuple[str, int, int, int], str] = {}
    row_frame_bounds: Dict[Tuple[str, int, int, int], Tuple[int, int, int,
                                                             int]] = {}
    for event in events:
        span_frame_no = event.frame_no if annotate_frames else -1
        span_loop_no = event.loop_no if annotate_frames else -1
        key = (event.worker,
               event.role,
               event.thread,
               event.job,
               span_frame_no,
               span_loop_no)
        if key not in spans:
            spans[key] = (event.ts, event.ts)
        else:
            start, _ = spans[key]
            spans[key] = (start, max(spans[key][1], event.ts))
        row_key = _row_key(event.worker,
                           event.thread,
                           event.job,
                           event.frame_no,
                           event.loop_no,
                           split_by_frame)
        _update_row_frame_bounds(row_frame_bounds,
                                 row_key,
                                 event.frame_no,
                                 event.loop_no)
        if row_key not in job_hint and event.worker == "decoder" and \
                event.job >= 0:
            job_hint[row_key] = event.job
        if row_key not in first_ts:
            first_ts[row_key] = event.ts
            primary_role[row_key] = event.role

    threads: Dict[Tuple[str, int, int, int],
                  List[Tuple[float, float, str, int, int, int]]] = {}
    if lifetime_only:
        row_spans: Dict[Tuple[str, int, int, int], Tuple[float, float]] = {}
        for (worker, _role, thread, job, frame_no, loop_no), \
                (start, end) in spans.items():
            row_key = _row_key(worker,
                               thread,
                               job,
                               frame_no,
                               loop_no,
                               split_by_frame)
            if row_key not in row_spans:
                row_spans[row_key] = (start, end)
            else:
                row_start, row_end = row_spans[row_key]
                row_spans[row_key] = (row_start, max(row_end, end))
        for row_key, (start, end) in row_spans.items():
            if start_time is not None and end < start_time:
                continue
            if end_time is not None and start > end_time:
                continue
            if start_time is not None:
                start = max(start, start_time)
            if end_time is not None:
                end = min(end, end_time)
            duration = max(0.0, end - start)
            role = primary_role.get(row_key, "")
            if row_key not in threads:
                threads[row_key] = []
            threads[row_key].append((start, duration, role, -1, -1, -1))
    else:
        # Collapse decode/copy phases per thread so one row shows the full
        # worker activity while keeping per-role colors.
        for (worker, role, thread, job, frame_no, loop_no), \
                (start, end) in spans.items():
            row_key = _row_key(worker,
                               thread,
                               job,
                               frame_no,
                               loop_no,
                               split_by_frame)
            duration = max(0.0, end - start)
            if start_time is not None and end < start_time:
                continue
            if end_time is not None and start > end_time:
                continue
            if start_time is not None:
                start = max(start, start_time)
            if end_time is not None:
                end = min(end, end_time)
            duration = max(0.0, end - start)
            if row_key not in threads:
                threads[row_key] = []
            threads[row_key].append((start, duration, role, job,
                                     frame_no, loop_no))

    for key in threads:
        threads[key].sort(key=lambda item: item[0])

    flow_palette = {
        "loader": (0.737, 0.494, 0.247),
        "scale": (0.556, 0.352, 0.690),
        "crop": (0.741, 0.333, 0.447),
        "colorspace": (0.243, 0.580, 0.580),
        "palette": (0.580, 0.517, 0.278),
        "dither": (0.231, 0.627, 0.231),
        "encode": (0.129, 0.4, 0.674),
        "pipeline": (0.4, 0.4, 0.4),
        "decoder": (0.278, 0.525, 0.847),
        "io": (0.725, 0.278, 0.525),
        "undither": (0.325, 0.533, 0.376),
        "png": (0.584, 0.376, 0.6),
    }

    role_palette = {
        "controller": 0.15,
        "decode": 0.35,
        "copy": 0.55,
        "writer": 0.7,
        "io": 0.25,
        "undither": 0.45,
        "png": 0.65,
        # Palette sub-phases stay in the same hue while darkening
        # initialization and brightening merge/export.  This keeps all
        # palette bars visually grouped but shows init/merge as distinct
        # shades on the timeline.
        "palette/init": 0.35,
        "palette/iterate": 0.55,
        "palette/merge": 0.75,
        "palette/export": 0.65,
    }

    role_color: Dict[Tuple[str, str], Tuple[float, float, float]] = {}
    for row_key in threads:
        worker = row_key[0]
        seen_roles = set(role for _, _, role, _, _, _ in threads[row_key])
        for role in seen_roles:
            base = flow_palette.get(worker, (0.2, 0.2, 0.2))
            shade = role_palette.get(role, 0.6)
            role_color[(worker, role)] = _lighten(base, shade)

    rows = list(threads.keys())
    if sort_order == "start":
        rows.sort(key=lambda row: _start_sort_key(
            row, first_ts, primary_role))
    else:
        rows.sort(key=lambda row: _flow_sort_key(
            row[0], primary_role.get(row, ""), row[1]) + (row[3], row[2]))
    fig_height = max(1.0, 0.6 * len(rows))
    fig, ax = plt.subplots(figsize=(12, fig_height))

    for idx, row in enumerate(rows):
        worker = row[0]
        spans_for_thread = threads[row]
        segments = []
        colors = []
        for start, duration, role, _, frame_no, loop_no in spans_for_thread:
            span = (start, duration if duration > 0 else 1e-6)
            segments.append(span)
            base = role_color.get((worker, role), (0.2, 0.2, 0.2))
            if annotate_frames:
                colors.append(_frame_variant_color(base, frame_no, loop_no))
            else:
                colors.append(base)
        ax.broken_barh(segments,
                       (idx - 0.4, 0.8),
                       facecolors=colors,
                       edgecolors="black",
                       linewidth=0.5)

    yticks = [pos for pos in range(len(rows))]
    ylabels = []
    for worker, slot, frame_no, loop_no in rows:
        label = worker
        if worker == "decoder":
            label = f"{worker} #{slot}"
        if split_by_frame and frame_no >= 0:
            label = f"{label} [L{loop_no} F{frame_no}]"
        elif annotate_frames:
            bounds = row_frame_bounds.get((worker, slot, frame_no, loop_no))
            if bounds is not None:
                min_loop, min_frame, max_loop, max_frame = bounds
                if (min_loop, min_frame) == (max_loop, max_frame):
                    label = f"{label} [L{min_loop} F{min_frame}]"
                else:
                    label = (f"{label} [L{min_loop} F{min_frame}.."
                             f"L{max_loop} F{max_frame}]")
        ylabels.append(label)
    ax.set_yticks(yticks)
    ax.set_yticklabels(ylabels)
    ax.set_xlabel("seconds")
    if start_time is not None or end_time is not None:
        left = start_time if start_time is not None else events[0].ts
        right = end_time if end_time is not None else events[-1].ts
        ax.set_xlim(left, right)
    if split_by_frame:
        ax.set_title("Parallel pipeline timeline (grouped by frame/thread)")
    elif annotate_frames:
        ax.set_title("Parallel pipeline timeline (thread rows, frame-colored)")
    else:
        ax.set_title("Parallel pipeline timeline (grouped by thread)")
    ax.grid(True, axis="x", linestyle=":", linewidth=0.5)
    ax.set_ylim(-0.5, len(rows) - 0.5)
    ax.invert_yaxis()

    handles = []
    labels = []
    seen_roles = set()
    for (worker, role), color in sorted(role_color.items(),
                                        key=lambda item: (item[0][1],
                                                          item[0][0])):
        if role in seen_roles:
            continue
        seen_roles.add(role)
        handles.append(plt.Line2D([], [], color=color, marker="s",
                                  linestyle=""))
        labels.append(role if role else "(unspecified)")
    if handles:
        ax.legend(handles, labels, title="Role", loc="upper right")

    fig.tight_layout()
    fig.savefig(output)
    plt.close(fig)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "logfile",
        help="Path to SIXEL_LOG_PATH output",
    )
    parser.add_argument(
        "--output",
        default="timeline.png",
        help="Output image path (default: timeline.png)",
    )
    parser.add_argument(
        "--sort-order",
        choices=["flow", "start"],
        default="flow",
        help=(
            "Row ordering: 'flow' keeps the conceptual pipeline order with "
            "dither above encode, while 'start' sorts by first activity"
        ),
    )
    parser.add_argument(
        "--start-time",
        type=float,
        default=None,
        help=(
            "Only show events occurring at or after this timestamp (seconds) "
            "to zoom into a portion of the log"
        ),
    )
    parser.add_argument(
        "--end-time",
        type=float,
        default=None,
        help=(
            "Only show events occurring at or before this timestamp (seconds) "
            "to zoom into a portion of the log"
        ),
    )
    parser.add_argument(
        "--lifetime-only",
        action="store_true",
        help=(
            "Render each worker/thread row using only the first and last "
            "timestamps seen so you can view the active window without "
            "showing every job"
        ),
    )
    parser.add_argument(
        "--frame-mode",
        choices=["auto", "off", "on", "compact"],
        default="auto",
        help=(
            "Frame grouping policy: 'auto' enables frame rows when logs "
            "contain multiple frame IDs, 'on' always splits rows by frame, "
            "'compact' keeps one row per thread and colors spans by frame, "
            "and 'off' keeps legacy worker/thread grouping"
        ),
    )
    args = parser.parse_args()

    events = load_events(args.logfile)
    if not events:
        print("No events found in log")
        return
    render(events,
           args.output,
           args.sort_order,
           lifetime_only=args.lifetime_only,
           start_time=args.start_time,
           end_time=args.end_time,
           frame_mode=args.frame_mode)


if __name__ == "__main__":
    main()
