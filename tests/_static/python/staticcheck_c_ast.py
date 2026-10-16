#!/usr/bin/env python3
"""Run tree-sitter-c based static checks for C source contracts."""

from __future__ import annotations

import re
import sys
from pathlib import Path
from typing import Iterable, Iterator, List, Sequence


class Finding:
    def __init__(self, path: Path, line: int, message: str) -> None:
        self.path = path
        self.line = line
        self.message = message

    def format(self, src_root: Path) -> str:
        relpath = self.path.relative_to(src_root)
        return f"{relpath}:{self.line}: {self.message}"


class SourceUnit:
    def __init__(self, src_root: Path, path: Path, parser,
                 source=None) -> None:
        self.src_root = src_root
        self.path = path
        self.source = path.read_bytes() if source is None else source
        self.tree = parser.parse(self.source)

    def text(self, node) -> str:
        return self.source[node.start_byte:node.end_byte].decode(
            "utf-8",
            errors="replace",
        )

    def line(self, node) -> int:
        point = node.start_point
        row = getattr(point, "row", None)
        if row is None:
            row = point[0]
        return int(row) + 1


class Rule:
    name = "rule"
    path_prefixes: Sequence[str] = ()

    def should_check(self, relpath: str) -> bool:
        return any(relpath.startswith(prefix) for prefix in self.path_prefixes)

    def check(self, unit: SourceUnit) -> Iterable[Finding]:
        raise NotImplementedError


def walk(node) -> Iterator:
    stack = [node]
    while stack:
        current = stack.pop()
        yield current
        stack.extend(reversed(current.children))


def call_name(unit: SourceUnit, node) -> str:
    callee = node.child_by_field_name("function")
    if callee is None:
        return ""
    text = unit.text(callee).strip()
    if "->" in text:
        return text.rsplit("->", 1)[-1].strip()
    if "." in text:
        return text.rsplit(".", 1)[-1].strip()
    return text


def call_arguments(unit: SourceUnit, node) -> str:
    arguments = node.child_by_field_name("arguments")
    if arguments is None:
        return ""
    return unit.text(arguments).strip()


class TimelineLogPathRule(Rule):
    name = "timeline-log-path-copy"
    path_prefixes = ("tests/processing/timeline/",)

    def check(self, unit: SourceUnit) -> Iterable[Finding]:
        findings: List[Finding] = []
        for node in walk(unit.tree.root_node):
            if node.type == "declaration":
                text = unit.text(node)
                if re.search(r"\bchar\s+const\s*\*\s*log_path\b", text):
                    findings.append(Finding(
                        unit.path,
                        unit.line(node),
                        "timeline tests must copy SIXEL_LOG_PATH",
                    ))
                if re.search(r"\bconst\s+char\s*\*\s*log_path\b", text):
                    findings.append(Finding(
                        unit.path,
                        unit.line(node),
                        "timeline tests must copy SIXEL_LOG_PATH",
                    ))
            if node.type == "call_expression":
                name = call_name(unit, node)
                if name == "sixel_sleep":
                    findings.append(Finding(
                        unit.path,
                        unit.line(node),
                        "timeline tests must not link internal sleep helper",
                    ))
        findings.extend(self._check_getenv_copy(unit))
        return findings

    def _check_getenv_copy(self, unit: SourceUnit) -> Iterable[Finding]:
        findings: List[Finding] = []
        for node in walk(unit.tree.root_node):
            if node.type != "function_definition":
                continue
            saw_log_path_getenv = False
            saw_log_path_copy = False
            getenv_line = unit.line(node)
            for child in walk(node):
                if child.type != "call_expression":
                    continue
                name = call_name(unit, child)
                arguments = call_arguments(unit, child)
                if (name == "sixel_compat_getenv" and
                        '"SIXEL_LOG_PATH"' in arguments):
                    saw_log_path_getenv = True
                    getenv_line = unit.line(child)
                if (name == "sixel_compat_strcpy" and
                        re.match(r"\(\s*log_path\s*,", arguments)):
                    saw_log_path_copy = True
            if saw_log_path_getenv and not saw_log_path_copy:
                findings.append(Finding(
                    unit.path,
                    getenv_line,
                    "SIXEL_LOG_PATH must be copied before writer use",
                ))
        return findings


class StatusAdditionalMessageStorageRule(Rule):
    name = "status-additional-message-storage"
    path_prefixes = ("src/status.c",)

    def check(self, unit: SourceUnit) -> Iterable[Finding]:
        findings: List[Finding] = []
        text = unit.source.decode("utf-8", errors="replace")
        tls_pattern = (
            r"static\s+SIXEL_STATUS_TLS\s+"
            r"sixel_status_message_store_t\s+status_message_store\s*;"
        )

        if not re.search(tls_pattern, text):
            findings.append(Finding(
                unit.path,
                1,
                "additional message store must be thread-local",
            ))

        for node in walk(unit.tree.root_node):
            if node.type != "declaration":
                continue
            declaration = unit.text(node)
            if re.search(r"\bstatic\s+char\s+status_markup_storage\b",
                         declaration):
                findings.append(Finding(
                    unit.path,
                    unit.line(node),
                    "status markup storage must not be process-global",
                ))
            if re.search(r"\bstatic\s+char\s+status_render_storage\b",
                         declaration):
                findings.append(Finding(
                    unit.path,
                    unit.line(node),
                    "status render storage must not be process-global",
                ))
        return findings


class NonfiniteStaticInitializerRule(Rule):
    name = "nonfinite-static-initializer"
    path_prefixes = (
        "src/", "include/", "converters/", "assessment/", "tests/",
        "tools/", "examples/", "fuzz/", "gdk-pixbuf-loader/", "wic/",
        "quicklook-extension/",
    )

    def check(self, unit: SourceUnit) -> Iterable[Finding]:
        for node in walk(unit.tree.root_node):
            if node.type != "declaration":
                continue
            storage = {
                unit.text(child) for child in node.named_children
                if child.type == "storage_class_specifier"
            }
            # File-scope objects have static storage even without `static`.
            # Preprocessor conditionals do not introduce a C block scope.
            ancestor = node.parent
            while ancestor is not None and ancestor.type not in (
                    "compound_statement", "function_definition"):
                ancestor = ancestor.parent
            if ancestor is not None and not storage.intersection({
                    "static", "extern", "_Thread_local", "thread_local"}):
                continue
            for declarator in node.named_children:
                if declarator.type != "init_declarator":
                    continue
                value = declarator.child_by_field_name("value")
                pending = [value] if value is not None else []
                while pending:
                    child = pending.pop()
                    # These operators do not evaluate the floating operand.
                    if child.type in ("sizeof_expression", "alignof_expression"):
                        continue
                    if (child.type == "identifier" and
                            unit.text(child) in ("NAN", "INFINITY")):
                        yield Finding(
                            unit.path, unit.line(child),
                            f"{unit.text(child)} in a static-storage initializer "
                            "is not portable to OpenVMS; initialize at runtime",
                        )
                    pending.extend(reversed(child.named_children))


def check_nonfinite_initializer_fixtures(src_root: Path, parser) -> None:
    # Exercise the production rule without compiling C or writing fixtures.
    # Keep both the original nested-array failure and implicit file storage.
    fixtures = (
        ("nested static array", b"""void f(void) {
static double const invalid[3][3] = {
    {NAN, 0, 0}, {0, INFINITY, 0}, {1e300, 0, 0}
};
}""", [3, 3]),
        ("implicit file storage", b"float value = NAN;", [1]),
        ("conditional file storage", b"#if ENABLED\nfloat value = INFINITY;\n#endif", [2]),
        ("multiple declarators", b"static float a = 0, b = -INFINITY;", [1]),
        ("header inline body", b"static inline void f(void) { static float x = NAN; }", [1]),
        ("qualified local", b"void f(void) { const static float x = (float)NAN; }", [1]),
        ("automatic array", b"void f(void) { const double x[] = {NAN, INFINITY}; }", []),
        ("automatic conditional", b"void f(void) {\n#if X\nfloat x = NAN;\n#endif\n}", []),
        ("runtime assignment", b"static float x; void f(void) { x = NAN; }", []),
        ("finite initializer", b"static double x[] = {0.0, 1e300};", []),
        ("comment and string", b'static const char *s = "NAN INFINITY"; /* NAN */', []),
        ("unevaluated operand", b"static unsigned n = sizeof(NAN) + sizeof(INFINITY);", []),
    )
    rule = NonfiniteStaticInitializerRule()
    for name, source, expected in fixtures:
        unit = SourceUnit(src_root, src_root / "src/staticcheck-fixture.c",
                          parser, source)
        actual = [finding.line for finding in rule.check(unit)]
        if unit.tree.root_node.has_error or actual != expected:
            raise RuntimeError(
                f"nonfinite initializer fixture {name}: "
                f"expected lines {expected}, got {actual}"
            )


def make_parser():
    from tree_sitter import Language, Parser
    import tree_sitter_c

    raw_language = tree_sitter_c.language()
    try:
        language = Language(raw_language)
    except TypeError:
        language = raw_language
    try:
        return Parser(language)
    except TypeError:
        parser = Parser()
        try:
            parser.set_language(language)
        except AttributeError:
            parser.language = language
        return parser


def probe() -> int:
    try:
        make_parser()
    except Exception as exc:
        print(f"tree-sitter-c probe failed: {exc}", file=sys.stderr)
        return 1
    return 0


def collect_files(src_root: Path, rules: Sequence[Rule]) -> List[Path]:
    prefixes = tuple(prefix for rule in rules for prefix in rule.path_prefixes)
    files: List[Path] = []
    for suffix in ("*.c", "*.h", "*.inc.c"):
        for path in src_root.rglob(suffix):
            relpath = path.relative_to(src_root).as_posix()
            if prefixes and relpath.startswith(prefixes):
                files.append(path)
    return sorted(set(files))


def main(argv: Sequence[str]) -> int:
    if len(argv) == 2 and argv[1] == "--probe":
        return probe()

    if len(argv) != 2:
        print("usage: staticcheck_c_ast.py <src_root>|--probe",
              file=sys.stderr)
        return 2

    src_root = Path(argv[1]).resolve()
    parser = make_parser()
    check_nonfinite_initializer_fixtures(src_root, parser)
    rules: Sequence[Rule] = (
        TimelineLogPathRule(),
        StatusAdditionalMessageStorageRule(),
        NonfiniteStaticInitializerRule(),
    )
    findings: List[Finding] = []

    for path in collect_files(src_root, rules):
        relpath = path.relative_to(src_root).as_posix()
        unit = SourceUnit(src_root, path, parser)
        for rule in rules:
            if rule.should_check(relpath):
                findings.extend(rule.check(unit))

    for finding in findings:
        print(finding.format(src_root))

    return 1 if findings else 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
