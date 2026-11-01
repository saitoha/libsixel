#!/usr/bin/env python3
"""Regenerate the unequal-duration VIF-P video fixture after a native build.

Both APNG frames repeat the decoded 64x64 snake SIXEL without color conversion.
The target is identity for 0.1 s, then flat black for 0.3 s. Thus VIF-P is
exactly 1 then 0, giving mean 0.25, minimum 0 and maximum 1 independently of
production aggregation. This generator is not run by the TAP tests.
"""
import json
from pathlib import Path
import struct
import subprocess
import zlib


def chunk(kind, data):
    return (struct.pack('>I', len(data)) + kind + data
            + struct.pack('>I', zlib.crc32(kind + data)))


def main():
    destination = Path(__file__).resolve().parent
    root = destination.parents[2]
    source = root / 'tests/data/inputs/snake_64.six'
    png = subprocess.check_output([str(root / 'converters/sixel2png'),
                                   '-i', str(source)])
    assert png[:8] == b'\x89PNG\r\n\x1a\n'
    chunks = []
    offset = 8
    while offset < len(png):
        size, = struct.unpack('>I', png[offset:offset + 4])
        chunks.append((png[offset + 4:offset + 8],
                       png[offset + 8:offset + 8 + size]))
        offset += size + 12
    width, height = struct.unpack('>II', chunks[0][1][:8])
    assert (width, height) == (64, 64)
    prefix = b''.join(chunk(k, d) for k, d in chunks
                      if k not in (b'IDAT', b'IEND'))
    compressed = b''.join(d for k, d in chunks if k == b'IDAT')
    first = struct.pack('>IIIIIHHBB', 0, width, height, 0, 0, 1, 10, 0, 0)
    second = struct.pack('>IIIIIHHBB', 1, width, height, 0, 0, 3, 10, 0, 0)
    reference = (png[:8] + prefix + chunk(b'acTL', struct.pack('>II', 2, 0))
                 + chunk(b'fcTL', first) + chunk(b'IDAT', compressed)
                 + chunk(b'fcTL', second)
                 + chunk(b'fdAT', struct.pack('>I', 2) + compressed)
                 + chunk(b'IEND', b''))
    identity = source.read_bytes()
    black = (b'\x1bP0;0;0q"1;1;64;64#0;2;0;0;0#0'
             + b'-'.join([b'!64~'] * 10 + [b'!64N']) + b'\x1b\\')
    timeline = [dict(frame=0, byte_end=len(identity), pts=0, duration=.1),
                dict(frame=1, byte_end=len(identity + black),
                     pts=.1, duration=.3)]
    (destination / 'vif-p-video.png').write_bytes(reference)
    (destination / 'vif-p-video.six').write_bytes(identity + black)
    (destination / 'vif-p-video.jsonl').write_text(
        ''.join(json.dumps(record) + '\n' for record in timeline))


if __name__ == '__main__':
    main()
