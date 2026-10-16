#!/usr/bin/env python3
"""Measure native, in-memory codecs and save all samples before plotting.

Requires Pillow (with AVIF support), pillow-heif, numpy, qoi, an isolated libsixel build and the codecs.c
adapter.
Input/output copies and wrapper allocation are included; disk I/O and LSQA
are excluded. Palette formats retain decoded indices between generations.
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
from PIL import AvifImagePlugin, Image, ImageCms, ImageDraw, ImageOps, features
import pillow_heif
import qoi

pillow_heif.register_heif_opener()

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
                    256, threads, codec.endswith('quality'), C.byref(data), C.byref(size))
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
        if codec == 'QOI':
            return qoi.encode(np.asarray(image.convert('RGB')))
        output = io.BytesIO()
        if codec.startswith('JPEG'):
            image.save(output, 'JPEG', quality=int(codec.split()[-1]), subsampling=2, optimize=False)
        elif codec == 'PNG':
            image.save(output, 'PNG', compress_level=6)
        elif codec.startswith('AVIF'):
            # libavif/aom receives the exact worker count.
            image.save(output, 'AVIF', quality=int(codec.split()[-1]), speed=6,
                       subsampling='4:2:0', max_threads=threads)
        elif codec.startswith('HEIC'):
            # libheif forwards x265:pools, the exact x265 worker count.
            pillow_heif.from_pillow(image).save(output,
                quality=int(codec.split()[-1]), chroma=420,
                enc_params={'x265:pools': str(threads)})
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
        if codec == 'QOI':
            return Image.fromarray(qoi.decode(data), 'RGB')
        # Both decoder thread counts are read when the image is opened.
        AvifImagePlugin.DEFAULT_MAX_THREADS = threads
        pillow_heif.options.DECODE_THREADS = threads
        image = Image.open(io.BytesIO(data))
        image.load()
        expected = {'AVIF': 'AVIF', 'HEIC': 'HEIF'}.get(codec.split()[0])
        if expected and image.format != expected:
            raise RuntimeError(f'{codec} opened as {image.format}')
        return image.copy() if indexed else image.convert('RGB')


def untagged_srgb(image):
    """Return (RGB image without metadata, converted profile name or None).

    Every codec treats RGB samples as sRGB, and LSQA would apply an embedded
    profile only to the reference. Convert a tagged photo to sRGB with its
    profile and drop the profile so that both sides share one interpretation.
    """
    icc = image.info.get('icc_profile')
    name = None
    if icc:
        source = ImageCms.ImageCmsProfile(io.BytesIO(icc))
        name = ImageCms.getProfileDescription(source).strip()
        image = ImageCms.profileToProfile(
            image, source, ImageCms.createProfile('sRGB'), outputMode='RGB',
            renderingIntent=ImageCms.Intent.RELATIVE_COLORIMETRIC)
    image = image.convert('RGB')
    image.info = {}
    return image, name


def photo_fixtures(directory, fit):
    """Load each source photo (PNG or JPEG) upright in sRGB and fit it inside
    `fit` (width, height) with Lanczos, preserving the aspect ratio and never
    enlarging; a 1:1 crop smaller than `fit` is therefore left untouched.
    """
    images, sources, profiles = {}, {}, {}
    paths = [p for p in directory.iterdir() if p.suffix.lower() in ('.png', '.jpg', '.jpeg')]
    for path in sorted(paths):
        image, profiles[path.stem] = untagged_srgb(ImageOps.exif_transpose(Image.open(path)))
        scale = min(1.0, fit[0] / image.width, fit[1] / image.height)
        size = (max(1, round(image.width * scale)), max(1, round(image.height * scale)))
        images[path.stem] = image.resize(size, Image.Resampling.LANCZOS)
        images[path.stem].info = {}
        sources[path.stem] = sha(path.read_bytes())
    return images, sources, profiles


def crop_fixtures(directory, names, sizes):
    """Center-crop each original at 1:1 to every size, without resampling.

    The offset follows libsixel-dataset's crop-640x480 procedure,
    ((W - w) // 2, (H - h) // 2) on the decoded pixels with no orientation
    change, so only the pixel count varies along the size axis.
    """
    images, sources = {}, {}
    for name in names:
        path = directory / f'{name}.jpg'
        image, _ = untagged_srgb(Image.open(path))
        sources[name] = sha(path.read_bytes())
        for w, h in sizes:
            x, y = (image.width - w) // 2, (image.height - h) // 2
            if x < 0 or y < 0:
                raise RuntimeError(f'{name} is smaller than {w}x{h}')
            images[name, (w, h)] = image.crop((x, y, x + w, y + h))
    return images, sources


def diagram_fixture():
    """Return the deterministic flat-color diagram with text."""
    image = Image.new('RGB', (768, 512), 'white')
    draw = ImageDraw.Draw(image)
    colors = ['#0072b2', '#e69f00', '#009e73', '#cc79a7', '#202020']
    for n in range(30):
        xx, yy = 20 + (n % 6) * 124, 20 + (n // 6) * 96
        draw.rectangle((xx, yy, xx + 105, yy + 65), fill=colors[n % 5])
        draw.text((xx + 5, yy + 72), f'Node {n:02d} / RGB', fill='black')
    return image


def sampled(fn, runs):
    samples = []
    for n in range(runs + 2):
        start = time.perf_counter_ns()
        result = fn()
        elapsed = (time.perf_counter_ns() - start) / 1e6
        if n >= 2:
            samples.append(elapsed)
    return result, samples


QUALITY_CACHE = {}


def quality(reference, image, lsqa, work):
    # Identical decoded pixels against the same reference score identically;
    # evaluate each pair once. LSQA is outside every timed region.
    key = (str(reference), sha(image.convert('RGB').tobytes()))
    if key in QUALITY_CACHE:
        return dict(QUALITY_CACHE[key])
    path = work / 'decoded.png'
    image.convert('RGB').save(path)
    env = os.environ.copy()
    env.update(LSQA_VERBOSE='0', LSQA_PREFIX=str(work / 'lsqa'), SIXEL_THREADS='1')
    p = subprocess.run([str(lsqa), str(reference), str(path)], capture_output=True, env=env, check=True)
    q = json.loads(p.stdout)
    q = q.get('quality', q)
    QUALITY_CACHE[key] = {'ms_ssim': float(q['MS-SSIM']), 'delta_e': float(q['Δ E00_mean'])}
    return dict(QUALITY_CACHE[key])


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
    p.add_argument('--photos', type=Path, required=True,
                   help='directory of source photographs (*.jpg)')
    p.add_argument('--generation-stride', type=int, default=5,
                   help='run the generation experiment on every Nth photograph')
    p.add_argument('--fit', default='1920x1080',
                   help='fit photographs inside WIDTHxHEIGHT (default: Full HD)')
    p.add_argument('--size-photos', type=Path,
                   help='directory of originals (*.jpg) for the size sweep')
    p.add_argument('--size-crops', default='160x120,320x240,640x480,1280x960',
                   help='comma-separated 1:1 center-crop sizes of the sweep')
    p.add_argument('--size-stride', type=int, default=10,
                   help='run the size sweep on every Nth photograph')
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
        fit = tuple(int(v) for v in args.fit.lower().split('x'))
        photos, photo_sources, photo_profiles = photo_fixtures(args.photos, fit)
        diagram = diagram_fixture()
        size_crops = [tuple(int(v) for v in c.lower().split('x')) for c in args.size_crops.split(',')]
        size_images = sorted(photos)[::args.size_stride] if args.size_photos else []
        sweep, sweep_sources = crop_fixtures(args.size_photos, size_images, size_crops) if args.size_photos else ({}, {})
        diagram.save(out / 'diagram.png')
        images = dict(photos, diagram=diagram)
        # Every Nth photograph in name order keeps the subset spread over the
        # corpus; the diagram always runs the generation experiment.
        generation_images = sorted(photos)[::args.generation_stride] + ['diagram']
        references = {}
        for name, image in images.items():
            references[name] = work / f'{name}.png'
            image.save(references[name])
            # A reference must not carry a profile that LSQA would apply.
            assert b'iCCP' not in references[name].read_bytes(), name
        codecs = Codecs(args.adapter.resolve(), args.library.resolve())
        lsqa = args.source / 'assessment/lsqa'
        settings = ['SIXEL default', 'SIXEL quality', 'GIF', 'PNG', 'QOI', 'JPEG 60', 'JPEG 80', 'JPEG 95', 'WebP 60', 'WebP 80', 'WebP 95', 'WebP lossless', 'AVIF 60', 'AVIF 80', 'HEIC 60', 'HEIC 80']
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
            'pillow': Image.__version__, 'jpeg': features.version('jpg'),
            'libjpeg_turbo': features.version('libjpeg_turbo'), 'zlib': features.version('zlib'),
            'qoi': getattr(qoi, '__version__', 'unknown'),
            'avif': features.version('avif'),
            'avif_codecs': AvifImagePlugin._avif.codec_versions(),
            'pillow_heif': pillow_heif.__version__,
            'libheif': pillow_heif.libheif_info(),
            'adapter_sha256': sha(args.adapter.read_bytes()), 'library_sha256': sha(args.library.read_bytes()),
            'runs': args.runs, 'warmups': 2, 'generations': args.generations,
            'settings': settings, 'fit': list(fit),
            'generation_images': generation_images,
            'photo_directory': str(args.photos),
            'photo_sources': photo_sources,
            'photo_profiles_converted_to_srgb': photo_profiles,
            'inputs': {k: sha(references[k].read_bytes()) for k in images},
            'input_sizes': {k: list(v.size) for k, v in images.items()},
            'size_sweep': {'photo_directory': str(args.size_photos) if args.size_photos else None,
                'images': size_images, 'sizes': [list(c) for c in size_crops],
                'threads': [1, 8], 'photo_sources': sweep_sources,
                'inputs': {f'{k[0]}@{k[1][0]}x{k[1][1]}': sha(v.tobytes()) for k, v in sweep.items()}}},
            'rows': rows}
        def save():
            (out / 'results.json').write_text(json.dumps(payload, indent=2) + '\n')
        for name, image in images.items():
            reference = references[name]
            # Alternate traversal between fixtures; every decoder budget uses
            # the same one-thread stream, independently of encoder changes.
            order = settings if len(rows) // len(settings) % 2 == 0 else settings[::-1]
            for codec in order:
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
            print(name, 'scaling complete', flush=True)
            if name not in generation_images:
                continue
            for codec in ['SIXEL default', 'GIF', 'PNG', 'QOI', 'JPEG 80', 'WebP 80', 'WebP lossless', 'AVIF 80', 'HEIC 80']:
                modes = ['indexed'] if codec in ['SIXEL default', 'GIF'] else ['RGB']
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
            print(name, 'generations complete', flush=True)
        # Speed against pixel count: no LSQA, so the sweep stays cheap; each
        # crop decodes the same one-thread stream at every budget.
        for index, ((name, crop), image) in enumerate(sweep.items()):
            for codec in settings if index % 2 == 0 else settings[::-1]:
                fixed = codecs.encode(image, codec, 1)
                expected = codecs.decode(fixed, codec, 1).tobytes()
                for threads in [1, 8]:
                    encoded, enc = sampled(lambda: codecs.encode(image, codec, threads), args.runs)
                    decoded, dec = sampled(lambda: codecs.decode(fixed, codec, threads), args.runs)
                    assert decoded.tobytes() == expected
                    rows.append(dict(image=name, experiment='size', size=list(crop),
                        codec=codec, threads=threads, encode_ms=enc, decode_ms=dec,
                        bytes=len(encoded), transport_bytes=len(encoded) if codec.startswith('SIXEL') else 4 * ((len(encoded) + 2) // 3),
                        stream_sha256=sha(encoded), decoder_stream_sha256=sha(fixed)))
            save()
            print(name, crop, 'size sweep complete', flush=True)


if __name__ == '__main__':
    main()
