#!/usr/bin/env python3
"""Reject literal find command dependencies in maintained build/test shells."""

import ast
from pathlib import Path
import re
import shlex
import sys

from source_files import files
from test_pattern_match import statements


WRAPPERS = frozenset(('exec', 'command', 'env', 'xargs', 'sudo'))
START = frozenset((';', ';;', '|', '||', '&', '&&', '(', ')', '{',
                   'if', 'elif', 'then', 'else', 'do', 'while', 'until', '!'))


def contains_find(source):
    """Inspect command positions and literal nested shell programs only."""
    # Make's $(shell ...) is not a POSIX command substitution. Normalize
    # its literal introducer before applying the shell command lexer.
    source = re.sub(r'\$\(shell\s+', '$(', source)
    for _, _, _, tokens in statements(source):
        command = True
        shell = False
        shell_code = False
        for token in tokens:
            raw = token.text
            if not raw.startswith("'"):
                for nested in re.findall(r'\$\(([^()]*)\)|`([^`]*)`', raw):
                    code = next(part for part in nested if part)
                    code = re.sub(r'^shell\s+', '', code)
                    if contains_find(code):
                        return True
            try:
                value = shlex.split(raw)
            except ValueError:
                continue
            text = value[0] if len(value) == 1 else raw
            if shell_code:
                if contains_find(text):
                    return True
                shell_code = False
            if shell and text == '-c':
                shell_code = True
            if text in START:
                command = True
                shell = False
                continue
            if not command:
                continue
            text = text.lstrip('@+-')
            if not text or re.match(r'^[A-Za-z_][A-Za-z0-9_]*=', text):
                continue
            if text in WRAPPERS or text.startswith('-'):
                continue
            if Path(text).name in ('find', 'find.exe', 'gfind'):
                return True
            shell = text in ('sh', 'bash', 'dash', 'ksh')
            command = False
    return False


def violations(root):
    errors = []
    # Exclude upstream Libtool and generated configure/Makefile inputs. Their
    # portability implementation is not maintained by this project.
    for filename in files([str(root)]):
        path = Path(filename)
        rel = path.relative_to(root).as_posix()
        makefile = path.name == 'Makefile.am'
        test_shell = rel.startswith('tests/') and path.suffix in ('.sh', '.t')
        helper = rel.startswith('build-aux/') and (path.name.endswith('.sh') or
                                                  path.name.endswith('.sh.in'))
        generator = rel in ('tools/gen-amalgamation.sh',
                            'tools/update-mime-database.sh')
        if rel == 'build-aux/ltmain.sh':
            continue
        if makefile or test_shell or helper or generator:
            source = path.read_text(errors='replace')
            try:
                if contains_find(source):
                    errors.append(rel)
            except ValueError as error:
                errors.append(f'{rel}: shell scan failed: {error}')
        elif path.name == 'meson.build':
            # Meson shell fragments are literal quoted strings, including
            # fragments joined with + before run_command evaluates them.
            literals = re.finditer(
                r"#[^\n]*|'(?:\\.|[^'\\])*'|\"(?:\\.|[^\"\\])*\"",
                path.read_text())
            for match in literals:
                literal = match[0]
                if literal.startswith('#'):
                    continue
                fragment = literal[1:-1].replace("\\'", "'")
                try:
                    forbidden = contains_find(fragment)
                except ValueError:
                    # A fragment can end inside a shell quote completed by
                    # another Meson operand. Check its command introducer.
                    forbidden = bool(re.search(
                        r'(?:^|[;|&])\s*(?:exec\s+)?'
                        r'(?:/[^ ]*/)?(?:find|gfind|find.exe)\b', fragment))
                if forbidden:
                    errors.append(rel)
                    break
        elif rel.startswith('tests/_static/python/') and path.suffix == '.py':
            tree = ast.parse(path.read_text(), filename=rel)
            for node in ast.walk(tree):
                if not isinstance(node, ast.Call) or not node.args:
                    continue
                function = node.func
                name = function.attr if isinstance(function, ast.Attribute) else ''
                if name not in ('run', 'Popen', 'call', 'check_call',
                                'check_output', 'system'):
                    continue
                argument = node.args[0]
                if isinstance(argument, (ast.List, ast.Tuple)) and argument.elts:
                    argument = argument.elts[0]
                if isinstance(argument, ast.Constant) and isinstance(argument.value, str):
                    if contains_find(argument.value):
                        errors.append(rel)
    return sorted(set(errors))


if __name__ == '__main__':
    errors = violations(Path(sys.argv[1]).resolve())
    for error in errors:
        print(f'{error}: findutils command is forbidden in build/test code',
              file=sys.stderr)
    sys.exit(bool(errors))
