#!/usr/bin/env python3
"""Build a timeline chart from SIXEL_PARALLEL_LOG_PATH output.

The logger writes JSON lines like:
{"ts":0.001,"thread":1234,"worker":"dither","event":"start",...}

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


class ParallelEvent:
    def __init__(self, record: Dict[str, object]):
        self.ts = float(record.get("ts", 0.0))
        self.thread = int(record.get("thread", -1))
        self.role = str(record.get("role", ""))
        self.worker = str(record.get("worker", ""))
        self.event = str(record.get("event", ""))
        self.job = int(record.get("job", -1))
        self.row = int(record.get("row", -1))
        self.y0 = int(record.get("y0", -1))
        self.y1 = int(record.get("y1", -1))
        self.in0 = int(record.get("in0", -1))
        self.in1 = int(record.get("in1", -1))
        self.message = str(record.get("message", ""))


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
    row: Tuple[str, int],
    first_ts: Dict[Tuple[str, int], float],
    primary_role: Dict[Tuple[str, int], str],
) -> Tuple[float, str, str, int]:
    """Sort rows by the first timestamp seen, then by identifiers."""

    return (
        first_ts.get(row, 0.0),
        row[0],
        primary_role.get(row, ""),
        row[1],
    )


def _row_key(worker: str, thread: int, job: int) -> Tuple[str, int]:
    """Choose a row key so decoder workers use the job index ordering."""

    if worker == "decoder" and job >= 0:
        return (worker, job)
    return (worker, thread)


def render(
    events: List[ParallelEvent],
    output: str,
    sort_order: str,
    lifetime_only: bool = False,
    start_time: float = None,
    end_time: float = None,
) -> None:
    events = sorted(events, key=lambda item: item.ts)
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

    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib is not available; printing summary instead")
        print(summarize(summary_events))
        return

    spans: Dict[Tuple[str, str, int, int], Tuple[float, float]] = {}
    job_hint: Dict[Tuple[str, int], int] = {}
    first_ts: Dict[Tuple[str, int], float] = {}
    primary_role: Dict[Tuple[str, int], str] = {}
    for event in events:
        key = (event.worker, event.role, event.thread, event.job)
        if key not in spans:
            spans[key] = (event.ts, event.ts)
        else:
            start, _ = spans[key]
            spans[key] = (start, max(spans[key][1], event.ts))
        row_key = _row_key(event.worker, event.thread, event.job)
        if row_key not in job_hint and event.worker == "decoder" and \
                event.job >= 0:
            job_hint[row_key] = event.job
        if row_key not in first_ts:
            first_ts[row_key] = event.ts
            primary_role[row_key] = event.role

    threads: Dict[Tuple[str, int], List[Tuple[float, float, str, int]]] = {}
    if lifetime_only:
        row_spans: Dict[Tuple[str, int], Tuple[float, float]] = {}
        for (worker, _role, thread, job), (start, end) in spans.items():
            row_key = _row_key(worker, thread, job)
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
            threads[row_key].append((start, duration, role, -1))
    else:
        # Collapse decode/copy phases per thread so one row shows the full
        # worker activity while keeping per-role colors.
        for (worker, role, thread, job), (start, end) in spans.items():
            row_key = _row_key(worker, thread, job)
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
            threads[row_key].append((start, duration, role, job))

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
    for worker, thread in threads:
        seen_roles = set(role for _, _, role, _ in threads[(worker, thread)])
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
            row[0], primary_role.get(row, ""), row[1]))
    fig_height = max(1.0, 0.6 * len(rows))
    fig, ax = plt.subplots(figsize=(12, fig_height))

    for idx, row in enumerate(rows):
        worker, thread = row
        spans_for_thread = threads[row]
        segments = []
        colors = []
        for start, duration, role, _ in spans_for_thread:
            span = (start, duration if duration > 0 else 1e-6)
            segments.append(span)
            colors.append(role_color.get((worker, role), (0.2, 0.2, 0.2)))
        ax.broken_barh(segments,
                       (idx - 0.4, 0.8),
                       facecolors=colors,
                       edgecolors="black",
                       linewidth=0.5)

    yticks = [pos for pos in range(len(rows))]
    ylabels = []
    for worker, thread in rows:
        label = worker
        if worker == "decoder":
            label = f"{worker} #{thread}"
        ylabels.append(label)
    ax.set_yticks(yticks)
    ax.set_yticklabels(ylabels)
    ax.set_xlabel("seconds")
    if start_time is not None or end_time is not None:
        left = start_time if start_time is not None else events[0].ts
        right = end_time if end_time is not None else events[-1].ts
        ax.set_xlim(left, right)
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
        help="Path to SIXEL_PARALLEL_LOG_PATH output",
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
           end_time=args.end_time)


if __name__ == "__main__":
    main()
