#!/usr/bin/env python3
# SPDX-License-Identifier: MIT
"""Opt-in exact/fast sequence equivalence; not registered in the default suite.

Run: python3 tests/decoder/sequence/0001_differential.py BUILD/src/LIBSIXEL
The shared library must export internal symbols (as native test builds do).
No fixtures or output files are created. A failure prints a reproducible seed,
case number and byte-string specimen. --cases controls generated captures;
fixed grammar boundaries always run. Python is only an optional test dependency.
Failed packets retain the preceding canvas atomically; the reference's eager
P2=0 clear on failure is an intentional exception to byte-for-byte equivalence.
"""
import argparse
import ctypes as C
import random


PTR = C.c_void_p
INT = C.c_int
UINT = C.c_uint
SIZE = C.c_size_t
CALLBACK = C.CFUNCTYPE(None, PTR, C.POINTER(C.c_ubyte), INT)


class Transform(C.Structure):
    _fields_ = [("context", PTR), ("map_rgb", CALLBACK)]


@CALLBACK
def map_rgb(_context, rgb, is_default):
    rgb[0], rgb[1], rgb[2] = rgb[2] ^ 17, rgb[0] ^ 33, rgb[1] ^ is_default


def bind(lib, name, args, result=INT):
    fn = getattr(lib, name)
    fn.argtypes, fn.restype = args, result
    return fn


class Decoder:
    def __init__(self, path):
        self.lib = C.CDLL(path)
        self.open = bind(self.lib, "sixel_sequence_open",
                         [C.POINTER(PTR), PTR, SIZE, INT, PTR])
        self.next = bind(self.lib, "sixel_sequence_next",
                         [PTR, C.POINTER(PTR), C.POINTER(INT),
                          C.POINTER(INT), C.POINTER(SIZE)])
        self.close = bind(self.lib, "sixel_sequence_close", [PTR], None)
        self.exact = bind(self.lib, "sixel_sequence_set_exact", [PTR], None)
        self.threads = bind(self.lib, "sixel_decoder_parallel_override_threads",
                            [C.c_char_p])
        args = [PTR, SIZE, PTR, C.POINTER(PTR), C.POINTER(INT),
                C.POINTER(INT), C.POINTER(UINT), C.POINTER(SIZE), PTR]
        self.mapped = [bind(self.lib, name, args) for name in
                       ("sixel_decode_sequence_mapped",
                        "sixel_decode_sequence_mapped_exact")]
        self.alloc = PTR()
        new = bind(self.lib, "sixel_allocator_new",
                   [C.POINTER(PTR), PTR, PTR, PTR, PTR])
        assert new(C.byref(self.alloc), None, None, None, None) == 0
        self.free = bind(self.lib, "sixel_allocator_free", [PTR, PTR], None)
        self.unref = bind(self.lib, "sixel_allocator_unref", [PTR], None)
        self.transform = Transform(None, map_rgb)

    def frames(self, data, animation, exact):
        decoder = PTR()
        buf = C.create_string_buffer(data)
        status = self.open(C.byref(decoder), buf, len(data), animation,
                           self.alloc)
        if status:
            return (status,)
        if exact:
            self.exact(decoder)
        results = []
        previous = None
        try:
            while True:
                pixels, w, h, end = PTR(), INT(-7), INT(-9), SIZE(99)
                status = self.next(decoder, C.byref(pixels), C.byref(w),
                                   C.byref(h), C.byref(end))
                if status:
                    # Failure must retain the last published canvas, and the
                    # next call must keep the iterator terminal. EOF may repeat.
                    retained = (C.string_at(*previous) if previous else None)
                    again = self.next(decoder, C.byref(pixels), C.byref(w),
                                      C.byref(h), C.byref(end))
                    results.append((status, again, bool(pixels), w.value,
                                    h.value, end.value, retained))
                    return tuple(results)
                size = w.value * h.value * 4
                assert size <= 6000 * 6000 * 4
                results.append((w.value, h.value, end.value,
                                C.string_at(pixels, size)))
                previous = (pixels.value, size)
        finally:
            self.close(decoder)

    def capture(self, data, exact, transformed):
        buf = C.create_string_buffer(data)
        pixels, w, h, flags, passes = PTR(), INT(), INT(), UINT(), SIZE()
        status = self.mapped[exact](
            buf, len(data), C.byref(self.transform) if transformed else None,
            C.byref(pixels), C.byref(w), C.byref(h), C.byref(flags),
            C.byref(passes), self.alloc)
        try:
            return (status, w.value, h.value, flags.value, passes.value,
                    C.string_at(pixels, w.value * h.value * 4) if pixels else None)
        finally:
            self.free(self.alloc, pixels)


def packet(body, width=8, height=13, scale=1, p2=1, c1=False):
    return ((b'\x90' if c1 else b'\x1bP') + f'0;{p2}q'.encode()
            + f'"{scale};1;{width};{height}'.encode() + body
            + (b'\x9c' if c1 else b'\x1b\\'))


def specimens(rng, count):
    red = b'#0;2;100;0;0'
    blue = b'#1;2;0;0;100'
    base = packet(red + b'@')
    # The reference clears before P3 overflows. The packet decoder must keep
    # the valid frame, while preserving the error and terminal iterator state.
    yield base + b'\x1bP0;0;2147483647q"1;1;8;13#0@\x1b\\'
    # Individual syntax boundaries, including zero/omitted numeric fields,
    # wide registers, partial multiplied rows and overwritten undefined paint.
    bodies = [b'', b'?', b'@', b'#0@', red, red + b'~', red + b'!0@',
              red + b'!1@', red + b'!65535?', red + b'!65536?',
              red + b'!2147483647?', red + b'!2147483648?',
              red + b'!@', red + b'!;@', red + b'!1;@', red + b'!1',
              red + b'!1#0@', red + b'!1-', red + b'!1"1;1;8;13@',
              red + b'#@', red + b'#;2;100;0;0@', b'#0;1;360;100;100@',
              b'#65535;2;100;0;0@', b'#65536@', b'#0;3;0;0;0@',
              b'#0;2;2147483647;0;0@', b'#0;2;2147483648;0;0@',
              red + b'#0;2;0;0@', red + b'#0;2;0;0;0;0@',
              blue + b'#0@$#1@', b'#0@' + red + b'$',
              red + b'@#0;2;0;0;100$', red + b'?@' + blue,
              red + b'!9@', red + b'--A', red + b'--@',
              red + b'--!65535?', red + b'"2;1;8;13@',
              red + b'@"2;1;8;13$A', red + b'$"2;1;8;13A',
              red + b'@"1;1;8;13$B', red + b'"3;1;8;13o',
              red + b'"3;1;8;13_']
    bodies += [red + c + b'@' for c in
               (b'\x00', b'\x18', b'\x1a', b'\x90', b'\x1bP',
                b' ', b'\n', b'\x7f', b'\xff', b':', b'=', b'>')]
    for body in bodies:
        for c1 in (False, True):
            for scale in (1, 2, 3):
                p = packet(body, scale=scale, c1=c1)
                yield p
                yield base + p
    for header in (b'q', b'0q', b'0;0q', b'0;1q', b'0;2q', b'0;5q',
                   b';1q', b'0;1;2147483647q', b'0;1;2147483648q',
                   b'0;1;1;1q', b'?0;1q'):
        for raster in (b'', b'"1;1;8;13', b'"1;1;0;13', b'"1;1;6001;1',
                       b'"1;1;8;13;1', b'"2147483647;1;8;13'):
            yield b'\x1bP' + header + raster + red + b'@\x1b\\'
    for prefix in (b'', b'$', b'-', b'--', b'?', b'@', b'!0?'):
        for scale in (1, 2):
            yield (b'\x1bP0;1q' + prefix
                   + f'"{scale};1;8;13'.encode() + red + b'@\x1b\\')
    for head_scale, later_scale in ((4, 1), (1, 4), (2, 3), (3, 2)):
        first = packet(red + b'@', scale=head_scale)
        second = packet(red + f'"{later_scale};1;8;13'.encode() + b'@-@',
                        scale=head_scale)
        yield first + second
    for bands in (596, 597, 598, 600):
        yield packet(red + b'-' * bands, 1, 1, 600000)
    for suffix in (b' ', b'\x1b', b'\x1b8', b'\x1b7', b'\nX',
                   b'\x1b[H', b'\x1bP0;1q"1;1;8;13', b''):
        yield base + suffix
        yield b'\x1b7' + base + b'\x1b8' + base + suffix
    for _ in range(count):
        width, height = rng.randint(1, 20), rng.randint(1, 25)
        chunks = []
        anchored = rng.choice((b'', b'\x1b7', b'\x1b[H'))
        for frame in range(rng.randint(1, 4)):
            scale = rng.randint(1, 4)
            body = red + blue if rng.randrange(4) else b''
            for _ in range(rng.randint(0, 24)):
                kind = rng.randrange(10)
                if kind < 4:
                    body += bytes(rng.choices(range(63, 127),
                                               k=rng.randint(1, width)))
                elif kind == 4:
                    body += rng.choice((b'$', b'-', b'?', b'!0?'))
                elif kind == 5:
                    body += f'!{rng.randint(0, width + 1)}'.encode()
                    body += bytes([rng.randint(63, 126)])
                elif kind == 6:
                    body += f'#{rng.choice((0, 1, 2, 255, 256, 65535))}'.encode()
                elif kind == 7:
                    body += rng.choice((red, blue, b'#2;1;120;50;100'))
                elif kind == 8:
                    body += f'"{rng.randint(0, 5)};1;{width};{height}'.encode()
                else:
                    body += rng.choice(bodies)
            prefix = b'\x1b8' if anchored == b'\x1b7' and frame else anchored
            chunks.append(prefix + packet(body, width, height, scale,
                                          rng.choice((0, 1, 1, 1)),
                                          bool(rng.randrange(2))))
        data = b''.join(chunks)
        if rng.randrange(4) == 0 and data:
            pos = rng.randrange(len(data))
            data = data[:pos] + bytes([rng.randrange(256)]) + data[pos + 1:]
        yield data
    # Valid dense/sparse captures give success paths and worker partitioning
    # meaningful coverage instead of measuring only malformed rejection.
    for _ in range(count):
        width, height = rng.randint(1, 32), rng.randint(1, 32)
        chunks = []
        for frame in range(rng.randint(1, 4)):
            scale = rng.randint(1, 4)
            body = red + blue
            for band in range((height + 6 * scale - 1) // (6 * scale)):
                rows = min(6, (height - band * 6 * scale + scale - 1) // scale)
                body += b'#0' + bytes(63 + rng.randrange(1 << rows)
                                     for _ in range(width))
                body += b'$#1' + bytes(63 + rng.randrange(1 << rows)
                                      for _ in range(width))
                if frame % 2:
                    body += b'$' + red + b'@'
                body += b'-'
            chunks.append(packet(body, width, height, scale,
                                 rng.choice((0, 1, 1))))
        yield b''.join(chunks)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('library')
    parser.add_argument('--cases', type=int, default=1000)
    parser.add_argument('--seed', type=int, default=682340)
    args = parser.parse_args()
    decoder = Decoder(args.library)
    cases = list(specimens(random.Random(args.seed), args.cases))
    successes = [0, 0]
    atomic_differences = 0
    try:
        for number, data in enumerate(cases):
            for animation in (0, 1):
                decoder.threads(b'1')
                expected = decoder.frames(data, animation, True)
                successes[animation] += len(expected) > 1
                if len(expected) > 1 and expected[-1][0] != 0x1000:
                    # Only the canvas snapshot changes: status, dimensions,
                    # offsets, output pointer and the retry still match.
                    previous = expected[-2][-1]
                    atomic_differences += expected[-1][-1] != previous
                    expected = expected[:-1] + (
                        expected[-1][:-1] + (previous,),)
                for threads in (b'1', b'4'):
                    decoder.threads(threads)
                    actual = decoder.frames(data, animation, False)
                    if actual != expected:
                        raise AssertionError(('iterator', animation, threads,
                                              expected, actual))
            transformed = number % 2
            decoder.threads(b'1')
            expected = decoder.capture(data, True, transformed)
            for threads in (b'1', b'4'):
                decoder.threads(threads)
                actual = decoder.capture(data, False, transformed)
                if actual != expected:
                    raise AssertionError(('mapped', threads, expected, actual))
    except AssertionError as error:
        print(f'FAIL seed={args.seed} case={number} data={data!r}')
        print(error)
        return 1
    finally:
        decoder.unref(decoder.alloc)
    print(f'PASS seed={args.seed}: {len(cases)} captures, 1/4 threads, '
          f'animation/multi-pass, mapped colors, flags and iterator state; '
          f'captures with frames={successes}; '
          f'expected atomic canvas differences={atomic_differences}')
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
