#!/usr/bin/env python3
"""Generate PNG sample oracles with the same declared source interpretation.

CMS-off fixtures treat stored RGB as sRGB even if the PNG declares gAMA=1.
Assign sRGB without changing samples before linear alpha composition. Merely
asking ImageMagick for -colorspace RGB would adopt the file's gamma instead.
Reference decoding and arithmetic are independent of libsixel.
"""
import argparse
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[4]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    rows = json.loads(Path(__file__).with_name('sample-references.json').read_text())
    for row in rows:
        command = ['magick', str(ROOT / row['source']), '-set', 'colorspace',
                   'sRGB', '-colorspace', 'RGB', '-background', row.get('background', '#000'),
                   '-alpha', 'remove', '-alpha', 'off', '-colorspace', 'sRGB']
        if row['crop']:
            command += ['-crop', '16x16+8+8', '+repage']
        data = subprocess.check_output(command + ['-depth', '16', 'PPM:-'])
        path = ROOT / row['output']
        if args.check:
            assert path.read_bytes() == data, path
        else:
            path.write_bytes(data)


if __name__ == '__main__':
    main()
