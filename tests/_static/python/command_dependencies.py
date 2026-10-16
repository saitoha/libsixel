#!/usr/bin/env python3
"""Compare literal shell/build command dependencies with configure probes.

This is a source audit, not a shell interpreter. Dynamic program selection
and the control flow of optional-tool fallbacks remain outside its proof.
"""

from collections import defaultdict
from pathlib import Path
import re
import shlex
import subprocess
import sys

try:
    from tree_sitter import Language, Parser
    import tree_sitter_bash
except ImportError:
    print('install tests/_static/python/command-dependencies-requirements.txt',
          file=sys.stderr)
    sys.exit(77)

from test_pattern_match import statements


LANGUAGE = Language(tree_sitter_bash.language())
BUILTINS = set('''
    : . [ alias bg break cd command continue declare echo eval exec exit
    export false fc fg getopts hash history jobs kill let local popd printf
    pushd pwd read readonly return set shift shopt source test times trap
    true type typeset ulimit umask unalias unset wait
'''.split())
KEYWORDS = set('if then else elif fi do done case esac for while until in'.split())
LITERAL = re.compile(r'[a-zA-Z_][a-zA-Z_0-9.+-]*\Z')


def literal_name(text):
    text = text.strip('\"\'')
    fallback = re.fullmatch(r'\$\{\w+:-([a-zA-Z_0-9.+-]+)\}', text)
    if fallback:
        text = fallback[1]
    return text if LITERAL.fullmatch(text) else None


def wrapped_command(name, arguments):
    """Unwrap common launch/probe commands without evaluating shell words."""
    words = list(arguments)
    while name in ('command', 'exec', 'env', 'sudo', 'xargs', 'nice', 'timeout'):
        takes_value = {
            'env': {'-u', '--unset', '-C', '--chdir'},
            'sudo': {'-u', '-g', '-h'},
            'xargs': {'-n', '-P', '-I', '-s', '-L'},
            'nice': {'-n'},
            'timeout': {'-s', '-k', '--signal', '--kill-after'},
        }.get(name, set())
        duration = name == 'timeout'
        while words:
            word = words.pop(0)
            if word in takes_value:
                if words:
                    words.pop(0)
                continue
            if word.startswith('-') or re.match(r'\w+=', word):
                continue
            if duration:
                duration = False
                continue
            name = literal_name(word)
            break
        else:
            return None
    return name


def shell_commands(source):
    """Read command-name nodes, excluding functions and inert heredoc data."""
    data = source.encode()
    tree = Parser(LANGUAGE).parse(data)
    stack = [tree.root_node]
    functions = set()
    commands = []
    ranges = []
    # The shared lexer understands heredoc bodies following an || guard,
    # where the Bash grammar can recover a data word as a command node.
    for offset, raw, _, _ in statements(source):
        begin = len(source[:offset].encode())
        ranges.append((begin, begin + len(raw.encode())))
    while stack:
        node = stack.pop()
        stack.extend(reversed(node.children))
        if node.type == 'function_definition':
            name = node.child_by_field_name('name')
            if name:
                functions.add(name.text.decode())
        if node.type != 'command':
            continue
        name = node.child_by_field_name('name')
        if name is None:
            continue
        # A grammar recovery can split $dir/$name.ext into an assignment
        # followed by a command. A real command needs a token boundary.
        if name.start_byte and chr(data[name.start_byte - 1]) not in ' \t\r\n;|&({':
            continue
        if not any(begin <= name.start_byte < end for begin, end in ranges):
            continue
        text = literal_name(name.text.decode())
        arguments = [child.text.decode()
                     for child in node.children_by_field_name('argument')]
        if text and text not in BUILTINS | KEYWORDS:
            commands.append((text, name.start_point.row + 1))
        wrapped = wrapped_command(text, arguments)
        if wrapped and wrapped != text and wrapped not in BUILTINS | KEYWORDS:
            commands.append((wrapped, name.start_point.row + 1))
        if text in ('sh', 'bash', 'dash', 'ksh') and '-c' in arguments:
            index = arguments.index('-c') + 1
            if index < len(arguments) and arguments[index].startswith("'"):
                code = shlex.split(arguments[index])
                if len(code) == 1:
                    nested_functions, nested = shell_commands(code[0])
                    functions.update(nested_functions)
                    commands.extend((tool, name.start_point.row + line)
                                    for tool, line in nested)
        if text == 'find':
            for index, word in enumerate(arguments[:-1]):
                if word in ('-exec', '-execdir', '-ok', '-okdir'):
                    tool = literal_name(arguments[index + 1])
                    if tool and tool not in BUILTINS:
                        commands.append((tool, name.start_point.row + 1))
    return functions, commands


def make_recipes(source):
    """Keep recipe statements, not continued source lists or prerequisites."""
    result = []
    pending = ''
    for line in source.splitlines(keepends=True):
        pending += line
        if line.rstrip('\r\n').endswith('\\'):
            continue
        count = pending.count('\n')
        if pending.startswith('\t'):
            recipe = re.sub(r'\\\r?\n', ' ', pending[1:])
            # Make variables are data to this parser, not shell substitutions.
            # Protect escaped dollars first: $$(tool) is a real shell call.
            recipe = recipe.replace('$$', '\0')
            recipe = re.sub(r'\$\([^)]*\)', '${MAKE_VARIABLE}', recipe)
            recipe = recipe.replace('\0', '$').lstrip('@-+')
            result.append(recipe.rstrip('\n') + '\n' * max(1, count))
        else:
            result.append('\n' * max(1, count))
        pending = ''
    return ''.join(result)


def configure_commands(source):
    commands = set()
    # Comments cannot establish a detection contract.
    source = re.sub(r'(?m)^\s*(?:#|dnl).*$', '', source)
    for match in re.finditer(
            r'AC_(?:PATH|CHECK)_(?:PROGS?|TOOLS?)\(\s*\[[^]]+\]\s*,\s*\[([^]]+)\]',
            source):
        commands.update(word for word in match[1].split() if LITERAL.fullmatch(word))
    # These Autoconf/Automake/Libtool entry points own their tool probes.
    implied = {
        'AC_PROG_AWK': 'awk gawk mawk nawk',
        'AC_PROG_CC': 'cc gcc clang',
        'AC_PROG_CPP': 'cpp',
        'AC_PROG_INSTALL': 'install',
        'AC_PROG_LN_S': 'ln',
        'AC_PROG_MAKE_SET': 'make gmake',
        'LT_INIT': 'ar as ld nm ranlib strip',
        'PKG_PROG_PKG_CONFIG': 'pkg-config',
    }
    for macro, tools in implied.items():
        if re.search(r'\b' + macro + r'\b', source):
            commands.update(tools.split())
    return commands


def inventory(root):
    # Include new task-owned sources as well as tracked sources; exclude
    # build outputs, generated upstream tooling, and non-shell Perl .t files.
    paths = subprocess.check_output(
        ['git', '-C', str(root), 'ls-files', '-z', '--cached', '--others',
         '--exclude-standard']).decode().split('\0')
    owners = defaultdict(set)
    products = set()
    sources = {}
    uses = []
    for relative in sorted(set(paths)):
        path = root / relative
        if not relative or not path.is_file() or relative == 'build-aux/ltmain.sh':
            continue
        is_make = (path.suffix in ('.am', '.mk') or
                   path.name in ('Makefile', 'makefile', 'GNUmakefile'))
        if not (is_make or path.suffix in ('.sh', '.t') or
                path.name.endswith('.sh.in') or path.name == 'meson.build'):
            # Maintained launchers can omit .sh. Do not inspect generated
            # configure or interpret arbitrary text/binary files as shell.
            if path.suffix not in ('', '.in') or path.name == 'configure':
                continue
            with path.open('rb') as stream:
                first = stream.read(160)
            if not re.match(rb'#![^\n]*\b(?:sh|bash|dash|ksh)\b', first):
                continue
        source = path.read_text(errors='replace')
        sources[relative] = source
        if path.suffix == '.t' and not re.match(r'#![^\n]*sh', source):
            continue
        if path.name == 'meson.build':
            # A Meson discovery also needs an Autotools counterpart. Literal
            # run_command/custom-target executables are dependencies too.
            pattern = r'(?:(?:find_program|run_command)\(|command\s*:\s*\[)\s*[\'\"]([a-zA-Z_][a-zA-Z_0-9.+-]*)[\'\"]'
            commands = [(m[1], source[:m.start()].count('\n') + 1)
                        for m in re.finditer(pattern, source)]
        else:
            if is_make:
                logical = re.sub(r'\\\r?\n', ' ', source)
                for match in re.finditer(
                        r'(?m)^\w+_(?:PROGRAMS|SCRIPTS)\s*\+?=([^\n]*)', logical):
                    products.update(word for word in match[1].split()
                                    if LITERAL.fullmatch(word))
                source = make_recipes(source)
            defined, commands = shell_commands(source)
            for name in defined:
                owners[name].add(relative)
        for command, line in commands:
            uses.append((command, relative, line))
    calls = defaultdict(list)
    for name, relative, line in uses:
        if name in products or relative in owners[name]:
            continue
        # Shared functions need a source edge as well as a matching name.
        # A function in an unrelated script must not hide an external call.
        source = sources[relative]
        sourced = re.search(r'(?m)^\s*(?:\.|source)\s', source)
        if sourced and any(Path(owner).name in source for owner in owners[name]):
            continue
        calls[name].append(f'{relative}:{line}')
    return calls


def selftest():
    sample = '''# unseen_tool in a comment is not a dependency
helper() { printf '%s' ok; }
helper
value=$(cksum input)
printf '%s' data | "${CKSUM:-cksum}"
cat <<'DATA'
unseen_tool input
DATA
unseen_tool input
env FLAG=1 unseen_tool input
command -v unseen_tool
'''
    functions, commands = shell_commands(sample)
    names = [name for name, _ in commands if name not in functions]
    if names != ['cksum', 'cksum', 'cat', 'unseen_tool', 'env',
                 'unseen_tool', 'unseen_tool']:
        raise ValueError(f'command parser control failed: {names}')
    declared = configure_commands('AC_PATH_PROGS([CKSUM], [cksum cksum.exe], [])')
    if not {'cksum', 'cksum.exe'} <= declared or 'unseen_tool' in declared:
        raise ValueError('configure probe control failed')
    if configure_commands('# AC_PATH_PROG([CKSUM], [cksum])'):
        raise ValueError('a commented-out probe was accepted')
    recipes = make_recipes('SOURCES = \\\n\tdata.c\nall: \\\n\tinput\n\tcksum input\n')
    if shell_commands(recipes)[1] != [('cksum', 5)]:
        raise ValueError('Makefile recipe control failed')
    recipes = make_recipes('all:\n\tvalue=$$(cksum input); $(CC) input.c\n')
    if shell_commands(recipes)[1] != [('cksum', 2)]:
        raise ValueError('Makefile shell substitution control failed')


def main():
    root = Path(sys.argv[1]).resolve()
    selftest()
    source = (root / 'configure.ac').read_text()
    source += '\n'.join(p.read_text() for p in (root / 'm4').glob('*.m4'))
    detected = configure_commands(source)
    calls = inventory(root)
    missing = sorted(set(calls) - detected)
    for name in missing:
        print(f'{calls[name][0]}: {name}: no configure tool probe', file=sys.stderr)
    print(f'# command dependencies: {len(calls)} named tools, {len(missing)} missing probes')
    return bool(missing)


if __name__ == '__main__':
    try:
        sys.exit(main())
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(error, file=sys.stderr)
        sys.exit(1)
