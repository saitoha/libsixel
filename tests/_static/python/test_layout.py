#!/usr/bin/env python3
"""Validate source case ownership, two category levels, and bounded numbering."""

from collections import Counter, defaultdict
import os
from pathlib import Path
import re
import sys


ENTRY = re.compile(r"([0-9]{4})_([a-zA-Z0-9][a-zA-Z0-9_-]*)\.(t|sh|py|rb|pl|php|ps1)$")
RESERVED = frozenset(("_static", "_support", "_artifacts", "data"))
MAX_CASES = 200
MAX_CHILDREN = 24


def inspect(tests):
    """Inspect the physical tree, including untracked source cases."""
    cases = defaultdict(list)
    errors = []
    for directory, children, names in os.walk(tests, followlinks=False):
        relative = Path(directory).relative_to(tests)
        if relative == Path('.'):
            children[:] = [n for n in children if n not in RESERVED and
                           not n.startswith('.')]
        else:
            children[:] = [n for n in children if not n.startswith('.')]
        for name in names:
            match = ENTRY.fullmatch(name)
            # A shell case cannot escape validation by losing its prefix.
            if not match and not name.endswith('.t'):
                continue
            path = relative / name
            if not match:
                errors.append(f'{path}: expected NNNN_observation.ext')
                continue
            if len(relative.parts) != 2:
                errors.append(f'{path}: expected two category levels')
                continue
            if (Path(directory) / name).is_symlink():
                errors.append(f'{path}: case must be a regular source file')
            cases[relative.as_posix()].append((int(match[1]), name))
        for name in children:
            if (Path(directory) / name).is_symlink():
                errors.append(f'{relative / name}: category cannot be a symlink')
    domains = defaultdict(set)
    for leaf, entries in sorted(cases.items()):
        domain, behavior = leaf.split('/')
        domains[domain].add(behavior)
        if len(entries) > MAX_CASES:
            errors.append(f'{leaf}: {len(entries)} cases exceed {MAX_CASES}')
        numbers = sorted(number for number, _ in entries)
        if numbers != list(range(1, len(entries) + 1)):
            errors.append(f'{leaf}: case numbers must be unique and contiguous from 0001')
    for parent, children in [('tests', set(domains)), *sorted(domains.items())]:
        if len(children) > MAX_CHILDREN:
            errors.append(f'{parent}: {len(children)} categories exceed {MAX_CHILDREN}')
    return cases, domains, errors


def main():
    root = Path(sys.argv[1])
    if not root.is_dir():
        raise ValueError(f'missing tests directory: {root}')
    cases, domains, errors = inspect(root)
    for error in errors:
        print(error, file=sys.stderr)
    if '--counts' in sys.argv[2:]:
        print('category\tcases')
        for category, entries in sorted(cases.items()):
            print(f'{category}\t{len(entries)}')
        print(f'TOTAL\t{sum(map(len, cases.values()))}')
        print('parent\tchild_categories')
        print(f'tests\t{len(domains)}')
        for domain, children in sorted(domains.items()):
            print(f'{domain}\t{len(children)}')
    return int(bool(errors))


if __name__ == '__main__':
    sys.exit(main())
