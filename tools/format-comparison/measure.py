#!/usr/bin/env python3
"""Measure native, in-memory codecs and save all samples before plotting.

Requires Pillow, numpy, an isolated libsixel build and the codecs.c adapter.
Input/output copies and wrapper allocation are included; disk I/O and LSQA
are excluded. RGB generations never silently retain an indexed image.
"""
import argparse
import datetime
import ctypes as C
import hashlib
import io
import json
import os
from pathlib import Path
import platform
import statistics
import subprocess
import time

import numpy as np
from PIL import Image, ImageDraw, features

P = C.c_void_p
I = C.c_int
Z = C.c_size_t


class Options(C.Structure):
    _fields_ = [('flags', C.c_uint), ('format', I), ('bg', C.c_ubyte * 3)]


class Result(C.Structure):
    _fields_ = [('pixels', P), ('width', I), ('height', I), ('format', I),
                ('stride', I), ('flags', C.c_uint)]


def sha(data):
    return hashlib.sha256(data).hexdigest()


def buffer(data):
    return C.create_string_buffer(data)


class Codecs:
    def __init__(self, adapter, library):
        self.a = C.CDLL(str(adapter))
        self.s = C.CDLL(str(library))
        if C.cast(self.a.sixel_decode_pixels, P).value != C.cast(self.s.sixel_decode_pixels, P).value:
            raise RuntimeError("adapter resolved a different libsixel library")
        self.a.study_free.argtypes = [P]
        self.a.study_sixel_encode.argtypes = [P, I, I, P, I, I, I, C.POINTER(P), C.POINTER(Z)]
        self.a.study_webp_encode.argtypes = [P, I, I, I, I, I, C.POINTER(P), C.POINTER(Z)]
        self.a.study_webp_decode.argtypes = [P, Z, I, C.POINTER(P), C.POINTER(I), C.POINTER(I)]
        self.s.sixel_decode_pixels.argtypes = [P, Z, C.POINTER(Options), C.POINTER(Result), P]
        self.s.sixel_decode_raw.argtypes = [P, I, C.POINTER(P), C.POINTER(I), C.POINTER(I), C.POINTER(P), C.POINTER(I), P]
        self.free = C.CDLL(None).free
        self.free.argtypes = [P]

    def encode(self, image, codec, threads):
        if codec.startswith(('SIXEL', 'WebP')):
            pixels = buffer(image.tobytes())
            data, size = P(), Z()
            palette = None
            if image.mode == 'P':
                palette = buffer(bytes(image.getpalette()).ljust(768, b'\0'))
            if codec.startswith('SIXEL'):
                ok = self.a.study_sixel_encode(pixels, *image.size, palette,
                    256, threads, codec.endswith('none'), C.byref(data), C.byref(size))
            else:
                quality = 80 if codec.endswith('lossless') else int(codec.split()[-1])
                ok = self.a.study_webp_encode(pixels, *image.size, quality,
                    codec.endswith('lossless'), threads, C.byref(data), C.byref(size))
            if not ok:
                raise RuntimeError(f'encode failed: {codec}')
            try:
                return C.string_at(data, size.value)
            finally:
                self.a.study_free(data)
        output = io.BytesIO()
        if codec.startswith('JPEG'):
            image.save(output, 'JPEG', quality=int(codec.split()[-1]), subsampling=2, optimize=False)
        elif codec == 'PNG':
            image.save(output, 'PNG', compress_level=6)
        else:
            if image.mode != 'P':
                image = image.quantize(colors=256, method=Image.Quantize.MEDIANCUT)
            image.save(output, 'GIF', optimize=False)
        return output.getvalue()

    def decode(self, data, codec, threads, indexed=False):
        if codec.startswith('SIXEL'):
            os.environ['SIXEL_THREADS'] = str(threads)
            if indexed:
                pixels, palette, w, h, colors = P(), P(), I(), I(), I()
                status = self.s.sixel_decode_raw(buffer(data), len(data), C.byref(pixels), C.byref(w), C.byref(h), C.byref(palette), C.byref(colors), None)
                if status & 0x1000:
                    raise RuntimeError(f'indexed decode failed {status}')
                try:
                    image = Image.frombytes('P', (w.value, h.value), C.string_at(pixels, w.value * h.value))
                    assert max(image.tobytes()) < colors.value
                    image.putpalette(C.string_at(palette, colors.value * 3).ljust(768, b'\0'))
                    return image
                finally:
                    self.free(pixels)
                    self.free(palette)
            result = Result()
            status = self.s.sixel_decode_pixels(buffer(data), len(data), C.byref(Options(0, 3)), C.byref(result), None)
            if status & 0x1000:
                raise RuntimeError(f'decode failed {status}')
            try:
                return Image.frombytes('RGB', (result.width, result.height), C.string_at(result.pixels, result.stride * result.height), 'raw', 'RGB', result.stride)
            finally:
                self.free(result.pixels)
        if codec.startswith('WebP'):
            pixels, w, h = P(), I(), I()
            ok = self.a.study_webp_decode(buffer(data), len(data), threads, C.byref(pixels), C.byref(w), C.byref(h))
            if not ok:
                raise RuntimeError('WebP decode failed')
            try:
                return Image.frombytes('RGB', (w.value, h.value), C.string_at(pixels, w.value * h.value * 3))
            finally:
                self.a.study_free(pixels)
        image = Image.open(io.BytesIO(data))
        image.load()
        return image.copy() if indexed else image.convert('RGB')


def fixtures(source, out):
    images = {'photo': Image.open(source / 'images/snake.png').convert('RGB').resize((768, 512), Image.Resampling.LANCZOS)}
    y, x = np.mgrid[:512, :768]
    rgb = np.stack((x * 255 / 767, y * 255 / 511, (np.sin(x / 65) * .5 + .5) * 255), axis=-1).astype('uint8')
    images['gradient'] = Image.fromarray(rgb)
    image = Image.new('RGB', (768, 512), 'white')
    draw = ImageDraw.Draw(image)
    colors = ['#0072b2', '#e69f00', '#009e73', '#cc79a7', '#202020']
    for n in range(30):
        xx, yy = 20 + (n % 6) * 124, 20 + (n // 6) * 96
        draw.rectangle((xx, yy, xx + 105, yy + 65), fill=colors[n % 5])
        draw.text((xx + 5, yy + 72), f'Node {n:02d} / RGB', fill='black')
    images['diagram'] = image
    for name, image in images.items():
        image.save(out / f'{name}.png')
    return images


def sampled(fn, runs):
    samples = []
    for n in range(runs + 2):
        start = time.perf_counter_ns()
        result = fn()
        elapsed = (time.perf_counter_ns() - start) / 1e6
        if n >= 2:
            samples.append(elapsed)
    return result, samples


def quality(reference, image, lsqa, work):
    path = work / 'decoded.png'
    image.convert('RGB').save(path)
    env = os.environ.copy()
    env.update(LSQA_VERBOSE='0', LSQA_PREFIX=str(work / 'lsqa'), SIXEL_THREADS='1')
    p = subprocess.run([str(lsqa), str(reference), str(path)], capture_output=True, env=env, check=True)
    q = json.loads(p.stdout)
    q = q.get('quality', q)
    return {'ms_ssim': float(q['MS-SSIM']), 'delta_e': float(q['Δ E00_mean'])}


def record(image, codec, threads, encoded, decoded, enc, dec, reference, lsqa, work):
    assert decoded.size == image.size
    rgb = decoded.convert('RGB').tobytes()
    if codec.startswith('SIXEL'):
        assert encoded.startswith(b'\x1bP') and encoded.endswith(b'\x1b\\')
        assert all(v < 128 for v in encoded)
    return dict(codec=codec, threads=threads, encode_ms=enc, decode_ms=dec,
        bytes=len(encoded), transport_bytes=len(encoded) if codec.startswith('SIXEL') else 4 * ((len(encoded) + 2) // 3),
        rgb_sha256=sha(rgb), stream_sha256=sha(encoded), exact_input=rgb == image.convert('RGB').tobytes(),
        **quality(reference, decoded, lsqa, work))


def main():
    p = argparse.ArgumentParser()
    p.add_argument('--source', type=Path, required=True)
    p.add_argument('--adapter', type=Path, required=True)
    p.add_argument('--library', type=Path, required=True)
    p.add_argument('--output', type=Path, required=True)
    p.add_argument('--revision', required=True)
    p.add_argument('--runs', type=int, default=7)
    p.add_argument('--generations', type=int, default=8)
    args = p.parse_args()
    for key in list(os.environ):
        if key.startswith(('SIXEL_', 'LSQA_')):
            del os.environ[key]
    os.environ.update(SIXEL_GPU_POLICY='off', SIXEL_PALETTE_KMEANS_SEED='1')
    out = args.output.resolve()
    out.mkdir(parents=True, exist_ok=True)
    import tempfile
    with tempfile.TemporaryDirectory(prefix='format-quality-') as tmp:
        work = Path(tmp)
        images = fixtures(args.source, out)
        codecs = Codecs(args.adapter.resolve(), args.library.resolve())
        lsqa = args.source / 'assessment/lsqa'
        settings = ['SIXEL default', 'SIXEL none', 'GIF', 'PNG', 'JPEG 60', 'JPEG 80', 'JPEG 95', 'WebP 60', 'WebP 80', 'WebP 95', 'WebP lossless']
        rows = []
        payload = {'metadata': {'revision': args.revision, 'platform': platform.platform(),
            'cpu_count': os.cpu_count(), 'python': platform.python_version(),
            'date_utc': datetime.datetime.now(datetime.timezone.utc).isoformat(),
            'cpu': subprocess.check_output(['sysctl', '-n', 'machdep.cpu.brand_string'], text=True).strip() if platform.system() == 'Darwin' else platform.processor(),
            'compiler': subprocess.check_output(['cc', '--version'], text=True).splitlines()[0],
            'webp': subprocess.check_output(['pkg-config', '--modversion', 'libwebp'], text=True).strip(),
            'build_config': (args.source / 'config.log').read_text().splitlines()[:10],
            'adapter_links': subprocess.check_output(['otool', '-L', str(args.adapter)], text=True) if platform.system() == 'Darwin' else subprocess.check_output(['ldd', str(args.adapter)], text=True),
            'measure_script_sha256': sha(Path(__file__).read_bytes()),
            'adapter_source_sha256': sha(Path(__file__).with_name('codecs.c').read_bytes()),
            'pillow': Image.__version__, 'jpeg': features.version('jpg'), 'zlib': features.version('zlib'),
            'adapter_sha256': sha(args.adapter.read_bytes()), 'library_sha256': sha(args.library.read_bytes()),
            'runs': args.runs, 'warmups': 2, 'generations': args.generations,
            'inputs': {k: sha((out / f'{k}.png').read_bytes()) for k in images}}, 'rows': rows}
        def save():
            (out / 'results.json').write_text(json.dumps(payload, indent=2) + '\n')
        for name, image in images.items():
            reference = out / f'{name}.png'
            # Alternate traversal between fixtures; every decoder budget uses
            # the same one-thread stream, independently of encoder changes.
            for codec in settings[::(-1 if name == 'gradient' else 1)]:
                fixed = codecs.encode(image, codec, 1)
                expected = codecs.decode(fixed, codec, 1).tobytes()
                for threads in [1, 2, 4, 8]:
                    encoded, enc = sampled(lambda: codecs.encode(image, codec, threads), args.runs)
                    decoded, dec = sampled(lambda: codecs.decode(fixed, codec, threads), args.runs)
                    assert decoded.tobytes() == expected
                    encoded_decoded = codecs.decode(encoded, codec, 1)
                    row = record(image, codec, threads, encoded, encoded_decoded, enc, dec, reference, lsqa, work)
                    row.update(image=name, experiment='scaling', decoder_stream_sha256=sha(fixed))
                    rows.append(row)
                    save()
                print(name, codec, 'scaling complete', flush=True)
            for codec in ['SIXEL default', 'GIF', 'PNG', 'JPEG 80', 'WebP 80', 'WebP lossless']:
                modes = ['RGB', 'indexed'] if codec in ['SIXEL default', 'GIF'] else ['RGB']
                for mode in modes:
                    current = image
                    first_rgb = None
                    for generation in range(1, args.generations + 1):
                        encoded, enc = sampled(lambda: codecs.encode(current, codec, 1), args.runs)
                        decoded, dec = sampled(lambda: codecs.decode(encoded, codec, 1, mode == 'indexed'), args.runs)
                        row = record(current, codec, 1, encoded, decoded, enc, dec, reference, lsqa, work)
                        rgb = decoded.convert('RGB').tobytes()
                        first_rgb = rgb if first_rgb is None else first_rgb
                        row.update(image=name, experiment='generation', mode=mode, generation=generation, exact_generation_one=rgb == first_rgb)
                        rows.append(row)
                        current = decoded if mode == 'indexed' else decoded.convert('RGB')
                        save()
                    print(name, codec, mode, 'generations complete', flush=True)


if __name__ == '__main__':
    main()
