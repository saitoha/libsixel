#!/usr/bin/env python3
"""Prepare independent loader references at the requested encoder dimensions.

The matching tests resize decoded source samples before palette construction.
References are resized offline from their existing external-renderer oracles,
never from the encoder output being tested. Integer nearest replication makes
coordinate alignment explicit without adding smoothing to the comparison.
"""
import argparse
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[4]
MANIFEST = Path(__file__).with_name('references.json')


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    for row in json.loads(MANIFEST.read_text()):
        assert min(row['width'], row['height']) >= 176
        data = subprocess.check_output(['magick', str(ROOT / row['reference']),
            '-sample', f"{row['width']}x{row['height']}!", '-depth', str(row.get('depth', 8)), 'PPM:-'])
        path = ROOT / row['output']
        if args.check:
            assert path.read_bytes() == data, path
        else:
            path.write_bytes(data)


if __name__ == '__main__':
    main()
