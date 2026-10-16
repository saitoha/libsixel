#!/usr/bin/env python3
"""Add stage timers to an unpacked Pillow 12.3.0 source tree (argv[1]).

The timers accumulate monotonic nanoseconds around Pillow's own stage
boundaries (the same points its DEBUG build prints) and are read and reset
with PIL._imaging._stage_times(). They do not change any computation: GIF
and PNG output of the patched build must stay byte-identical to the wheel,
which stages_pillow.py does not assume and the study verifies separately.
"""
import sys
from pathlib import Path
root = Path(sys.argv[1]) / 'src'
def rep(path, a, b, count=1):
    p = root / path; s = p.read_text()
    assert s.count(a) == count, (path, a[:60], s.count(a))
    p.write_text(s.replace(a, b))
# Shared accumulators: [0] quantize total, [1] histogram, [2] median cut and
# palette, [3] distance tables and pixel mapping, [4] GIF LZW encoder calls,
# [5] PNG encoder calls, [6] PNG deflate() inside them.
rep('libImaging/Imaging.h', '#include "Arrow.h"\n',
    '#include "Arrow.h"\n\n'
    'extern double ImagingStageNs[8];\nextern double ImagingStageNow(void);\n')
rep('libImaging/Quant.c', '\nstatic int\nquantize(\n', '''
#include <time.h>
double ImagingStageNs[8];
double
ImagingStageNow(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e9 + (double)ts.tv_nsec;
}

static int
quantize(
''')
rep('libImaging/Quant.c', '    h = create_pixel_hash(pixelData, nPixels);\n',
    '    double stage_t0 = ImagingStageNow(), stage_t1;\n'
    '    h = create_pixel_hash(pixelData, nPixels);\n'
    '    stage_t1 = ImagingStageNow();\n    ImagingStageNs[1] += stage_t1 - stage_t0;\n    stage_t0 = stage_t1;\n')
rep('libImaging/Quant.c', '''    free_box_tree(root);
    root = NULL;

    /* malloc check ok, using calloc for overflow */
    qp = calloc(nPixels, sizeof(uint32_t));''', '''    free_box_tree(root);
    root = NULL;
    stage_t1 = ImagingStageNow();
    ImagingStageNs[2] += stage_t1 - stage_t0;
    stage_t0 = stage_t1;

    /* malloc check ok, using calloc for overflow */
    qp = calloc(nPixels, sizeof(uint32_t));''')
rep('libImaging/Quant.c', '''    if (!map_image_pixels_from_median_box(
            pixelData, nPixels, p, nPaletteEntries, h, avgDist, avgDistSortKey, qp
        )) {
        goto error_7;
    }
''', '''    if (!map_image_pixels_from_median_box(
            pixelData, nPixels, p, nPaletteEntries, h, avgDist, avgDistSortKey, qp
        )) {
        goto error_7;
    }
    ImagingStageNs[3] += ImagingStageNow() - stage_t0;
''')
rep('libImaging/Quant.c', '\nImaging\nImagingQuantize(Imaging im, int colors, int mode, int kmeans) {\n',
    '\nstatic Imaging\nImagingQuantizeInner(Imaging im, int colors, int mode, int kmeans);\n\n'
    'Imaging\nImagingQuantize(Imaging im, int colors, int mode, int kmeans) {\n'
    '    double t0 = ImagingStageNow();\n'
    '    Imaging out = ImagingQuantizeInner(im, colors, mode, kmeans);\n'
    '    ImagingStageNs[0] += ImagingStageNow() - t0;\n    return out;\n}\n\n'
    'static Imaging\nImagingQuantizeInner(Imaging im, int colors, int mode, int kmeans) {\n')
rep('libImaging/GifEncode.c', '\nint\nImagingGifEncode(Imaging im, ImagingCodecState state, UINT8 *buf, int bytes) {\n',
    '\nstatic int\nImagingGifEncodeInner(Imaging im, ImagingCodecState state, UINT8 *buf, int bytes);\n\n'
    'int\nImagingGifEncode(Imaging im, ImagingCodecState state, UINT8 *buf, int bytes) {\n'
    '    double t0 = ImagingStageNow();\n'
    '    int n = ImagingGifEncodeInner(im, state, buf, bytes);\n'
    '    ImagingStageNs[4] += ImagingStageNow() - t0;\n    return n;\n}\n\n'
    'static int\nImagingGifEncodeInner(Imaging im, ImagingCodecState state, UINT8 *buf, int bytes) {\n')
rep('libImaging/ZipEncode.c', '\nint\nImagingZipEncode(Imaging im, ImagingCodecState state, UINT8 *buf, int bytes) {\n',
    '\nstatic int\ntimed_deflate(z_streamp stream, int flush) {\n'
    '    double t0 = ImagingStageNow();\n    int err = deflate(stream, flush);\n'
    '    ImagingStageNs[6] += ImagingStageNow() - t0;\n    return err;\n}\n\n'
    'static int\nImagingZipEncodeInner(Imaging im, ImagingCodecState state, UINT8 *buf, int bytes);\n\n'
    'int\nImagingZipEncode(Imaging im, ImagingCodecState state, UINT8 *buf, int bytes) {\n'
    '    double t0 = ImagingStageNow();\n'
    '    int n = ImagingZipEncodeInner(im, state, buf, bytes);\n'
    '    ImagingStageNs[5] += ImagingStageNow() - t0;\n    return n;\n}\n\n'
    'static int\nImagingZipEncodeInner(Imaging im, ImagingCodecState state, UINT8 *buf, int bytes) {\n')
rep('libImaging/ZipEncode.c', 'err = deflate(&context->z_stream,', 'err = timed_deflate(&context->z_stream,', count=3)
rep('_imaging.c', 'static PyMethodDef functions[] = {\n',
    '''static PyObject *
_stage_times(PyObject *self, PyObject *args) {
    PyObject *out = PyList_New(8);
    int i;
    for (i = 0; i < 8; i++) {
        PyList_SET_ITEM(out, i, PyFloat_FromDouble(ImagingStageNs[i] / 1e6));
        ImagingStageNs[i] = 0;
    }
    return out;
}

static PyMethodDef functions[] = {
    {"_stage_times", (PyCFunction)_stage_times, METH_NOARGS},
''')
print('patched')
