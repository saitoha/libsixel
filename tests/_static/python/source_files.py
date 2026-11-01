#!/usr/bin/env python3
"""Enumerate maintained source trees for static checks, without findutils."""

import argparse
import fnmatch
import os
import subprocess
import sys


PRUNED = frozenset((".git", "_artifacts", ".perl-test-venv",
                    ".php-test-venv", ".python-test-venv", ".ruby-test-venv"))


def excluded_path(path, patterns):
    """Meson uses slash patterns even when scandir returns Windows paths."""
    normalized = path.replace('\\', '/')
    return any(fnmatch.fnmatchcase(normalized, pattern.replace('\\', '/'))
               for pattern in patterns)


def files(roots, names=(), excludes=(), excluded_paths=(), max_depth=None):
    """Do not follow symlinks or silently accept missing/unreadable roots."""
    def walk(directory, depth):
        with os.scandir(directory) as entries:
            for entry in entries:
                path = entry.path
                if excluded_path(path, excluded_paths):
                    continue
                if entry.is_dir(follow_symlinks=False):
                    if not entry.name.startswith(".") and entry.name not in PRUNED and (max_depth is None or
                                                   depth < max_depth):
                        yield from walk(path, depth + 1)
                elif entry.is_file(follow_symlinks=False):
                    if names and not any(fnmatch.fnmatchcase(entry.name, n)
                                         for n in names):
                        continue
                    if any(fnmatch.fnmatchcase(entry.name, n) for n in excludes):
                        continue
                    yield path
    for root in roots:
        yield from walk(root, 1)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("roots", nargs="+")
    parser.add_argument("--name", action="append", default=[])
    parser.add_argument("--exclude-name", action="append", default=[])
    parser.add_argument("--exclude-path", action="append", default=[])
    parser.add_argument("--max-depth", type=int)
    parser.add_argument("--null", action="store_true")
    parser.add_argument("--exec", dest="command", nargs=argparse.REMAINDER)
    args = parser.parse_args()
    try:
        paths = files(args.roots, args.name, args.exclude_name,
                      args.exclude_path, args.max_depth)
        if args.command:
            # Bound both bytes and number of paths. No empty command runs,
            # and a failed batch remains a failure even if later ones pass.
            budget = max(1024, (24000 if os.name == "nt" else 120000) - sum(len(os.fsencode(x)) + 1
                                          for x in args.command))
            batch = []
            size = 0
            failed = False
            for path in paths:
                length = len(os.fsencode(path)) + 1
                if batch and (size + length > budget or len(batch) == 1024):
                    failed |= subprocess.run(args.command + batch).returncode != 0
                    batch = []
                    size = 0
                batch.append(path)
                size += length
            if batch:
                failed |= subprocess.run(args.command + batch).returncode != 0
            return int(failed)
        separator = "\0" if args.null else "\n"
        for path in paths:
            sys.stdout.write(path + separator)
        return 0
    except (OSError, ValueError) as error:
        print(f"source inventory: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
