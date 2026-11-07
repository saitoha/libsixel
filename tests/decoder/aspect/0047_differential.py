#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Opt-in segmented/reference equivalence; never built or run by default.

python3 tests/decoder/aspect/0047_differential.py BUILD/src/libsixel.dylib
Uses internal test exports, deterministic seeds, and no generated files.
Compares status, geometry, flags, palette, pixels and masks, RGB/RGBA fast4,
whole captures and animation frames at thread budgets 1 and 4. Both automatic
fallback and forced segments are checked against the dedicated device painter.
The sibling sequence runner supplies shared bindings and grammar specimens.
"""
import argparse
import ctypes as C
import importlib.util
from pathlib import Path
import random


spec = importlib.util.spec_from_file_location(
    "sequence_differential",
    Path(__file__).resolve().parents[1] / "sequence/0001_differential.py")
sequence = importlib.util.module_from_spec(spec)
spec.loader.exec_module(sequence)
PTR, INT, UINT = sequence.PTR, sequence.INT, sequence.UINT
bind = sequence.bind


class Decoder(sequence.Decoder):
    def __init__(self, path):
        super().__init__(path)
        self.reference = bind(self.lib, "sixel_decode_aspect_reference",
                              [INT], None)
        tail = [C.POINTER(INT), C.POINTER(INT), C.POINTER(PTR),
                C.POINTER(INT), C.POINTER(UINT), PTR]
        self.raw = bind(self.lib, "sixel_decode_raw_with_options_mask",
                        [PTR, INT, UINT, C.POINTER(PTR), C.POINTER(PTR)] + tail)
        self.direct = bind(self.lib, "sixel_decode_direct_with_options",
                           [PTR, INT, UINT, C.POINTER(PTR)] + tail)
        self.fast4 = bind(self.lib, "sixel_decode_kundither_fast4_with_options",
                          [PTR, INT, INT, INT, UINT, C.POINTER(UINT),
                           C.POINTER(PTR), C.POINTER(INT), C.POINTER(INT), PTR])

    def pixels(self, data, trusted, kind):
        buf = C.create_string_buffer(data)
        pixels, mask, palette = PTR(), PTR(), PTR()
        width, height, colors, flags = INT(), INT(), INT(), UINT()
        if kind >= 2:
            status = self.fast4(buf, len(data), kind - 2, 0, trusted,
                                C.byref(flags), C.byref(pixels),
                                C.byref(width), C.byref(height), self.alloc)
        else:
            args = [buf, len(data), trusted, C.byref(pixels)]
            if kind == 0:
                args.append(C.byref(mask))
            status = (self.raw if kind == 0 else self.direct)(
                *args, C.byref(width), C.byref(height), C.byref(palette),
                C.byref(colors), C.byref(flags), self.alloc)
        try:
            size = width.value * height.value
            depth = (1, 4, 3, 4)[kind]
            return (status, width.value, height.value, colors.value, flags.value,
                    C.string_at(pixels, size * depth) if pixels else None,
                    C.string_at(mask, size) if mask else None,
                    C.string_at(palette, max(0, colors.value) * 3)
                    if palette else None)
        finally:
            for pointer in (pixels, mask, palette):
                self.free(self.alloc, pointer)


def specimens(rng, count):
    colors = b'#0;2;100;0;0#1;2;0;100;0#2;2;0;0;100'
    # Empty segments advance the cursor without allocating device rows;
    # default-color paint wholly outside Pv must still update color counts.
    for body in (b'"1;1;1;1-"2;1;1;1@',
                 b'"1;1;1;1-"2;1;1;1?',
                 b'"1000000;1;1;1-"1;1;1;1-',
                 b'"1000000;1;1;1' + b'-' * 357 + b'"1;1;1;1',
                 b'"1000000;1;1;1' + b'-' * 358 + b'"1;1;1;1'):
        yield b'\x1bPq' + body + b'\x1b\\'
    for index in range(count):
        width, height = rng.randint(1, 32), rng.randint(1, 65)
        if index % 4 == 0:
            width, height = 64, 1536
        chunks = [colors]
        # Exercise the density fallback, and forced segments, with exactly
        # the repeated rewind/paint/change pattern that defeats x slicing.
        for turn in range(rng.randint(2, 35)):
            scale = rng.randint(1, 6)
            if rng.randrange(6) == 0:
                chunks.append(b'-' * rng.randint(1, 3))
            chunks.append(f'"{scale};1;{width};{height}'.encode())
            chunks.append(f'#{rng.randrange(3)}!{rng.randint(1, 40)}'.encode()
                          + bytes([rng.randint(63, 126)]))
            chunks.append(b'$' + bytes([rng.randint(63, 126)]))
            if rng.randrange(4) == 0:
                chunks.append(f'#{rng.randrange(3)};2;{rng.randrange(101)};'
                              f'{rng.randrange(101)};{rng.randrange(101)}'.encode())
            if rng.randrange(6) == 0:
                chunks.append(b'#257;2;100;0;100@')
            if rng.randrange(8) == 0 and index % 4 != 0:
                # Raster edits within one scale need the preceding clip.
                chunks.append(f'"{scale};1;{rng.randint(1, 32)};'
                              f'{rng.randint(1, 65)}~'.encode())
        body = b''.join(chunks)
        stream = sequence.packet(body, width, height, p2=index % 3,
                                 c1=bool(index % 2))
        yield stream
        if index % 7 == 0:
            yield sequence.packet(body, width, height, p2=5)
        if index % 3 == 0:
            yield stream + sequence.packet(colors + b'"2;1;'
                  + f'{width};{height}'.encode() + b'@$A"1;1;'
                  + f'{width};{height}'.encode() + b'B$C', width, height)


def equivalent(decoder, label, data, call):
    decoder.reference(1)
    reference = call()
    for mode in (0, 2):
        decoder.reference(mode)
        actual = call()
        if actual != reference:
            raise AssertionError((label, mode, data, "result mismatch"))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("library")
    parser.add_argument("--seed", type=int, default=183)
    parser.add_argument("--cases", type=int, default=2000)
    parser.add_argument("--grammar", action="store_true",
                        help="also exercise the sequence grammar corpus")
    args = parser.parse_args()
    decoder = Decoder(args.library)
    streams = list(specimens(random.Random(args.seed), args.cases))
    if args.grammar:
        streams += list(sequence.specimens(random.Random(args.seed), 0))
    try:
        for budget in (1, 4):
            assert decoder.threads(str(budget).encode()) == 0
            for index, data in enumerate(streams):
                label = (args.seed, budget, index)
                for trusted in (0, 1):
                    for kind in range(4):
                        equivalent(decoder, label + (trusted, kind), data,
                                   lambda: decoder.pixels(data, trusted, kind))
                for animation in (0, 1):
                    equivalent(decoder, label + ("frames", animation), data,
                               lambda: decoder.frames(data, animation, False))
                for transformed in (False, True):
                    equivalent(decoder, label + ("capture", transformed), data,
                               lambda: decoder.capture(data, False, transformed))
            print(f"budget={budget}: {len(streams)} streams passed", flush=True)
    finally:
        decoder.reference(0)
        decoder.threads(b"1")
        decoder.unref(decoder.alloc)


if __name__ == "__main__":
    main()
