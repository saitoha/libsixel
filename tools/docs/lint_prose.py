# SPDX-License-Identifier: MIT
"""Inventory validation, classification scopes and summaries for optional Vale."""

from __future__ import annotations

import argparse
from collections import Counter
import csv
import json
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[2]
VALE_DIR = ROOT / "tools/docs/vale"
INVENTORY = ROOT / "tools/docs/doc-types.tsv"
FIELDS = ["path", "type", "audience", "note"]
TYPES = {"tutorial", "how-to", "reference", "explanation"}
AUDIENCES = {"user", "contributor", "both"}
RULES = [
    "Libsixel.Terminology",
    "Libsixel.ContributorPaths",
    "Libsixel.CommitReferences",
    "Libsixel.ChangeLogPhrasing",
    "Libsixel.LocalProtocol",
]


def read_inventory():
    """Compare Git's index, not filesystem globbing, with the curated inventory."""
    tracked = subprocess.check_output(
        ["git", "ls-files", "-z", "--", "docs"], cwd=ROOT
    ).decode("utf-8").split("\0")
    tracked = {path for path in tracked if path.endswith(".md")}
    rows = {}
    problems = []
    with INVENTORY.open(encoding="utf-8", newline="") as source:
        reader = csv.DictReader(source, delimiter="\t")
        if reader.fieldnames != FIELDS:
            problems.append("expected TSV header: " + " / ".join(FIELDS))
        for line, row in enumerate(reader, 2):
            if set(row) != set(FIELDS) or any(value is None for value in row.values()):
                problems.append(f"line {line}: expected exactly four columns")
                continue
            path = row["path"]
            if path in rows:
                problems.append(f"line {line}: duplicate path: {path}")
            if row["type"] not in TYPES or row["audience"] not in AUDIENCES:
                problems.append(f"line {line}: invalid type or audience: {path}")
                continue
            if row["audience"] == "both" and not row["note"].strip():
                problems.append(f"line {line}: mixed audience needs a note: {path}")
            rows[path] = row
    problems.extend(f"missing from inventory: {path}" for path in sorted(tracked - rows.keys()))
    problems.extend(f"not a tracked Markdown document: {path}" for path in sorted(rows.keys() - tracked))
    if problems:
        print(f"Inventory: {len(problems)} advisory problem(s)")
        for problem in problems:
            print(f"  {problem}")
    else:
        print(f"Inventory: OK ({len(tracked)} tracked Markdown documents)")
    # Extra/invalid records never become INI section names. Unclassified files
    # retain the base terminology/protocol checks but no audience assumptions.
    return {path: row for path, row in rows.items() if path in tracked}, problems


def vale_configuration(rows):
    """Derive per-page scopes without keeping a second classification list."""
    config = (VALE_DIR / "vale.ini").read_text(encoding="utf-8")
    config = config.replace("StylesPath = styles", f"StylesPath = {VALE_DIR / 'styles'}", 1)
    for path, row in sorted(rows.items()):
        if path == "docs/AGENTS.md" or path.startswith("docs/testing/"):
            continue
        if row["audience"] not in {"user", "both"}:
            continue
        config += f"\n[{path}]\nLibsixel.ContributorPaths = YES\nLibsixel.CommitReferences = YES\n"
        if row["type"] == "reference":
            config += "Libsixel.ChangeLogPhrasing = YES\n"
    return config


def summarize(findings):
    totals = Counter(alert["Check"] for alerts in findings.values() for alert in alerts)
    print(f"Vale: {sum(totals.values())} findings in {sum(bool(a) for a in findings.values())} documents (advisory)")
    print("\nPer rule:")
    for rule in RULES:
        print(f"  {totals[rule]:6d}  {rule}")
    print("\nPer document (count descending; ties by path):")
    for path, alerts in sorted(findings.items(), key=lambda item: (-len(item[1]), item[0])):
        if alerts:
            counts = Counter(alert["Check"].removeprefix("Libsixel.") for alert in alerts)
            detail = ", ".join(f"{rule}={count}" for rule, count in sorted(counts.items()))
            print(f"  {len(alerts):6d}  {path}  ({detail})")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--inventory-only", action="store_true", help="check tracked-document coverage without Vale")
    parser.add_argument("--json", type=Path, help="write Vale's full findings JSON to this caller-selected path")
    args = parser.parse_args()
    rows, _problems = read_inventory()
    if args.inventory_only:
        return
    vale = shutil.which("vale")
    if vale is None:
        print("Vale: skipped (vale is not on PATH); inventory check completed. All checks are advisory.")
        return
    # A private transient config keeps the checked-in config and inventory
    # independent of the caller's working directory or global Vale styles.
    with tempfile.TemporaryDirectory(prefix="libsixel-prose-") as temporary:
        config = Path(temporary) / "vale.ini"
        config.write_text(vale_configuration(rows), encoding="utf-8")
        result = subprocess.run(
            [vale, "--no-global", "--no-exit", f"--config={config}", "--output=JSON", "--glob=*.md", "docs/"],
            cwd=ROOT, capture_output=True, text=True,
        )
    if result.stderr:
        print(result.stderr.rstrip(), file=sys.stderr)
    if result.returncode:
        print(f"Vale: could not complete (exit {result.returncode}); this is not a clean lint result.")
        print(result.stdout.rstrip())
        return
    findings = json.loads(result.stdout)
    if not isinstance(findings, dict):
        raise ValueError("Vale did not return a findings object")
    summarize(findings)
    if args.json:
        args.json.write_text(json.dumps(findings, indent=2, sort_keys=True) + "\n", encoding="utf-8")
        print(f"\nFindings JSON: {args.json}")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, csv.Error, subprocess.SubprocessError) as error:
        print(f"lint-prose: advisory check could not complete: {error}", file=sys.stderr)
