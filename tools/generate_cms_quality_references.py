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
import zlib

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


def png_reference(path, isolated_chrm=False):
    chunks = png_chunks(path)
    with Image.open(path) as image:
        assert image.mode in ('L', 'P', 'RGB') and image.size == (64, 16)
        gray = image.mode == 'L'
        rgb = np.asarray(image.convert('RGB'), dtype=np.float64) / 255
    # Follow the documented container priority, including its three-chunk
    # sRGB exception. Evaluate the selected declaration with source equations.
    if all(tag in chunks for tag in (b'iCCP', b'sRGB', b'cHRM')):
        return rgb
    if b'iCCP' in chunks:
        data = chunks[b'iCCP']
        offset = data.index(0) + 2
        return icc_reference(rgb, zlib.decompress(data[offset:]))
    if b'sRGB' in chunks:
        return rgb
    if b'gAMA' in chunks:
        gamma = struct.unpack('>I', chunks[b'gAMA'])[0] / 100000
        linear = rgb ** (1 / gamma)
    elif isolated_chrm and b'cHRM' in chunks and not gray:
        # The libpng path adopts valid isolated cHRM with its default sRGB
        # transfer; the legacy builtin fallback requires an explicit gAMA.
        linear = np.where(rgb <= .04045, rgb / 12.92, ((rgb + .055) / 1.055) ** 2.4)
    else:
        return rgb
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


def tiff_rgb_lab_reference(path, managed):
    with Image.open(path) as image:
        assert image.mode in ('RGB', 'LAB')
        samples = np.asarray(image, dtype=np.float64)
        profile = image.info.get('icc_profile')
        if image.mode == 'RGB':
            rgb = samples / 255
            return icc_reference(rgb, profile) if managed and profile else rgb
        # TIFF CIELAB8 chroma is signed physical a*/b*, not ICC offset binary.
        # These specimens deliberately carry an incompatible RGB ICC when
        # tagged: the Lab fallback must ignore that profile and retain D50.
        assert image.tag_v2[262] == 8 and image.tag_v2[258] == (8, 8, 8)
        assert profile is None or profile[16:20] == b'RGB '
        l = samples[..., 0] * 100 / 255
        ab = np.where(samples[..., 1:] < 128, samples[..., 1:], samples[..., 1:] - 256)
        fy = (l + 16) / 116
        f = np.stack([fy + ab[..., 0] / 500, fy, fy - ab[..., 1] / 200], axis=-1)
        white = np.array([.9642, 1, .8249])
        pcs = np.where(f > 6 / 29, f ** 3, 3 * (6 / 29) ** 2 * (f - 4 / 29)) * white
        transform = np.linalg.solve(SRGB, adapt(white, xyz(D65)))
        return encode_srgb(pcs @ transform.T)


def icc_reference(rgb, profile):
    assert profile[16:24] in (b'RGB XYZ ', b'GRAYXYZ ')
    tags = {}
    for i in range(struct.unpack_from('>I', profile, 128)[0]):
        key, offset, length = struct.unpack_from('>4sII', profile, 132 + 12 * i)
        tags[key] = profile[offset:offset + length]
    if profile[16:20] == b'GRAY':
        tag = tags[b'kTRC']
        assert tag[:4] == b'curv' and struct.unpack_from('>I', tag, 8)[0] == 1
        gamma = struct.unpack_from('>H', tag, 12)[0] / 256
        # A neutral PCS gray remains neutral after white-point adaptation.
        return encode_srgb(rgb ** gamma)
    columns, linear = [], []
    for channel, axis in enumerate((b'r', b'g', b'b')):
        tag = tags[axis + b'XYZ']
        assert tag[:4] == b'XYZ '
        columns.append(np.array(struct.unpack_from('>3i', tag, 8)) / 65536)
        tag = tags[axis + b'TRC']
        value = rgb[..., channel]
        if tag[:4] == b'curv':
            assert struct.unpack_from('>I', tag, 8)[0] == 1
            gamma = struct.unpack_from('>H', tag, 12)[0] / 256
            linear.append(value ** gamma)
        else:
            # ICC parametricCurveType 0/1/2, evaluated from its s15Fixed16
            # parameters rather than a production sampled lookup table.
            assert tag[:4] == b'para'
            kind = struct.unpack_from('>H', tag, 8)[0]
            assert kind in (0, 1, 2)
            count = (1, 3, 4)[kind]
            params = np.array(struct.unpack_from('>' + 'i' * count, tag, 12)) / 65536
            if kind == 0:
                linear.append(value ** params[0])
            else:
                gamma, a, b = params[:3]
                c = params[3] if kind == 2 else 0
                linear.append(np.maximum(a * value + b, 0) ** gamma + c)
    # Stored ICC colorants already inhabit PCS D50; do not adapt them twice.
    transform = np.linalg.solve(
        SRGB, adapt(np.array([.9642, 1, .8249]), xyz(D65)) @ np.stack(columns, axis=1))
    return encode_srgb(np.stack(linear, axis=-1) @ transform.T)


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
    for source in sorted((BASE / 'input/png').glob('*/*.png')):
        record(source)
        target = BASE / 'reference/png' / source.parent.name
        save(target / (source.stem + '.ppm'), png_reference(source))
    for model in ('idx', 'rgb'):
        source = BASE / f'input/png/{model}/img_{model}_icc0_srgb0_chrm1_gama0.png'
        target = BASE / 'reference/png/libpng' / model
        if not args.check:
            target.mkdir(parents=True, exist_ok=True)
        save(target / (source.stem + '.ppm'), png_reference(source, True))
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
    assert len(sources) == 64 and profile is not None
    save(BASE / 'reference/tiff/palette/colormap-unmanaged.ppm', baseline)
    save(BASE / 'reference/tiff/palette/colormap-icc-srgb.ppm', icc_reference(baseline, profile))
    for model in ('rgb', 'lab'):
        baseline = None
        for source in sorted((BASE / 'input/tiff' / model).glob('*.tiff')):
            record(source)
            with Image.open(source) as image:
                samples = np.asarray(image)
                if baseline is None:
                    baseline = samples
                assert np.array_equal(samples, baseline), source
        source = BASE / f'input/tiff/{model}/img_tiff_{model}_icc1_wp0_pc0_trc0.tiff'
        target = BASE / 'reference/tiff' / model
        save(target / 'samples-unmanaged.ppm', tiff_rgb_lab_reference(source, False))
        save(target / 'samples-managed.ppm', tiff_rgb_lab_reference(source, True))
    source = BASE / 'input/custom/rgb_parametric_012.tiff'
    record(source)
    save(BASE / 'reference/custom/rgb_parametric_012_tiff_analytic.ppm',
         tiff_rgb_lab_reference(source, True))
    source = BASE / 'input/custom/rgb_parametric_012.png'
    record(source)
    with Image.open(source) as image:
        rgb = np.asarray(image.convert('RGB'), dtype=np.float64) / 255
        reference = icc_reference(rgb, image.info['icc_profile'])
    save(BASE / 'reference/custom/rgb_parametric_012_analytic.ppm', reference)
    manifest = dict(generator=Path(__file__).name, sources=sources, references=outputs)
    path = BASE / 'reference/analytic-quality.json'
    if args.check:
        assert json.loads(path.read_text()) == manifest
    else:
        path.write_text(json.dumps(manifest, indent=2) + '\n')


if __name__ == '__main__':
    main()
