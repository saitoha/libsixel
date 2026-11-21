#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Embed GLSL or Metal bytes in portable C string-array fragments."""

import os
from pathlib import Path
import sys
import tempfile


ESCAPES = {ord(c): b"\\" + replacement for c, replacement in [
    ("\\", b"\\"), ('"', b'"'), ("\t", b"t"), ("\r", b"r"),
    ("\n", b"n"), ("?", b"?"),
]}


def embed(source, output):
    output.write(b"/* SPDX-License-Identifier: MIT */\n"
                 b"/* Generated from shader source. Edit the source file. */\n")
    # Match the line-based encoder, including its newline on a final
    # unterminated input line. Count bytes before escaping them.
    line = bytearray()
    prefix = b"("
    size = 0
    for source_line in source:
        for value in source_line.rstrip(b"\n") + b"\n":
            encoded = ESCAPES.get(value, bytes([value]))
            if len(line) + len(encoded) > 72:
                output.write(prefix + b'"' + line + b'"\n')
                prefix = b""
                line.clear()
            line.extend(encoded)
            size += 1
            if size == 1024:
                output.write(prefix + b'"' + line + b'"),\n')
                prefix = b"("
                line.clear()
                size = 0
    if size:
        output.write(prefix + b'"' + line + b'"),\n')


def main(source, output):
    with Path(source).open("rb") as input_file:
        if output == "-":
            embed(input_file, sys.stdout.buffer)
            return
        destination = Path(output)
        with tempfile.NamedTemporaryFile(dir=destination.parent, delete=False) as temp:
            temporary = temp.name
            try:
                embed(input_file, temp)
                temp.close()
                os.replace(temporary, destination)
            finally:
                if os.path.exists(temporary):
                    os.unlink(temporary)


if __name__ == "__main__":
    main(*sys.argv[1:])
