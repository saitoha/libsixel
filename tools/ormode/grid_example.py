#!/usr/bin/env python3
"""Encode and verify the fixed 3-by-6 indexed example used in the OR guide."""

import argparse
import ctypes as C
import hashlib
import json
import os
import re
import subprocess
from io import BytesIO
from pathlib import Path

from PIL import Image

OUT = Path(__file__).resolve().parents[2] / "docs/functionality/or-mode/grid-example"
ROWS = [[0, 1, 2], [3, 4, 5], [6, 7, 0], [1, 2, 3], [4, 5, 6], [7, 0, 1]]
PALETTE = [
    (255, 255, 255),
    (255, 153, 153),
    (255, 204, 102),
    (153, 204, 153),
    (102, 153, 255),
    (204, 153, 255),
    (255, 153, 204),
    (153, 204, 204),
]


def generate(library, decoder):
    lib = C.CDLL(str(library))
    ptr = C.c_void_p
    callback_type = C.CFUNCTYPE(C.c_int, ptr, C.c_int, ptr)
    signatures = {
        "sixel_dither_new": ([C.POINTER(ptr), C.c_int, ptr], C.c_int),
        "sixel_output_new": ([C.POINTER(ptr), callback_type, ptr, ptr], C.c_int),
        "sixel_encode": ([ptr, C.c_int, C.c_int, C.c_int, ptr, ptr], C.c_int),
        "sixel_dither_set_palette": ([ptr, ptr], None),
    }
    for name in [
        "sixel_dither_set_pixelformat",
        "sixel_dither_set_optimize_palette",
        "sixel_output_set_ormode",
        "sixel_output_set_encode_policy",
        "sixel_output_set_palette_type",
    ]:
        signatures[name] = ([ptr, C.c_int], None)
    for name in ["sixel_dither_unref", "sixel_output_unref"]:
        signatures[name] = ([ptr], None)
    for name, (args, result) in signatures.items():
        fn = getattr(lib, name)
        fn.argtypes = args
        fn.restype = result

    def check(status):
        assert not status & 0x1000, hex(status)

    source = Image.new("P", (3, 6))
    source.putpalette([v for rgb in PALETTE for v in rgb])
    source.putdata([v for row in ROWS for v in row])
    OUT.mkdir(exist_ok=True)
    source.save(OUT / "input.png")
    records = {}
    for mode in [0, 1]:
        chunks = []

        @callback_type
        def write(data, size, unused, chunks=chunks):
            chunks.append(C.string_at(data, size))
            return size

        dither, output = ptr(), ptr()
        check(lib.sixel_dither_new(C.byref(dither), 8, None))
        palette = (C.c_ubyte * 24)(*[v for rgb in PALETTE for v in rgb])
        lib.sixel_dither_set_palette(dither, palette)
        lib.sixel_dither_set_pixelformat(dither, 0x83)
        lib.sixel_dither_set_optimize_palette(dither, 0)
        check(lib.sixel_output_new(C.byref(output), write, None, None))
        lib.sixel_output_set_ormode(output, mode)
        lib.sixel_output_set_encode_policy(output, 1)
        lib.sixel_output_set_palette_type(output, 2)
        pixels = (C.c_ubyte * 18)(*[v for row in ROWS for v in row])
        check(lib.sixel_encode(pixels, 3, 6, 1, dither, output))
        lib.sixel_output_unref(output)
        lib.sixel_dither_unref(dither)
        stream = b"".join(chunks)
        name = "or" if mode else "normal"
        (OUT / f"{name}.six").write_bytes(stream)
        env = dict(
            os.environ,
            DYLD_LIBRARY_PATH=str(library.parent),
            LD_LIBRARY_PATH=str(library.parent),
        )
        decoded = subprocess.run(
            [str(decoder), "--direct", "--threads=1"],
            input=stream,
            capture_output=True,
            env=env,
            check=True,
        )
        image = Image.open(BytesIO(decoded.stdout)).convert("RGB")
        assert (
            image.size == source.size
            and image.tobytes() == source.convert("RGB").tobytes()
        )
        definitions = list(re.finditer(rb"#[0-9]+;2;[0-9]+;[0-9]+;[0-9]+", stream))
        body = stream[definitions[-1].end() : -2].decode("ascii")
        records[name] = {
            "body": body,
            "body_bytes": len(body),
            "total_bytes": len(stream),
            "exact_rgb": True,
        }
    result = {
        "rows": ROWS,
        "palette": PALETTE,
        "policy": "fast",
        "palette_optimization": False,
        "library_sha256": hashlib.sha256(library.read_bytes()).hexdigest(),
        "source_revision": subprocess.check_output(
            ["git", "-C", str(library.parent), "rev-parse", "HEAD"], text=True
        ).strip(),
        "outputs": records,
    }
    (OUT / "example.json").write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(records, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--library", type=Path, required=True)
    parser.add_argument("--decoder", type=Path, required=True)
    args = parser.parse_args()
    generate(args.library.resolve(), args.decoder.resolve())
