#!/usr/bin/env python3
"""Check explicit inventory scopes, document links and source backlinks."""

from collections import Counter
import os
from pathlib import Path
import re
import subprocess
import sys
import tempfile

from source_files import files


GLOB = re.compile(r'tests/[A-Za-z0-9_./-]+/(?:\?\?\?\?_)?\*\.[A-Za-z0-9]+')
MANIFEST = re.compile(r'@tests/_support/test-plans/[A-Za-z0-9_-]+\.list')
TEST_PATH = re.compile(r'tests/[A-Za-z0-9_./-]+')
LINK = re.compile(r'\[(tests/[A-Za-z0-9_./-]+)\]\(([^()\s]+)\)')


def inventory_errors(root, references):
    """Compare independently declared scopes, links and source references."""
    expected = set()
    linked = Counter()
    errors = []
    scopes = set()
    for filename in files([str(root / 'docs')], ['*.md']):
        path = Path(filename)
        doc = path.relative_to(root).as_posix()
        active = False
        for line in path.read_text().splitlines():
            if line.startswith('<!-- test-plan:'):
                if active:
                    errors.append(f'{doc}: nested test-plan marker')
                match = re.fullmatch(r'<!-- test-plan: (.+) -->', line)
                spec = match[1] if match else ''
                if (doc, spec) in scopes:
                    errors.append(f'{doc}: duplicate test-plan scope: {spec}')
                scopes.add((doc, spec))
                members = []
                if GLOB.fullmatch(spec):
                    members = [p.relative_to(root).as_posix()
                               for p in root.glob(spec) if p.is_file()]
                elif MANIFEST.fullmatch(spec):
                    manifest = root / spec[1:]
                    try:
                        members = manifest.read_text().splitlines()
                    except OSError as error:
                        errors.append(f'{doc}: {error}')
                    for member, count in Counter(members).items():
                        if not TEST_PATH.fullmatch(member) or '..' in member.split('/'):
                            errors.append(f'{spec}: invalid test path: {member}')
                        if count != 1:
                            errors.append(f'{spec}: duplicate test path: {member}')
                else:
                    errors.append(f'{doc}: unsupported test-plan marker: {line}')
                if not members:
                    errors.append(f'{doc}: empty test-plan scope: {spec}')
                for member in members:
                    if not (root / member).is_file():
                        errors.append(f'{doc}: missing scoped test: {member}')
                    expected.add((doc, member))
                active = True
            elif line == '<!-- test-plan-end -->':
                if not active:
                    errors.append(f'{doc}: unmatched test-plan-end')
                active = False
            elif active:
                for match in LINK.finditer(line):
                    test, target = match.groups()
                    linked[doc, test] += 1
                    target_path = path.parent / target
                    if not target_path.is_file() or target_path.resolve() != (root / test).resolve():
                        errors.append(f'{doc}: incorrect link target for {test}: {target}')
        if active:
            errors.append(f'{doc}: missing test-plan-end')
    if not scopes:
        errors.append('docs: no test-plan marker found')
    for pair, count in linked.items():
        if count != 1:
            errors.append(f'{pair[0]}: duplicate inventory link: {pair[1]}')
    errors.extend((references / 'test-plan-errors').read_text().splitlines())
    backlinks = Counter(tuple(line.split('|', 1)) for line in
                        (references / 'test-plan-links').read_text().splitlines())
    for pair, count in backlinks.items():
        if count != 1:
            errors.append(f'{pair[1]}: duplicate Test-plan reference: {pair[0]}')
    for label, actual in [('inventory link', set(linked)),
                          ('Test-plan reference', set(backlinks))]:
        for doc, test in sorted(expected - actual):
            errors.append(f'{doc}: missing {label}: {test}')
        for doc, test in sorted(actual - expected):
            errors.append(f'{doc}: out-of-scope {label}: {test}')
    return errors


def check(root, references):
    errors = inventory_errors(root, references)
    generated = subprocess.run([sys.executable,
        str(root / 'tools/generate_builtin_loader_coverage.py'), '--check'],
        cwd=root, capture_output=True, text=True)
    if generated.returncode:
        errors.append(generated.stdout + generated.stderr)
    for error in errors:
        print(error, file=sys.stderr)
    return int(bool(errors))


def main():
    root = Path(sys.argv[1]).resolve()
    shared = os.environ.get('SIXEL_STATICCHECK_REFERENCE_DIR')
    if shared:
        return check(root, Path(shared))
    with tempfile.TemporaryDirectory(prefix='libsixel-test-plan-') as temporary:
        subprocess.run(['sh', str(root / 'tests/_static/sh/collect-source-references.sh'),
                        str(root), temporary, 'test-plan'], check=True)
        return check(root, Path(temporary))


if __name__ == '__main__':
    sys.exit(main())
