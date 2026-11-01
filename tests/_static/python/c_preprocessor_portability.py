#!/usr/bin/env python3
"""Check platform macro collisions without preprocessing or compiling C."""

from pathlib import Path
import re
import sys

from source_files import files


LITERALS = re.compile(r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'', re.S)


def blank(match):
    return re.sub(r'[^\n]', ' ', match[0])


def violations(source, local_providers=()):
    # Keep line positions and include operands, but ignore comments and strings
    # when inspecting C identifiers. All conditional branches are inspected.
    code = LITERALS.sub(blank, source)
    errors = []
    for match in re.finditer(r'\b(?:near|far)\b', code):
        errors.append((code.count('\n', 0, match.start()) + 1,
                       'identifier collides with Windows macro: ' + match[0]))
    # stdint is also reached through the public header. limits.h alone is
    # insufficient: a later stdint include can redefine an early fallback.
    comments = LITERALS.sub(lambda m: blank(m) if m[0].startswith('/') else m[0], source)
    provider = False
    for number, line in enumerate(comments.splitlines(), 1):
        if re.match(r'\s*#\s*include\s*[<"](?:stdint|inttypes|sixel)\.h[>"]', line):
            provider = True
        included = re.match(r'\s*#\s*include\s*"([^"]+)"', line)
        if included and included[1] in local_providers:
            provider = True
        if re.match(r'\s*#\s*define\s+SIZE_MAX\b', line) and not provider:
            errors.append((number, 'SIZE_MAX fallback precedes integer header'))
    return errors


def controls():
    good = ['double nearest_distance, farthest_distance;',
            '/* near */ char *s = "far"; // near\n',
            '#include <stdint.h>\n#ifndef SIZE_MAX\n#define SIZE_MAX 1\n#endif',
            '#include "sixel.h"\n#define SIZE_MAX 1']
    bad = ['double near;', 'double far, other;',
           '#include <limits.h>\n#define SIZE_MAX 1\n#include <sixel.h>',
           '/* #include <stdint.h> */\n#define SIZE_MAX 1']
    if violations('#include "local.h"\n#define SIZE_MAX 1', {'local.h'}):
        raise RuntimeError('quoted provider control failed')
    if not violations('#define SIZE_MAX 1\n#include "local.h"', {'local.h'}):
        raise RuntimeError('late quoted provider escaped ordering check')
    if any(violations(s) for s in good) or any(not violations(s) for s in bad):
        raise RuntimeError('preprocessor portability scanner controls failed')


def providers(root, path, seen=None):
    """Follow maintained quoted headers, never platform-dependent system trees."""
    seen = set() if seen is None else seen
    if path in seen:
        return False
    seen.add(path)
    source = LITERALS.sub(lambda m: blank(m) if m[0].startswith('/') else m[0], path.read_text(encoding='utf-8'))
    if re.search(r'^\s*#\s*include\s*[<"](?:stdint|inttypes|sixel)\.h[>"]', source, re.M):
        return True
    for name in re.findall(r'^\s*#\s*include\s*"([^"]+)"', source, re.M):
        for candidate in (path.parent / name, root / name, root / 'src' / name):
            if candidate.is_file() and providers(root, candidate, seen):
                return True
    return False


def main(root):
    controls()
    failed = False
    # Maintained C, including test helpers, is compiled on Windows. Vendor
    # headers are excluded because their macros are not project identifiers.
    for name in files([root / d for d in ('src', 'include', 'converters',
                                         'assessment', 'tests')],
                      names=('*.c', '*.h', '*.h.in')):
        path = Path(name)
        if 'third_party' in path.parts or 'data' in path.parts:
            continue
        source = path.read_text(encoding='utf-8')
        local = set()
        if re.search(r'#\s*define\s+SIZE_MAX\b', source):
            for name in re.findall(r'^\s*#\s*include\s*"([^"]+)"', source, re.M):
                for candidate in (path.parent / name, root / name, root / 'src' / name):
                    if candidate.is_file() and providers(root, candidate):
                        local.add(name)
        for line, message in violations(source, local):
            print(f'{path.relative_to(root)}:{line}: {message}')
            failed = True
    return int(failed)


if __name__ == '__main__':
    try:
        sys.exit(main(Path(sys.argv[1]).resolve()))
    except (OSError, RuntimeError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
