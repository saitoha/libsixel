#!/usr/bin/env python3
"""Build a timeline chart from SIXEL_PARALLEL_LOG_PATH output.

The logger writes JSON lines like:
{"ts":0.001,"thread":1234,"worker":"dither","event":"start",...}

Usage:
  python tools/parallel_log_timeline.py /path/to/log --output timeline.png

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
        "encode": 6,
        "pipeline": 7,
    }

    rank = priority.get(worker, 10)
    return (rank, role, thread)


def _start_sort_key(
    row: Tuple[str, str, int],
    first_ts: Dict[Tuple[str, str, int], float],
) -> Tuple[float, str, str, int]:
    """Sort rows by the first timestamp seen, then by identifiers."""

    return (first_ts.get(row, 0.0), row[0], row[1], row[2])


def render(events: List[ParallelEvent], output: str, sort_order: str) -> None:
    try:
        import matplotlib.pyplot as plt
    except ImportError:
        print("matplotlib is not available; printing summary instead")
        print(summarize(events))
        return

    events = sorted(events, key=lambda item: item.ts)

    spans: Dict[Tuple[str, str, int, int], Tuple[float, float]] = {}
    for event in events:
        key = (event.worker, event.role, event.thread, event.job)
        if key not in spans:
            spans[key] = (event.ts, event.ts)
        else:
            start, _ = spans[key]
            spans[key] = (start, max(spans[key][1], event.ts))

    threads: Dict[Tuple[str, str, int], List[Tuple[float, float, int]]] = {}
    for (worker, role, thread, job), (start, end) in spans.items():
        duration = max(0.0, end - start)
        key = (worker, role, thread)
        if key not in threads:
            threads[key] = []
        threads[key].append((start, duration, job))

    for key in threads:
        threads[key].sort(key=lambda item: item[0])

    # Base hues per flow so encode/dither/pipeline stay visually grouped.
    flow_palette = {
        "loader": (0.737, 0.494, 0.247),
        "scale": (0.556, 0.352, 0.690),
        "crop": (0.741, 0.333, 0.447),
        "colorspace": (0.243, 0.580, 0.580),
        "palette": (0.580, 0.517, 0.278),
        "dither": (0.231, 0.627, 0.231),
        "encode": (0.129, 0.4, 0.674),
        "pipeline": (0.4, 0.4, 0.4),
    }

    # Shade roles within a flow to keep related rows distinguishable.
    role_shades = {
        "controller": 0.15,
        "scheduler": 0.3,
        "producer": 0.45,
        "worker": 0.6,
        "writer": 0.75,
    }

    role_color = {}
    for worker, role, _ in threads:
        base = flow_palette.get(worker, (0.2, 0.2, 0.2))
        shade = role_shades.get(role, 0.6)
        role_color[(worker, role)] = _lighten(base, shade)

    rows = list(threads.keys())
    first_ts: Dict[Tuple[str, str, int], float] = {}
    for event in events:
        key = (event.worker, event.role, event.thread)
        if key not in first_ts:
            first_ts[key] = event.ts

    if sort_order == "start":
        rows.sort(key=lambda row: _start_sort_key(row, first_ts))
    else:
        rows.sort(key=lambda row: _flow_sort_key(row[0], row[1], row[2]))
    fig_height = max(1.0, 0.6 * len(rows))
    fig, ax = plt.subplots(figsize=(12, fig_height))

    for idx, row in enumerate(rows):
        worker, role, thread = row
        spans_for_thread = threads[row]
        color = role_color.get((worker, role), (0.2, 0.2, 0.2))
        segments = []
        for start, duration, _ in spans_for_thread:
            segments.append((start, duration if duration > 0 else 1e-6))
        ax.broken_barh(segments,
                       (idx - 0.4, 0.8),
                       facecolors=color,
                       edgecolors="black",
                       linewidth=0.5)

        for start, duration, job in spans_for_thread:
            if worker != "dither":
                continue
            xpos = start + (duration if duration > 0 else 1e-6) / 2.0
            ax.text(xpos,
                    idx,
                    f"#{job}",
                    ha="center",
                    va="center",
                    fontsize=6,
                    color="black")

    yticks = [pos for pos in range(len(rows))]
    ylabels = []
    for worker, role, thread in rows:
        if role:
            ylabels.append(f"{worker}/{role}[{thread}]")
        else:
            ylabels.append(f"{worker}[{thread}]")
    ax.set_yticks(yticks)
    ax.set_yticklabels(ylabels)
    ax.set_xlabel("seconds")
    ax.set_title("Parallel pipeline timeline (grouped by thread)")
    ax.grid(True, axis="x", linestyle=":", linewidth=0.5)
    ax.set_ylim(-0.5, len(rows) - 0.5)
    ax.invert_yaxis()

    handles = []
    labels = []
    seen_keys = set()
    for worker, role, _ in threads:
        key = (worker, role)
        if key in seen_keys:
            continue
        seen_keys.add(key)
        label_worker = worker if worker else "(unknown)"
        label_role = role if role else "(unspecified)"
        handles.append(plt.Line2D([], [], color=role_color[key],
                                  marker="s", linestyle=""))
        labels.append(f"{label_worker}/{label_role}")
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
    args = parser.parse_args()

    events = load_events(args.logfile)
    if not events:
        print("No events found in log")
        return
    render(events, args.output, args.sort_order)


if __name__ == "__main__":
    main()
