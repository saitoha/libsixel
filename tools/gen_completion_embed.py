#!/usr/bin/env python3
"""Generate completion_embed.h for img2sixel.

This script builds a tiny header that embeds both bash and zsh
completion scripts as string literals so that the runtime can fall back
on in-memory data when disk files are unavailable.

ASCII map of the data flow:

    [bash completion] ----\
                             >--[embedder]--> completion_embed.h
    [zsh completion]  ----/

The resulting header defines two C arrays that mirror the input files.
"""

from __future__ import annotations

import argparse
import pathlib
import sys
from typing import Iterable


def _read_text(path: pathlib.Path) -> str:
    """Load a text file while preserving trailing newlines."""
    return path.read_text(encoding="utf-8")


def _escape_lines(text: str) -> Iterable[str]:
    """Yield C string literal lines that reproduce *text* verbatim."""

    # Keep generated literals well under the 4095-byte limit required by ISO
    # C99 to avoid -Woverlength-strings warnings on strict compilers.
    chunk_limit = 2000

    for line in text.splitlines(True):
        escaped = line.replace("\\", "\\\\").replace("\"", "\\\"")
        escaped = escaped.replace("\n", "\\n")
        while escaped:
            chunk = escaped[:chunk_limit]
            escaped = escaped[chunk_limit:]
            yield f"\"{chunk}\""
    if not text.endswith("\n"):
        yield "\"\""


def _write_header(path: pathlib.Path, bash_text: str, zsh_text: str) -> None:
    """Emit the header file contents."""
    lines = [
        "/* auto-generated; do not edit */",
        "#ifndef SIXEL_CONVERTERS_COMPLETION_EMBED_H",
        "#define SIXEL_CONVERTERS_COMPLETION_EMBED_H",
        "",
        "static const char img2sixel_bash_completion[] =",
        *(_escape_lines(bash_text)),
        "\"\";",
        "",
        "static const char img2sixel_zsh_completion[] =",
        *(_escape_lines(zsh_text)),
        "\"\";",
        "",
        "#endif /* SIXEL_CONVERTERS_COMPLETION_EMBED_H */",
    ]
    path.write_text("\n".join(lines), encoding="utf-8")


def main(argv: list[str]) -> int:
    parser = argparse.ArgumentParser(
        description="Create completion_embed.h from shell completion inputs.",
    )
    parser.add_argument("output", type=pathlib.Path)
    parser.add_argument("bash_source", type=pathlib.Path)
    parser.add_argument("zsh_source", type=pathlib.Path)
    args = parser.parse_args(argv)

    try:
        bash_text = _read_text(args.bash_source)
        zsh_text = _read_text(args.zsh_source)
        args.output.parent.mkdir(parents=True, exist_ok=True)
        _write_header(args.output, bash_text, zsh_text)
    except OSError as exc:
        print(f"gen_completion_embed.py: {exc}", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
