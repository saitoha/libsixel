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


def render(events: List[ParallelEvent], output: str) -> None:
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

    role_order = []
    for _, role, _ in threads:
        if role not in role_order:
            role_order.append(role)
    role_color = {role: f"C{index % 10}"
                  for index, role in enumerate(role_order)}

    rows = sorted(list(threads.keys()))
    fig_height = max(1.0, 0.6 * len(rows))
    fig, ax = plt.subplots(figsize=(12, fig_height))

    for idx, row in enumerate(rows):
        worker, role, thread = row
        spans_for_thread = threads[row]
        color = role_color.get(role, "C0")
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

    handles = []
    labels = []
    for role in role_order:
        label = role if role else "(unspecified)"
        handles.append(plt.Line2D([], [], color=role_color[role],
                                  marker="s", linestyle=""))
        labels.append(label)
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
    args = parser.parse_args()

    events = load_events(args.logfile)
    if not events:
        print("No events found in log")
        return
    render(events, args.output)


if __name__ == "__main__":
    main()
