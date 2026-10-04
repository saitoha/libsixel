#!/usr/bin/env python3
"""Generate unquantized sRGB references for selected CMS quality fixtures.

Offline only; requires Pillow and NumPy. Neither libsixel nor a CMM is used.
Pillow only unpacks source pixels/tags; the color equations below are independent.
Use --check to compare committed samples with a one-UINT16-code rounding allowance.
"""

import argparse
import hashlib
import json
from pathlib import Path
import struct

import numpy as np
from PIL import Image

from generate_png_cms_matrix import png_chunks

ROOT = Path(__file__).resolve().parents[1]
BASE = ROOT / "tests/data/colormgmt"
BRADFORD = np.array([[.8951, .2664, -.1614],
                     [-.7502, 1.7135, .0367],
                     [.0389, -.0685, 1.0296]])
D65 = (.3127, .3290)


def xyz(xy):
    x, y = xy
    return np.array([x / y, 1, (1 - x - y) / y])


def matrix(coords):
    columns = np.stack([xyz(coords[i:i + 2]) for i in (2, 4, 6)], axis=1)
    return columns @ np.diag(np.linalg.solve(columns, xyz(coords[:2])))


SRGB = matrix([*D65, .64, .33, .30, .60, .15, .06])


def adapt(source_white, destination_white):
    ratio = (BRADFORD @ destination_white) / (BRADFORD @ source_white)
    return np.linalg.solve(BRADFORD, np.diag(ratio) @ BRADFORD)


def encode_srgb(linear):
    # Do not clip matrix intermediates. Only the final display reference is
    # bounded, since these tests compare ordinary sRGB SIXEL output.
    return np.where(linear <= .0031308, 12.92 * linear,
                    1.055 * np.maximum(linear, 0) ** (1 / 2.4) - .055)


def png_reference(path):
    chunks = png_chunks(path)
    assert b'iCCP' not in chunks and b'sRGB' not in chunks
    assert struct.unpack('>I', chunks[b'gAMA'])[0] == 25000
    with Image.open(path) as image:
        assert image.mode in ('L', 'RGB') and image.size == (64, 16)
        gray = image.mode == 'L'
        rgb = np.asarray(image.convert('RGB'), dtype=np.float64) / 255
    # PNG gAMA stores the encoding exponent; decode with its reciprocal.
    linear = rgb ** 4
    if b'cHRM' in chunks and not gray:
        coords = np.array(struct.unpack('>8I', chunks[b'cHRM'])) / 100000
        transform = np.linalg.solve(
            SRGB, adapt(xyz(coords[:2]), xyz(D65)) @ matrix(coords))
        linear = linear @ transform.T
    return encode_srgb(linear)


def tiff_source(path):
    with Image.open(path) as image:
        assert image.mode == 'P' and image.size == (64, 64)
        assert image.tag_v2[262] == 3 and image.tag_v2[258] == (8,)
        indices = np.asarray(image)
        table = np.asarray(image.tag_v2[320], dtype=np.float64).reshape(3, -1).T
        return table[indices] / 65535, image.info.get('icc_profile')


def icc_reference(rgb, profile):
    assert profile[16:24] == b'RGB XYZ '
    tags = {}
    for i in range(struct.unpack_from('>I', profile, 128)[0]):
        key, offset, length = struct.unpack_from('>4sII', profile, 132 + 12 * i)
        tags[key] = profile[offset:offset + length]
    columns, exponents = [], []
    for axis in (b'r', b'g', b'b'):
        tag = tags[axis + b'XYZ']
        assert tag[:4] == b'XYZ '
        columns.append(np.array(struct.unpack_from('>3i', tag, 8)) / 65536)
        tag = tags[axis + b'TRC']
        assert tag[:4] == b'curv' and struct.unpack_from('>I', tag, 8)[0] == 1
        exponents.append(struct.unpack_from('>H', tag, 12)[0] / 256)
    # Stored ICC colorants already inhabit PCS D50; do not adapt them twice.
    transform = np.linalg.solve(
        SRGB, adapt(np.array([.9642, 1, .8249]), xyz(D65)) @ np.stack(columns, axis=1))
    return encode_srgb((rgb ** np.array(exponents)) @ transform.T)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    sources, outputs = {}, {}

    def record(path):
        sources[str(path.relative_to(BASE))] = hashlib.sha256(path.read_bytes()).hexdigest()

    def save(path, rgb):
        samples = np.floor(np.clip(rgb, 0, 1) * 65535 + .5).astype('>u2')
        header = f'P6\n{rgb.shape[1]} {rgb.shape[0]}\n65535\n'.encode()
        data = header + samples.tobytes()
        if args.check:
            actual = path.read_bytes()
            assert actual.startswith(header) and len(actual) == len(data), path
            words = np.frombuffer(actual[len(header):], dtype='>u2').astype(np.int64)
            assert np.max(np.abs(words - samples.reshape(-1).astype(np.int64))) <= 1, path
        else:
            path.write_bytes(data)
        outputs[str(path.relative_to(BASE))] = hashlib.sha256(path.read_bytes()).hexdigest()

    # Algebraic anchors do not depend on a decoded fixture or production output.
    assert np.max(np.abs(SRGB @ np.ones(3) - xyz(D65))) < 1e-14
    assert np.max(np.abs(adapt(xyz(D65), xyz(D65)) - np.eye(3))) < 1e-14
    assert abs(float(encode_srgb(np.array(.5 ** 4))) - .27730417690977277) < 1e-14
    for model in ('gray', 'rgb'):
        for chrm in (0, 1):
            stem = f'img_{model}_icc0_srgb0_chrm{chrm}_gama1'
            source = BASE / 'input/png' / model / (stem + '.png')
            record(source)
            save(BASE / 'reference/png' / model / (stem + '.ppm'), png_reference(source))
    baseline = None
    profile = None
    for source in sorted((BASE / 'input/tiff/palette').glob('*.tiff')):
        record(source)
        rgb, icc = tiff_source(source)
        if baseline is None:
            baseline = rgb
        assert np.array_equal(baseline, rgb), source
        if icc:
            if profile is None:
                profile = icc
            assert icc == profile, source
    assert len(sources) == 20 and profile is not None
    save(BASE / 'reference/tiff/palette/colormap-unmanaged.ppm', baseline)
    save(BASE / 'reference/tiff/palette/colormap-icc-srgb.ppm', icc_reference(baseline, profile))
    manifest = dict(generator=Path(__file__).name, sources=sources, references=outputs)
    path = BASE / 'reference/analytic-quality.json'
    if args.check:
        assert json.loads(path.read_text()) == manifest
    else:
        path.write_text(json.dumps(manifest, indent=2) + '\n')


if __name__ == '__main__':
    main()
