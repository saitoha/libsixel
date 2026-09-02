#!/usr/bin/env python3
"""Generate the documented CI support inventory from CI configuration."""

import argparse
import difflib
import itertools
import pathlib
import re
import sys


JOB_RE = re.compile(r"^  ([A-Za-z0-9_.-]+):[ \t]*$")
MATRIX_RE = re.compile(r"^( *)matrix:[ \t]*$")
KEY_VALUE_RE = re.compile(r"^([A-Za-z0-9_.-]+):[ \t]*(.*)$")
INLINE_LIST_RE = re.compile(r"^\[(.*)\]$")


def scalar(value):
    """Normalize a simple YAML scalar used by the workflow matrices."""
    value = value.strip()
    if len(value) >= 2 and value[0] == value[-1] and value[0] in "\"'":
        return value[1:-1]
    return value


def split_inline_list(value):
    """Parse the simple inline lists used for matrix axes."""
    match = INLINE_LIST_RE.match(value.strip())
    if match is None:
        return None
    body = match.group(1).strip()
    if not body:
        return []
    return [scalar(item) for item in body.split(",")]


def job_blocks(lines):
    """Yield top-level GitHub Actions job blocks without a YAML dependency."""
    in_jobs = False
    current_name = None
    current_lines = []

    for line in lines:
        if line.rstrip() == "jobs:":
            in_jobs = True
            continue
        if not in_jobs:
            continue
        if line and not line.startswith((" ", "\t", "#", "\n")):
            break
        match = JOB_RE.match(line.rstrip("\n"))
        if match is not None:
            if current_name is not None:
                yield current_name, current_lines
            current_name = match.group(1)
            current_lines = []
            continue
        if current_name is not None:
            current_lines.append(line.rstrip("\n"))

    if current_name is not None:
        yield current_name, current_lines


def parse_include_rows(lines, start, matrix_indent):
    """Parse scalar mappings below a matrix include key."""
    include_indent = matrix_indent + 2
    row_indent = include_indent + 2
    value_indent = row_indent + 2
    rows = []
    current = None
    index = start

    while index < len(lines):
        line = lines[index]
        stripped = line.strip()
        indent = len(line) - len(line.lstrip(" "))
        if stripped and not stripped.startswith("#") and indent <= include_indent:
            break
        if indent == row_indent and stripped.startswith("- "):
            if current is not None:
                rows.append(current)
            current = {}
            item = stripped[2:]
            match = KEY_VALUE_RE.match(item)
            if match is not None:
                current[match.group(1)] = scalar(match.group(2))
        elif current is not None and indent == value_indent:
            match = KEY_VALUE_RE.match(stripped)
            if match is not None:
                current[match.group(1)] = scalar(match.group(2))
        index += 1

    if current is not None:
        rows.append(current)
    return rows


def matrix_configurations(job_name, lines):
    """Return stable display names for one job's matrix configurations."""
    axes = {}
    include_rows = []
    matrix_found = False
    index = 0

    while index < len(lines):
        match = MATRIX_RE.match(lines[index])
        if match is None:
            index += 1
            continue
        matrix_found = True
        matrix_indent = len(match.group(1))
        index += 1
        while index < len(lines):
            line = lines[index]
            stripped = line.strip()
            indent = len(line) - len(line.lstrip(" "))
            if stripped and not stripped.startswith("#") and indent <= matrix_indent:
                break
            if indent == matrix_indent + 2 and stripped == "include:":
                include_rows.extend(
                    parse_include_rows(lines, index + 1, matrix_indent)
                )
            elif indent == matrix_indent + 2:
                value_match = KEY_VALUE_RE.match(stripped)
                if value_match is not None:
                    values = split_inline_list(value_match.group(2))
                    if values is not None:
                        axes[value_match.group(1)] = values
            index += 1

    if not matrix_found:
        return []

    configurations = []
    for row in include_rows:
        if "label" in row:
            configurations.append("{}: {}".format(job_name, row["label"]))
            continue
        fields = ["{}={}".format(key, row[key]) for key in sorted(row)]
        configurations.append("{}: {}".format(job_name, ", ".join(fields)))

    if axes:
        keys = sorted(axes)
        for values in itertools.product(*(axes[key] for key in keys)):
            fields = ["{}={}".format(key, value)
                      for key, value in zip(keys, values)]
            configurations.append("{}: {}".format(job_name, ", ".join(fields)))

    return configurations


def action_configurations(root):
    """Collect build configurations from all GitHub Actions matrices."""
    result = {}
    workflow_dir = root / ".github" / "workflows"
    for path in sorted(workflow_dir.glob("*.y*ml")):
        lines = path.read_text(encoding="utf-8").splitlines(True)
        configurations = []
        for job_name, lines_for_job in job_blocks(lines):
            configurations.extend(
                matrix_configurations(job_name, lines_for_job)
            )
        if configurations:
            result[path.name] = sorted(set(configurations))
    return result


def local_configurations(path):
    """Read normalized job and runner-profile pairs from a TSV catalog."""
    result = []
    for number, line in enumerate(path.read_text(encoding="utf-8").splitlines(), 1):
        if not line or line.startswith("#"):
            continue
        fields = line.split("\t")
        if len(fields) < 2 or not fields[0] or not fields[1]:
            raise ValueError("{}:{}: expected job and runner profile".format(
                path, number))
        result.append((fields[0], fields[1]))
    return sorted(set(result))


def write_local_snapshot(path, configurations):
    """Write the normalized, non-operative local CI catalog snapshot."""
    lines = [
        "# Non-operative normalized snapshot of libsixel-ci/srv/misc/jobs.tsv.",
        "# job_name\trunner_profile",
    ]
    lines.extend("{}\t{}".format(job, profile)
                 for job, profile in configurations)
    path.write_text("\n".join(lines) + "\n", encoding="utf-8")


def render(root, local_snapshot):
    """Render the complete Markdown inventory."""
    actions = action_configurations(root)
    local = local_configurations(local_snapshot)
    action_total = sum(len(items) for items in actions.values())
    lines = [
        "# CI Support Matrix",
        "",
        "<!-- Generated by tests/_static/python/generate_ci_support_matrix.py. -->",
        "<!-- Update CI configuration, then regenerate this file. -->",
        "",
        "This inventory lists the build configurations represented by the",
        "GitHub Actions matrices and the @saitoha local desktop CI catalog.",
        "It records configured support; runner availability and an individual",
        "job's live result must still be checked in the relevant CI system.",
        "",
        "## GitHub Actions",
        "",
        "Source: `.github/workflows/*.yml`.",
        "",
        "Configured matrix entries: **{}**.".format(action_total),
        "",
    ]

    for workflow, configurations in actions.items():
        lines.extend([
            "### `{}`".format(workflow),
            "",
        ])
        lines.extend("- `{}`".format(item) for item in configurations)
        lines.append("")

    lines.extend([
        "## @saitoha Local Desktop CI",
        "",
        "Authoritative source: `libsixel-ci/srv/misc/jobs.tsv` in the",
        "companion `libsixel-ci` repository. The tracked",
        "`docs/ci/local-jobs.tsv` file is its normalized documentation",
        "snapshot and is checked against the authoritative catalog when that",
        "catalog is available in the maintainer workspace.",
        "",
        "Configured jobs: **{}**.".format(len(local)),
        "",
    ])
    lines.extend(
        "- `{}` — runner profile `{}`".format(job, profile)
        for job, profile in local
    )
    lines.append("")
    return "\n".join(lines)


def show_diff(expected, actual, from_name, to_name):
    """Write a unified diff to stderr."""
    sys.stderr.writelines(difflib.unified_diff(
        actual.splitlines(True),
        expected.splitlines(True),
        fromfile=from_name,
        tofile=to_name,
    ))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", required=True, type=pathlib.Path)
    parser.add_argument("--local-snapshot", required=True, type=pathlib.Path)
    parser.add_argument("--check", type=pathlib.Path)
    parser.add_argument("--write", type=pathlib.Path)
    parser.add_argument("--check-local-catalog", type=pathlib.Path)
    parser.add_argument("--sync-local-catalog", type=pathlib.Path)
    args = parser.parse_args()

    root = args.root.resolve()
    snapshot = args.local_snapshot.resolve()
    if args.sync_local_catalog is not None:
        write_local_snapshot(
            snapshot,
            local_configurations(args.sync_local_catalog.resolve()),
        )
    expected = render(root, snapshot)

    if args.check is not None:
        actual = args.check.read_text(encoding="utf-8")
        if actual != expected:
            show_diff(expected, actual, str(args.check), "generated inventory")
            return 1

    if args.check_local_catalog is not None:
        tracked = local_configurations(snapshot)
        current = local_configurations(args.check_local_catalog.resolve())
        if tracked != current:
            tracked_text = "".join("{}\t{}\n".format(*item)
                                   for item in tracked)
            current_text = "".join("{}\t{}\n".format(*item)
                                   for item in current)
            show_diff(current_text, tracked_text,
                      str(snapshot), str(args.check_local_catalog))
            return 1

    if args.write is not None:
        args.write.write_text(expected, encoding="utf-8")
    elif args.check is None:
        sys.stdout.write(expected)
    return 0


if __name__ == "__main__":
    sys.exit(main())
