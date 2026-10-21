#!/usr/bin/env python3
"""Check recorded test wall times without rerunning tests or hiding setup cost."""

import argparse
import csv
from dataclasses import dataclass, replace
import json
import math
from pathlib import Path
import re
import sys


@dataclass(frozen=True)
class Timing:
    name: str
    milliseconds: float
    result: str
    boundary: str = "recorded"
    native_result: str = ""
    context: bool = False


def milliseconds(value):
    """Reject unknown, negative, and non-finite measurements instead of passing."""
    number = float(value)
    if not math.isfinite(number) or number < 0:
        raise ValueError("invalid duration: {!r}".format(value))
    return number


def read_timing_lines(text):
    """Read the common records used by Automake reports and batch TAP runners."""
    records = []
    lines = text.splitlines()
    # Global reports repeat failing TAP output; prefer their complete table.
    plain_records = any(line.startswith("test-time:") for line in lines)
    for line in lines:
        if plain_records and line.startswith("# "):
            continue
        if not re.match(r"^(?:# )?test-time:", line):
            continue
        match = re.fullmatch(r"(?:# )?test-time: (\S+) ms (\S+) (.+)", line)
        if not match:
            raise ValueError("malformed timing record: " + line)
        duration, result, name = match.groups()
        # A pre-execution capability skip has no test process to measure.
        if duration == "unmeasured" and result == "SKIP":
            continue
        records.append(Timing(name, milliseconds(duration), result))
    return records


def read_child_time(row, name, nested):
    """Validate timing evidence against generic registration, never TAP text.

    LSO_CHILD_TIME is captured in Meson's native env field even when the process
    never starts or is killed. Thus absence is not inferred from a marker or
    from a private test-path naming convention.
    """
    mode = row.get("env", {}).get("LSO_CHILD_TIME")
    markers = [line for stream in ("stdout", "stderr")
               for line in row.get(stream, "").splitlines()
               if line.startswith("# child-time:")]
    if mode != "1":
        if markers:
            raise ValueError("unexpected child-time marker without registration")
        if mode is not None and (not isinstance(mode, str) or
                                 not mode.startswith("outer-only: ")):
            raise ValueError("invalid child-time registration: " + repr(mode))
        return None
    # Interactive Meson inherits stdio rather than capturing it and reports
    # IGNORED, not a TAP verdict. It cannot supply captured child evidence.
    if not markers and row["result"] == "IGNORED":
        return None
    if len(markers) != 1:
        raise ValueError("expected one child-time marker, found {}".format(len(markers)))
    terminal = re.fullmatch(r"# child-time: 1 unmeasured terminal (.+)", markers[0])
    if terminal:
        if terminal[1] != name:
            raise ValueError("child-time identity mismatch: " + terminal[1])
        return None
    match = re.fullmatch(r"# child-time: 1 ([0-9]+(?:\.[0-9]+)?) ms "
                         r"([0-9]+) ([01]) (.+)", markers[0])
    if not match:
        raise ValueError("malformed/unsupported child-time marker")
    duration, exit_code, timed_out, identity = match.groups()
    if identity != name:
        raise ValueError("child-time identity mismatch: " + identity)
    exit_code = int(exit_code)
    if exit_code > 255 or type(row.get("returncode")) is not int:
        raise ValueError("invalid child-time/Meson exit status")
    if exit_code != row["returncode"]:
        raise ValueError("child-time exit differs from Meson returncode")
    if timed_out == "1" and exit_code != 124:
        raise ValueError("child-time timeout requires exit 124")
    elapsed = milliseconds(duration)
    # These are nested monotonic intervals; allow only rounding at the marker's
    # decimal precision. Do not subtract framework/GC cost or rewrite the log.
    if elapsed > milliseconds(row["duration"]) * 1000 + 0.000001:
        raise ValueError("child-time exceeds enclosing Meson duration")
    return Timing(name, elapsed, "BATCH" if nested else row["result"],
                  "child-spawn-through-output", row["result"])


def read_meson(text, evidence_errors=None, fallbacks=None):
    """Read native JSON Lines without modifying its result, duration or output.

    On missing/malformed child evidence the CLI still reports the original
    outer interval/verdict, then returns evidence-error status (not test FAIL).
    Library callers omitting evidence_errors receive the validation exception.
    """
    records = []
    for line in text.splitlines():
        if not line.strip():
            continue
        row = json.loads(line)
        name = row["name"]
        if not isinstance(name, str) or not name:
            raise ValueError("invalid Meson test name")
        name = re.sub(r"^(?:.* - )?libsixel:", "", name.rsplit(" / ", 1)[-1])
        nested = read_timing_lines(row.get("stdout", ""))
        records.append(Timing(name, milliseconds(milliseconds(row["duration"]) * 1000),
                              "BATCH" if nested else row["result"],
                              "meson-outer", row["result"]))
        try:
            child = read_child_time(row, name, nested)
            if child is not None:
                records[-1] = replace(records[-1], context=True)
                records.append(child)
            elif fallbacks is not None:
                mode = row.get("env", {}).get("LSO_CHILD_TIME")
                reason = mode or "outer-only: legacy/uninstrumented registration"
                if mode == "1":
                    reason = ("outer-only: Meson interactive output is not captured"
                              if row["result"] == "IGNORED" else
                              "outer-only: terminal command (child unmeasured)")
                fallbacks.append((name, reason))
        except (ValueError, TypeError, KeyError) as error:
            if evidence_errors is None:
                raise
            evidence_errors.append("{}: child unmeasured/evidence error: {} "
                                   "(native result {}, outer {:.3f} ms)".format(
                                       name, error, row["result"],
                                       records[-1].milliseconds))
        # Existing WIC setup and individual observations keep their identities,
        # values and verdicts; both surrounding intervals are non-additive.
        records.extend(nested)
    return records


DEFAULT_LIMIT_MS = 30.0


def read_allowlist(text):
    """Exceptions need an exact test ID, a reason, and recorded user approval."""
    rows = csv.DictReader(text.splitlines(), delimiter="\t")
    if rows.fieldnames != ["test", "limit_ms", "reason", "approval"]:
        raise ValueError("invalid test time allowlist header")
    limits = {}
    for row in rows:
        if set(row) != {"test", "limit_ms", "reason", "approval"} or any(value is None for value in row.values()):
            raise ValueError("invalid allowlist columns")
        name = row["test"]
        if not name or name != name.strip() or any(c in name for c in "*?[]"):
            raise ValueError("allowlist requires an exact test name")
        if name in limits:
            raise ValueError("duplicate allowlist entry: " + name)
        if not row["reason"] or not row["reason"].strip():
            raise ValueError("allowlist requires a reason: " + name)
        if not row["approval"] or not row["approval"].strip():
            raise ValueError("allowlist requires user approval: " + name)
        if not re.fullmatch(r"[0-9]+(?:\.[0-9]+)?", row["limit_ms"] or ""):
            raise ValueError("invalid allowlist limit: " + name)
        limit = milliseconds(row["limit_ms"])
        if not DEFAULT_LIMIT_MS < limit:
            raise ValueError("allowlist limit must be above 30 ms: " + name)
        limits[name] = limit
    return limits


def over_budget(records, limits):
    return sorted((record for record in records
                   if record.milliseconds > limits.get(record.name, DEFAULT_LIMIT_MS)),
                  key=lambda record: (-record.milliseconds, record.name))


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("logs", nargs="+", type=Path,
                        help="test-suite.log, batch TAP log, or Meson testlog.json")
    parser.add_argument("--allowlist", type=Path,
                        default=Path(__file__).resolve().parents[1] /
                        "tests/test-time-allowlist.tsv")
    args = parser.parse_args(argv)
    records = []
    evidence_errors = []
    fallbacks = []
    try:
        limits = read_allowlist(args.allowlist.read_text(encoding="utf-8"))
        for path in args.logs:
            text = path.read_text(encoding="utf-8", errors="replace")
            measured = (read_meson(text, evidence_errors, fallbacks)
                        if path.suffix == ".json" else read_timing_lines(text))
            if not measured:
                raise ValueError("{}: no measured tests; rerun tests with timing enabled".format(path))
            records.extend(measured)
    except (OSError, ValueError, KeyError, TypeError) as error:
        print("testtimecheck: {}".format(error), file=sys.stderr)
        return 2
    # Keep accepted exceptions visible so a whitelist never hides their cost.
    allowed = [record for record in records if record.name in limits
               and record.milliseconds <= limits[record.name]]
    for record in allowed:
        print("ALLOW {:.3f} ms (limit {:.3f}): {} [{}; boundary={}]".format(
            record.milliseconds, limits[record.name], record.name,
            record.result, record.boundary))
    slow = over_budget(records, limits)
    for record in slow:
        print("SLOW {:.3f} ms (limit {:.3f}): {} [{}; boundary={}; native={}]".format(
            record.milliseconds, limits.get(record.name, DEFAULT_LIMIT_MS),
            record.name, record.result, record.boundary,
            record.native_result or record.result))
    contexts = sum(record.result in ("SETUP", "BATCH") or
                   record.context for record in slow)
    print("Slow tests: {}; slow setup/batch processes: {}.".format(len(slow) - contexts, contexts))
    print("Checked {} recorded timings; {} exceed their budget "
          "(default 30 ms, explicit per-test exception budgets).".format(
              len(records), len(slow)))
    for boundary in sorted({record.boundary for record in records}):
        print("Boundary {}: {} recorded; {} over budget (non-additive).".format(
            boundary, sum(record.boundary == boundary for record in records),
            sum(record.boundary == boundary for record in slow)))
    for reason in sorted({reason for _, reason in fallbacks}):
        print("{} ({} actual commands; no child measurement).".format(
            reason, sum(value == reason for _, value in fallbacks)))
    for error in evidence_errors:
        print("testtimecheck: " + error, file=sys.stderr)
    # Timing is advisory; only malformed or missing evidence is an error.
    return 2 if evidence_errors else 0


if __name__ == "__main__":
    sys.exit(main())
