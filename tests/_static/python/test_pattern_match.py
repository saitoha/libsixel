#!/usr/bin/env python3
"""Check shell TAP pattern predicates without executing test source."""

import hashlib
import json
import re
import shlex
import sys
from dataclasses import dataclass
from pathlib import Path


@dataclass
class Word:
    text: str
    start: int
    end: int


def expansion_end(text, start):
    """Parameter words have their own quotes, independent of outer quotes."""
    i = start + 2
    quote = ''
    while i < len(text):
        c = text[i]
        if c == '\\' and quote != "'":
            i += 2
            continue
        if quote == "'":
            if c == quote:
                quote = ''
        elif text.startswith('${', i):
            i = expansion_end(text, i)
            continue
        elif c in "\"'":
            if not quote:
                quote = c
            elif c == quote:
                quote = ''
        elif c == '}' and not quote:
            return i + 1
        i += 1
    raise ValueError('unterminated parameter expansion')


def words(text):
    """Retain spelling and offsets; quoted operators stay inside a word."""
    result = []
    i = 0
    while i < len(text):
        if text[i].isspace():
            i += 1
            continue
        if text[i] == '#':
            break
        start = i
        if text[i] in ';|&()<>' and not text.startswith('$(', i):
            i += 1
            if i < len(text) and text[i] == text[start]:
                i += 1
        else:
            quote = ''
            while i < len(text):
                c = text[i]
                if c == '\\' and quote != "'":
                    i += 2
                    continue
                if quote == "'":
                    if c == quote:
                        quote = ''
                elif text.startswith('${', i):
                    i = expansion_end(text, i)
                    continue
                elif c in "\"'":
                    if not quote:
                        quote = c
                    elif c == quote:
                        quote = ''
                elif not quote and (c.isspace() or c in ';|&()<>'):
                    break
                i += 1
            if quote:
                raise ValueError('unterminated shell quote')
        result.append(Word(text[start:i], start, i))
    return result


def statements(source):
    """Join continuations and exclude heredoc payloads from shell syntax."""
    pending = ''
    offset = 0
    start = 0
    heredocs = []
    for line in source.splitlines(keepends=True):
        if heredocs:
            delimiter, tabs = heredocs[0]
            candidate = line.rstrip('\r\n')
            if tabs:
                candidate = candidate.lstrip('\t')
            if candidate == delimiter:
                heredocs.pop(0)
            offset += len(line)
            continue
        if not pending:
            start = offset
        pending += line
        offset += len(line)
        if re.search(r'\\\r?\n$', line):
            continue
        logical = re.sub(r'\\\r?\n', ' ', pending)
        # Heredoc introducers are operators, not strings printed by a test.
        try:
            tokens = words(logical)
        except ValueError:
            continue
        for i, token in enumerate(tokens[:-1]):
            if token.text == '<<':
                raw = tokens[i + 1].text
                tabs = raw.startswith('-')
                raw = raw[1:] if tabs else raw
                if tabs and not raw and i + 2 < len(tokens):
                    raw = tokens[i + 2].text
                # Delimiters undergo quote removal, not variable expansion.
                delimiter = shlex.split(raw)
                if len(delimiter) == 1:
                    heredocs.append((delimiter[0], tabs))
        yield start, pending, logical, tokens
        pending = ''
    if pending:
        yield start, pending, pending, words(pending)


def parameter(word):
    """Return an exact variable operand or one prefix/suffix removal."""
    if word.startswith('"') and word.endswith('"'):
        word = word[1:-1]
    match = re.fullmatch(r'\$([A-Za-z_][A-Za-z0-9_]*)', word)
    if match:
        return match[1], '', ''
    match = re.match(r'\$\{([A-Za-z_][A-Za-z0-9_]*)', word)
    if not match or expansion_end(word, 0) != len(word):
        return None
    rest = word[match.end():-1]
    if not rest:
        return match[1], '', ''
    removal = re.match(r'(##|%%|#|%)(.*)\Z', rest, re.S)
    if removal:
        return match[1], removal[1], removal[2]
    return None


def shell_pattern(word):
    """Recognize unquoted glob syntax or a dynamically supplied pattern."""
    quote = ''
    i = 0
    while i < len(word):
        c = word[i]
        if c == '\\' and quote != "'":
            i += 2
            continue
        if quote:
            if c == quote:
                quote = ''
        elif c in "\"'":
            quote = c
        elif c in '*?[$':
            return True
        i += 1
    return False


def redundant_prefix_scans(source):
    """Reject the syntactically redundant wildcard in ${value#*marker*}.

    Shortest-prefix removal repeatedly tests prefix lengths in some shells.
    A trailing wildcard is redundant here and usually signals a trace-tail
    predicate. This narrow check does not ban useful field/path extraction
    or pretend to infer input lengths and arbitrary shell data flow.
    """
    findings = []
    for offset, raw, logical, tokens in statements(source):
        for token in tokens:
            value = token.text
            assignment = re.fullmatch(r'[A-Za-z_][A-Za-z0-9_]*=(.*)',
                                      value, re.S)
            if assignment:
                value = assignment[1]
            parsed = parameter(value)
            if not parsed or parsed[1] != '#':
                continue
            pattern = parsed[2]
            if len(pattern) < 3 or not pattern.startswith('*'):
                continue
            # Only an unquoted, unescaped final star is a wildcard.
            quote = ''
            last_star = -1
            i = 0
            while i < len(pattern):
                c = pattern[i]
                if c == '\\' and quote != "'":
                    i += 2
                    continue
                if quote:
                    if c == quote:
                        quote = ''
                elif c in "\"'":
                    quote = c
                elif c == '*':
                    last_star = i
                i += 1
            if last_star != len(pattern) - 1:
                continue
            findings.append({
                'line': source.count('\n', 0, offset) + 1,
                'statement': raw.rstrip('\r\n'),
                'logical': logical,
                'start': token.start,
                'end': token.end,
                'reason': 'avoid ${value#*marker*}; use ordered case patterns '
                          'for trace order/count checks',
            })
            break
    return findings


def predicates(source):
    """Find removal-vs-original tests, including assigned/copy operands.

    Field/path extraction is allowed: it produces a value. Comparing that
    value with its unmodified source to discover a pattern match is not.
    This is a syntactic policy checker, not a general shell interpreter.
    """
    derived = {}
    aliases = {}
    findings = []
    for offset, raw, logical, tokens in statements(source):
        for i, token in enumerate(tokens):
            assignment = re.fullmatch(r'([A-Za-z_][A-Za-z0-9_]*)=(.*)',
                                      token.text, re.S)
            if assignment:
                name, value = assignment.groups()
                parsed = parameter(value)
                derived.pop(name, None)
                aliases.pop(name, None)
                if parsed:
                    var, op, pattern = parsed
                    if var == name:
                        continue
                    if op:
                        derived[name] = (aliases.get(var, var), op, pattern)
                    elif var in derived:
                        derived[name] = derived[var]
                    else:
                        aliases[name] = aliases.get(var, var)
            if token.text not in ('test', '[', '[['):
                continue
            if i and tokens[i - 1].text not in (
                    ';', '&&', '||', '!', '{', '(', ')', 'if', 'elif', 'else',
                    'then', 'do', 'while', 'until',
                    'command', 'builtin'):
                continue
            operand = i + 1
            if operand < len(tokens) and tokens[operand].text == '!':
                operand += 1
            if operand + 2 >= len(tokens):
                continue
            left, operator, right = tokens[operand:operand + 3]
            lhs, rhs = parameter(left.text), parameter(right.text)
            for a, b in ((lhs, rhs), (rhs, lhs)):
                if not a or not b or b[1]:
                    continue
                removed = (aliases.get(a[0], a[0]), a[1], a[2]) if a[1] \
                    else derived.get(a[0])
                if removed and operator.text in ('=', '!=', '==') and \
                        removed[0] == aliases.get(b[0], b[0]):
                    findings.append({
                        'line': source.count('\n', 0, offset) + 1,
                        'statement': raw.rstrip('\r\n'),
                        'logical': logical,
                        'start': token.start,
                        'end': right.end,
                        'source': b[0],
                        'operator': operator.text,
                        'removal': removed[1],
                        'pattern': removed[2],
                    })
                    break
            else:
                # Bash regex/glob predicates are pattern matching too.
                if token.text == '[[' and (operator.text == '=~' or
                        (operator.text in ('=', '==', '!=') and
                         shell_pattern(right.text))):
                    findings.append({
                        'line': source.count('\n', 0, offset) + 1,
                        'statement': raw.rstrip('\r\n'),
                        'logical': logical,
                        'start': token.start,
                        'end': right.end,
                    })
    # One statement has one approval identity even if both rules detect it.
    keys = {(f['line'], f['statement']) for f in findings}
    findings.extend(f for f in redundant_prefix_scans(source)
                    if (f['line'], f['statement']) not in keys)
    return findings


def exceptions(document, findings):
    """Validate exact, reviewable approvals; never accept a broad file skip."""
    if set(document) != {'version', 'entries'} or document['version'] != 1:
        raise ValueError('whitelist must have version 1 and entries')
    if not isinstance(document['entries'], list):
        raise ValueError('whitelist entries must be a list')
    approved = set()
    available = {(f['path'], f['line'], f['sha256']) for f in findings}
    for item in document['entries']:
        if set(item) != {'path', 'line', 'sha256', 'reason', 'approval'}:
            raise ValueError('whitelist needs exact location, reason, approval')
        path = item['path']
        if not isinstance(path, str) or not path.startswith('tests/') or \
                not path.endswith('.t') or '..' in Path(path).parts or \
                re.search(r'[*?\[\\]', path):
            raise ValueError('whitelist path must name one shell TAP file')
        if type(item['line']) is not int or item['line'] < 1 or \
                not re.fullmatch(r'[a-f0-9]{64}', item['sha256']):
            raise ValueError('whitelist needs a positive line and SHA-256')
        if any(not isinstance(item[k], str) or not item[k].strip()
               for k in ('reason', 'approval')):
            raise ValueError('whitelist requires necessity and user approval')
        key = path, item['line'], item['sha256']
        if key in approved:
            raise ValueError('duplicate whitelist entry: ' + path)
        if key not in available:
            raise ValueError('stale whitelist entry: ' + path)
        approved.add(key)
    return approved


def audit(root):
    findings = []
    paths = sorted(p for p in (root / 'tests').rglob('*.t')
                   if not {'_artifacts', 'data'}.intersection(
                       p.relative_to(root / 'tests').parts[:-1]))
    if not paths:
        raise ValueError('no shell TAP files found')
    for path in paths:
        for finding in predicates(path.read_text(encoding='utf-8')):
            finding['path'] = path.relative_to(root).as_posix()
            finding['sha256'] = hashlib.sha256(
                finding['statement'].encode()).hexdigest()
            findings.append(finding)
    approved = exceptions(json.loads((root / 'tests/_static/data/'
        'test-pattern-match-whitelist.json').read_text()), findings)
    rejected = [f for f in findings if
                (f['path'], f['line'], f['sha256']) not in approved]
    for finding in rejected:
        print('# {}:{}: {} (sha256 {})'.format(
            finding['path'], finding['line'],
            finding.get('reason', 'use case for pattern matching'),
            finding['sha256']))
    return not rejected


def selftest():
    """Independent lexical and exception controls run with every audit."""
    cases = [
        ('test "${x#*needle}" != "$x"', 1),
        ('{ test "${x#*needle}" != "$x"; }', 1),
        ('(test "${x#*needle}" != "$x")', 1),
        ('case "$a" in *) test "${x#*needle}" != "$x" ;; esac', 1),
        ('test ! "${x#*needle}" != "$x"', 1),
        ('while test "${x#*needle}" != "$x"; do :; done', 1),
        ('test "$x" = "${x%needle*}"', 1),
        ('[ "${x##*needle}" != "${x}" ]', 1),
        ('test "${x%%needle*}" = "$x"', 1),
        ('tail=${x#*needle}\ntest "$tail" != "$x"', 1),
        ('tail=${x#*needle}\ncopy="$tail"\n'
         'test "$copy" != "$x"', 1),
        ('test "${x#*needle}" \\\n != "$x"', 1),
        ('test "${x#\'line\n\'}" != "$x"', 1),
        ('test "${x#*"${needle}"}" != "$x"', 1),
        ('[[ "$x" == *needle* ]]', 1),
        ('[[ "$x" == $pattern ]]', 1),
        ('[[ "$x" == "${prefix}"* ]]', 1),
        ('[[ "$x" == "*needle*" ]]', 0),
        ('[[ "$x" =~ needle ]]', 1),
        ('case "$x" in *needle*) true ;; *) false ;; esac', 0),
        ('tail=${trace#*marker*}\ncase "$tail" in *next*) :;; esac', 1),
        ('tail="${trace#*"${marker}"*}"', 1),
        ('trace=${trace#*marker*}', 1),
        ('test "${trace#*marker*}" != "$trace"', 1),
        ('# tail=${trace#*marker*}', 0),
        ("printf '%s' '${trace#*marker*}'", 0),
        ("cat <<'EOF'\ntail=${trace#*marker*}\nEOF\n", 0),
        ('tail=${trace#*marker\\*}', 0),
        ('tail=${trace#*"marker*"}', 0),
        ('tail=${trace#"*marker*"}', 0),
        ('field=${trace#*key=}', 0),
        ('field=${line##*=}\ntest "$field" = expected', 0),
        ('test "${file##*/}" = fixture.png', 0),
        ('x=${x#needle}\ntest "$x" = "$x"', 0),
        ('# test "${x#*needle}" != "$x"', 0),
        ("printf '%s\\n' 'test \"${x#*needle}\" != \"$x\"'", 0),
        ("cat <<'EOF'\ntest \"${x#*needle}\" != \"$x\"\nEOF\n", 0),
        ("cat <<'END OF DATA'\ntest \"${x#*needle}\" != \"$x\"\n"
         "END OF DATA\n", 0),
        ('cat <<- END-1\n\ttest "${x#*needle}" != "$x"\n\tEND-1\n', 0),
        ('cat <<-EOF\n\ttest "${x#*needle}" != "$x"\n\tEOF\n'
         'test "${x#*needle}" != "$x"', 1),
        ('tail=${x#needle}\ntail=other\ntest "$tail" != "$x"', 0),
    ]
    for source, count in cases:
        if len(predicates(source)) != count:
            raise ValueError('pattern checker self-test failed: ' + repr(source))
    finding = {'path': 'tests/control.t', 'line': 3, 'sha256': 'a' * 64}
    entry = dict(finding, reason='synthetic necessity control',
                 approval='synthetic control, not an actual user approval')
    valid = {'version': 1, 'entries': [entry]}
    if len(exceptions(valid, [finding])) != 1:
        raise ValueError('whitelist positive control failed')
    bad_entries = [dict(entry, reason=''), dict(entry, approval=''),
                   dict(entry, path='tests/*.t'), dict(entry, line=0),
                   dict(entry, sha256='b' * 64), dict(entry, line=4),
                   dict(entry, extra='broad waiver')]
    invalid = [{'version': 1, 'entries': [e]} for e in bad_entries]
    invalid += [{'version': 1, 'entries': [entry, entry]},
                {'version': 2, 'entries': []},
                {'version': 1, 'entries': 'all'}]
    for document in invalid:
        try:
            exceptions(document, [finding])
        except ValueError:
            continue
        raise ValueError('invalid whitelist control was accepted')


def main():
    try:
        selftest()
        success = audit(Path(sys.argv[1]))
    except (OSError, ValueError, KeyError, TypeError) as error:
        print('# ' + str(error))
        return 1
    return 0 if success else 1


if __name__ == '__main__':
    sys.exit(main())
