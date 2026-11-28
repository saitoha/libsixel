/* SPDX-License-Identifier: MIT */
/* A byte derivative belongs to a clone, never to the canonical float frame. */
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sixel.h>
#include "src/frame.h"
#include "src/encoder.h"
#include "tests/_support/planner/pipeline/gpu_ahead_common.h"
#include "src/frame-normalize.h"
#include "src/pixelformat.h"
#include <6cells.h>
#include "src/chunk-factory.h"
#include "src/cms.h"
#include "src/sixel_decode_pixels.h"
#include "tests/_support/planner/pipeline/playback_test_common.h"
#include "src/loader.h"
#include "src/loader-common.h"
#include "tests/_support/diagnostics/icc_numeric_common.h"
#include "tests/data/color-numeric/icc.h"
#include "tests/data/color-numeric/paired-lut.h"
#include "tests/data/color-numeric/opponents.h"
#include "tests/data/color-numeric/vectors.h"

/* Every pixel has the same color. Only the palette key, alpha channel and
 * explicit mask distinguish the independently expected painted positions. */
static int
test_converted_paint(int with_mask, int source, int target, int width_out)
{
    unsigned char indices[] = {1, 0, 0, 1};
    unsigned char palette[] = {255, 0, 0, 255, 0, 0};
    unsigned char packed[] = {0x90};
    unsigned char last[] = {255, 255, 255, 0, 255, 255, 255, 255,
                            255, 255, 255, 127, 255, 255, 255, 0};
    unsigned char first[] = {0, 255, 255, 255, 255, 255, 255, 255,
                             127, 255, 255, 255, 0, 255, 255, 255};
    unsigned char ga[] = {255, 0, 255, 255, 255, 127, 255, 0};
    unsigned char ag[] = {0, 255, 255, 255, 127, 255, 0, 255};
    unsigned char *samples;
    int indexed = (source & SIXEL_FORMATTYPE_PALETTE) != 0;
    unsigned char expected[] = {0, 255, 255, 0};
    sixel_frame_t *frame = NULL;
    sixel_frame_interface_t *iface;
    sixel_frame_transparency_t coverage = {0};
    sixel_encoder_t *encoder = NULL;
    sixel_output_t *output = NULL;
    playback_test_buffer_t stream = {0};
    unsigned char *pixels = NULL, *paint = NULL, *colors = NULL;
    int width, height, ncolors;
    unsigned int flags;
    int result = EXIT_FAILURE;

    switch (source) {
    case SIXEL_PIXELFORMAT_PAL8: samples = indices; break;
    case SIXEL_PIXELFORMAT_PAL1: samples = packed; break;
    case SIXEL_PIXELFORMAT_ARGB8888:
    case SIXEL_PIXELFORMAT_ABGR8888: samples = first; break;
    case SIXEL_PIXELFORMAT_GA88: samples = ga; break;
    case SIXEL_PIXELFORMAT_AG88: samples = ag; break;
    default: samples = last; break;
    }
    if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
        SIXEL_FAILED(sixel_frame_init_borrowed(frame, samples, 4, 1,
            source, indexed ? palette : NULL, indexed ? 2 : -1))) {
        goto end;
    }
    iface = sixel_frame_as_interface(frame);
    coverage.transparent = indexed ? 1 : -1;
    coverage.alpha_zero_is_transparent = !indexed;
    if (with_mask) {
        coverage.transparent_mask = calloc(4, 1);
        if (coverage.transparent_mask == NULL) {
            goto end;
        }
        coverage.transparent_mask_size = 4;
        coverage.transparent_mask[2] = 1;
        expected[2] = 0;
    }
    if (SIXEL_FAILED(iface->vtbl->set_transparency(iface, &coverage))) {
        free(coverage.transparent_mask);
        goto end;
    }
    if ((width_out != 4 &&
         SIXEL_FAILED(iface->vtbl->clip(iface, 0, 0, width_out, 1))) ||
        SIXEL_FAILED(iface->vtbl->set_pixelformat(iface, target)) ||
        SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
        SIXEL_FAILED(sixel_output_new(&output, playback_test_write,
            &stream, NULL)) ||
        SIXEL_FAILED(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_GPU_POLICY,
            "off")) ||
        SIXEL_FAILED(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_DIFFUSION,
            "none")) ||
        SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame, output)) ||
        SIXEL_FAILED(sixel_decode_raw_with_options_mask(stream.bytes,
            (int)stream.size, 0, &pixels, &paint, &width, &height, &colors,
            &ncolors, &flags, NULL)) ||
        width != width_out || height != 1 || paint == NULL ||
        memcmp(paint, expected, (size_t)width_out) != 0) {
        goto end;
    }
    result = EXIT_SUCCESS;
end:
    free(colors);
    free(paint);
    free(pixels);
    free(stream.bytes);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    sixel_frame_unref(frame);
    return result;
}

/* Padding is nonzero. Only the two actual indices name palette entries. */
static int
test_implicit_palette(void)
{
    unsigned char row[] = {0x7f};
    unsigned char palette[] = {255, 0, 0, 0, 255, 0};
    float const expected[] = {1, 0, 0, 0, 1, 0};
    sixel_frame_t *frame = NULL;
    sixel_frame_interface_t *iface, *clone = NULL;
    sixel_frame_pixels_view_t view;
    size_t bytes;
    int result = EXIT_FAILURE;

    if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
        SIXEL_FAILED(sixel_frame_init_borrowed(frame, row, 2, 1,
            SIXEL_PIXELFORMAT_PAL1, palette, -1))) {
        goto end;
    }
    iface = sixel_frame_as_interface(frame);
    if (SIXEL_FAILED(iface->vtbl->measure_storage(iface, &bytes)) ||
        bytes != sizeof(row) + sizeof(palette) ||
        SIXEL_FAILED(iface->vtbl->clone(iface, NULL, &clone))) {
        goto end;
    }
    /* A clone must retain its own colors after the source palette changes. */
    memset(palette, 0, sizeof(palette));
    if (SIXEL_SUCCEEDED(clone->vtbl->set_pixelformat(clone,
            SIXEL_PIXELFORMAT_RGBFLOAT32)) &&
        SIXEL_SUCCEEDED(sixel_frame_get_work_pixels(clone, &view)) &&
        view.pixels_float32 != NULL &&
        memcmp(view.pixels_float32, expected, sizeof(expected)) == 0) {
        result = EXIT_SUCCESS;
    }
end:
    if (clone != NULL) {
        clone->vtbl->unref(clone);
    }
    sixel_frame_unref(frame);
    return result;
}

static int
test_implicit_palette_crop(void)
{
    unsigned char *indices = malloc(2), *palette = calloc(256, 3);
    sixel_frame_t *frame = NULL;
    sixel_frame_interface_t *iface;
    size_t bytes;
    int result = EXIT_FAILURE;

    if (indices == NULL || palette == NULL) {
        goto end;
    }
    indices[0] = 0;
    indices[1] = 255;
    if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
        SIXEL_FAILED(sixel_frame_init(frame, indices, 2, 1,
            SIXEL_PIXELFORMAT_PAL8, palette, -1))) {
        goto end;
    }
    indices = palette = NULL;
    iface = sixel_frame_as_interface(frame);
    /* Cropping away register 255 releases neither adopted allocation. */
    if (SIXEL_SUCCEEDED(iface->vtbl->clip(iface, 0, 0, 1, 1)) &&
        SIXEL_SUCCEEDED(iface->vtbl->measure_storage(iface, &bytes)) &&
        bytes == 2 + 256 * 3) {
        result = EXIT_SUCCESS;
    }
end:
    sixel_frame_unref(frame);
    free(indices);
    free(palette);
    return result;
}

static int
test_linear_packed_gray(int format)
{
    void *source_pixels;
    /* sRGB OETF evaluated at k/15 and rounded to the nearest byte.
     * Every fifth value also gives the four levels of a G2 ramp. */
    static unsigned char const expected[] = {
        0, 73, 102, 124, 141, 156, 170, 182,
        193, 203, 213, 222, 231, 239, 247, 255
    };
    unsigned char g2[] = {0x1b};
    unsigned char g4[] = {0x01, 0x23, 0x45, 0x67, 0x89, 0xab, 0xcd, 0xef};
    sixel_frame_t *frame = NULL;
    sixel_frame_pixels_view_t view;
    sixel_frame_interface_t *iface;
    int i, c, width = format == SIXEL_PIXELFORMAT_G2 ? 4 : 16;
    int result = EXIT_FAILURE;

    /* PCC can fail when converting a conditional pointer to void * inside
     * a braced initializer. Convert it in a separate assignment. */
    source_pixels = width == 4 ? g2 : g4;
    if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
        SIXEL_FAILED(sixel_frame_borrow_pixels(frame,
            &(sixel_frame_pixels_request_t){
                source_pixels, NULL, width, 1, format,
                SIXEL_COLORSPACE_SRGB_LINEAR, -1, SIXEL_FRAME_PIXELS_U8
            }))) {
        goto end;
    }
    iface = sixel_frame_as_interface(frame);
    if (SIXEL_FAILED(iface->vtbl->set_pixelformat(iface,
            SIXEL_PIXELFORMAT_RGB888)) ||
        SIXEL_FAILED(sixel_frame_get_work_pixels(iface, &view)) ||
        view.colorspace != SIXEL_COLORSPACE_SRGB || view.pixels == NULL) {
        goto end;
    }
    for (i = 0; i < width; ++i) {
        for (c = 0; c < 3; ++c) {
            if (view.pixels[3 * i + c] != expected[i * 15 / (width - 1)]) {
                goto end;
            }
        }
    }
    result = EXIT_SUCCESS;
end:
    sixel_frame_unref(frame);
    return result;
}

/* Check each channel independently, including overlapping in-place writes
 * to the first pixel and alpha at both endpoints and halfway. */
static int
test_alpha_composite(int format, int composite)
{
    unsigned char background[] = {10, 20, 30};
    unsigned char const expected[][9] = {
        {32, 64, 192, 32, 64, 192, 32, 64, 192},
        {32, 64, 192, 10, 20, 30, 21, 42, 111}
    };
    unsigned char pixels[12];
    unsigned char const alpha[] = {255, 0, 128};
    sixel_frame_t *frame = NULL;
    int i, offset, reverse;
    int result = EXIT_FAILURE;

    offset = format == SIXEL_PIXELFORMAT_ARGB8888 ||
             format == SIXEL_PIXELFORMAT_ABGR8888;
    reverse = format == SIXEL_PIXELFORMAT_BGRA8888 ||
              format == SIXEL_PIXELFORMAT_ABGR8888;
    for (i = 0; i < 3; ++i) {
        pixels[4 * i + (offset ? 0 : 3)] = alpha[i];
        pixels[4 * i + offset] = reverse ? 192 : 32;
        pixels[4 * i + offset + 1] = 64;
        pixels[4 * i + offset + 2] = reverse ? 32 : 192;
    }
    if (SIXEL_SUCCEEDED(sixel_frame_new(&frame, NULL)) &&
        SIXEL_SUCCEEDED(sixel_frame_init_borrowed(frame, pixels, 3, 1,
            format, NULL, -1)) &&
        SIXEL_SUCCEEDED(sixel_frame_strip_alpha(
            frame, composite ? background : NULL)) &&
        sixel_frame_format(frame) == SIXEL_PIXELFORMAT_RGB888 &&
        memcmp(sixel_frame_get_pixels(frame), expected[composite],
               sizeof(expected[0])) == 0) {
        result = EXIT_SUCCESS;
    }
    sixel_frame_unref(frame);
    return result;
}

/* Record the requested payload size and leave readable guard space. The
 * source also backs a full 18 bytes so an old unpacked copy is observable. */
static size_t packed_expected_size;
static unsigned char *packed_allocation;

static void *
packed_clone_malloc(size_t size)
{
    unsigned char *ptr;

    if (size != packed_expected_size) {
        return malloc(size);
    }
    ptr = malloc(size + 18u);
    if (ptr != NULL) {
        memset(ptr, 0xee, size + 18u);
    }
    packed_allocation = ptr;
    return ptr;
}

static int
test_packed_rows(int clone_pixels)
{
    static const struct {
        int format;
        size_t aligned, padded;
        int ncolors;
    } cases[] = {
        {SIXEL_PIXELFORMAT_PAL1, 2, 4, 2},
        {SIXEL_PIXELFORMAT_PAL2, 4, 6, 2},
        {SIXEL_PIXELFORMAT_PAL4, 8, 10, 2},
        {SIXEL_PIXELFORMAT_G1, 2, 4, 0},
        {SIXEL_PIXELFORMAT_G2, 4, 6, 0},
        {SIXEL_PIXELFORMAT_G4, 8, 10, 0}
    };
    unsigned char source[18], palette[6] = {0};
    sixel_frame_t *frame = NULL;
    sixel_allocator_t *allocator = NULL;
    sixel_frame_interface_t *iface, *clone = NULL;
    sixel_frame_pixels_view_t view;
    size_t i, measured, bytes;
    int width;
    int result = EXIT_FAILURE;

    for (i = 0; i < sizeof(source); ++i) {
        source[i] = (unsigned char)(0xa0u + i);
    }
    if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
        SIXEL_FAILED(sixel_allocator_new(&allocator, packed_clone_malloc,
            calloc, realloc, free))) {
        goto end;
    }
    iface = sixel_frame_as_interface(frame);
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        for (width = 8; width <= 9; ++width) {
            bytes = width == 8 ? cases[i].aligned : cases[i].padded;
            if (SIXEL_FAILED(sixel_frame_init_borrowed(frame, source, width,
                    2, cases[i].format, cases[i].ncolors ? palette : NULL,
                    cases[i].ncolors))) {
                goto end;
            }
            if (!clone_pixels) {
                if (SIXEL_FAILED(iface->vtbl->measure_storage(iface,
                        &measured)) ||
                    measured != bytes + (size_t)cases[i].ncolors * 3u) {
                    goto end;
                }
                continue;
            }
            packed_expected_size = bytes;
            packed_allocation = NULL;
            if (SIXEL_FAILED(iface->vtbl->clone(iface, allocator, &clone)) ||
                SIXEL_FAILED(sixel_frame_get_work_pixels(clone, &view)) ||
                view.pixels == NULL || view.pixels != packed_allocation ||
                memcmp(view.pixels, source, bytes) != 0 ||
                view.pixels[bytes] != 0xee) {
                goto end;
            }
            clone->vtbl->unref(clone);
            clone = NULL;
        }
    }
    result = EXIT_SUCCESS;
end:
    if (clone != NULL) {
        clone->vtbl->unref(clone);
    }
    packed_expected_size = 0;
    packed_allocation = NULL;
    sixel_allocator_unref(allocator);
    sixel_frame_unref(frame);
    return result;
}

/* Use the actual adopted plane sizes. The profile's footprint can vary with
 * the build, but it is identical on the source and its one-pixel clone. */
static int
test_crop_storage(int coverage)
{
    enum { side = 256, count = side * side };
    sixel_frame_t *frame = NULL;
    sixel_frame_interface_t *iface, *clone = NULL;
    sixel_frame_pixels_view_t view;
    sixel_frame_transparency_t mask = {0};
    sixel_frame_alpha_t alpha = {0};
    sixel_color_def_t *p3 = NULL;
    float *pixels, *adopted;
    size_t before, retained, compact, stride, extra;
    int result = EXIT_FAILURE;

    pixels = calloc(count, 3 * sizeof(float));
    adopted = pixels;
    p3 = sixel_cdef_name("display-p3");
    if (pixels == NULL || p3 == NULL ||
        SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
        SIXEL_FAILED(sixel_frame_init(frame, (unsigned char *)pixels,
            side, side, SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1))) {
        goto end;
    }
    pixels = NULL;
    iface = sixel_frame_as_interface(frame);
    if (SIXEL_FAILED(iface->vtbl->set_rgb_definition(iface, p3))) {
        goto end;
    }
    extra = 0;
    if (coverage == 1) {
        mask.transparent = -1;
        mask.transparent_mask_size = count;
        mask.transparent_mask = calloc(count, 1);
        if (mask.transparent_mask == NULL) {
            goto end;
        }
        mask.transparent_mask[count - 1] = 1;
        if (SIXEL_FAILED(iface->vtbl->set_transparency(iface, &mask))) {
            goto end;
        }
        mask.transparent_mask = NULL;
        extra = 1;
    } else if (coverage == 2) {
        alpha.values = calloc(count, sizeof(float));
        alpha.count = count;
        if (alpha.values == NULL ||
            SIXEL_FAILED(iface->vtbl->set_alpha(iface, &alpha))) {
            goto end;
        }
        alpha.values = NULL;
        extra = sizeof(float);
    }

    stride = 3 * sizeof(float) + extra;
    if (SIXEL_FAILED(iface->vtbl->measure_storage(iface, &before)) ||
        before < count * stride ||
        (coverage && SIXEL_FAILED(iface->vtbl->clip(iface, side - 2,
            side - 2, 2, 2))) ||
        SIXEL_FAILED(iface->vtbl->clip(iface, coverage ? 1 : side - 1,
            coverage ? 1 : side - 1, 1, 1)) ||
        SIXEL_FAILED(sixel_frame_get_work_pixels(iface, &view)) ||
        view.width != 1 || view.height != 1 || view.pixels_float32 != adopted ||
        SIXEL_FAILED(iface->vtbl->measure_storage(iface, &retained)) ||
        retained != before ||
        SIXEL_FAILED(iface->vtbl->clone(iface, NULL, &clone)) ||
        SIXEL_FAILED(clone->vtbl->measure_storage(clone, &compact)) ||
        compact != before - (count - 1) * stride) {
        goto end;
    }
    /* Detaching a coverage plane releases its entire original allocation.
     * Converting the cropped pixels then leaves just one RGB888 triplet. */
    if (coverage == 1) {
        mask.transparent_mask_size = 0;
        if (SIXEL_FAILED(iface->vtbl->set_transparency(iface, &mask))) {
            goto end;
        }
    } else if (coverage == 2) {
        alpha.count = 0;
        if (SIXEL_FAILED(iface->vtbl->set_alpha(iface, &alpha))) {
            goto end;
        }
    }
    if (SIXEL_FAILED(iface->vtbl->measure_storage(iface, &retained)) ||
        retained != before - count * extra ||
        SIXEL_FAILED(iface->vtbl->set_pixelformat(iface,
            SIXEL_PIXELFORMAT_RGB888)) ||
        SIXEL_FAILED(iface->vtbl->measure_storage(iface, &retained)) ||
        retained != 3) {
        goto end;
    }
    result = EXIT_SUCCESS;
end:
    free(alpha.values);
    free(mask.transparent_mask);
    free(pixels);
    if (clone != NULL) {
        clone->vtbl->unref(clone);
    }
    sixel_frame_unref(frame);
    sixel_cdef_free(p3);
    return result;
}

/* Promotion allocates float triplets before unpacking the two PAL1 rows.
 * Reject only the unpack allocation so the first allocation must unwind. */
static int fail_six_bytes;

static void *
profile_frame_malloc(size_t size)
{
    return fail_six_bytes && size == 6 ? NULL : malloc(size);
}

/* A palette key identifies every matching index. A separate mask can also
 * hide just one occurrence of a visible index, without losing its color. */
static int
test_indexed_delivery(int format, int key, int linear, int target,
                      int explicit_capacity)
{
    void *source_pixels;
    int colorspace;
    unsigned char const pattern[] = {0, 1, 1, 0, 1, 0, 0, 1};
    unsigned char indices[sizeof(pattern)];
    unsigned char packed[] = {0x60, 0x90};
    unsigned char palette[] = {128, 128, 128, 64, 64, 64};
    unsigned char const hidden[] = {1, 0, 1, 0, 0, 0, 0, 0};
    unsigned char *pixels = NULL, *paint = NULL, *colors = NULL;
    sixel_frame_t *frame = NULL;
    sixel_frame_interface_t *iface;
    sixel_frame_pixels_view_t view;
    sixel_frame_transparency_t coverage = {0};
    sixel_encoder_t *encoder = NULL;
    sixel_output_t *output = NULL;
    playback_test_buffer_t stream = {0};
    int width, height, ncolors, i, c, expected;
    unsigned int flags;
    double code, gamma;
    int result = EXIT_FAILURE;

    memcpy(indices, pattern, sizeof(indices));
    source_pixels = format == SIXEL_PIXELFORMAT_PAL8 ? indices : packed;
    colorspace = linear ? SIXEL_COLORSPACE_SRGB_LINEAR : SIXEL_COLORSPACE_SRGB;
    if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
        SIXEL_FAILED(sixel_frame_borrow_pixels(frame,
            &(sixel_frame_pixels_request_t){
                source_pixels,
                palette, 4, 2, format,
                colorspace,
                2, SIXEL_FRAME_PIXELS_U8
            }))) {
        goto end;
    }
    iface = sixel_frame_as_interface(frame);
    if (!linear) {
        coverage.transparent = key;
        coverage.alpha_zero_is_transparent = 1;
        coverage.transparent_mask_size = sizeof(hidden);
        coverage.transparent_mask = malloc(sizeof(hidden));
        if (coverage.transparent_mask == NULL) {
            goto end;
        }
        memcpy(coverage.transparent_mask, hidden, sizeof(hidden));
        if (SIXEL_FAILED(iface->vtbl->set_transparency(iface, &coverage))) {
            free(coverage.transparent_mask);
            goto end;
        }
    }
    if (target != format) {
        if (SIXEL_FAILED(iface->vtbl->set_pixelformat(iface, target)) ||
            SIXEL_FAILED(sixel_frame_get_work_pixels(iface, &view)) ||
            view.pixelformat != target ||
            view.colorspace != SIXEL_COLORSPACE_SRGB) {
            goto end;
        }
        for (i = 0; i < 8; ++i) {
            code = (pattern[i] ? 64.0 : 128.0) / 255.0;
            gamma = 1.055 * pow(code, 1.0 / 2.4) - 0.055;
            for (c = 0; c < 3; ++c) {
                if (target == SIXEL_PIXELFORMAT_RGB888) {
                    if (view.pixels[3 * i + c] != (pattern[i] ? 137 : 188)) {
                        goto end;
                    }
                } else if (fabs(view.pixels_float32[3 * i + c] - gamma)
                           > 1e-6) {
                    goto end;
                }
            }
        }
    }
    if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
        SIXEL_FAILED(sixel_output_new(&output, playback_test_write,
            &stream, NULL)) ||
        SIXEL_FAILED(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_GPU_POLICY,
            "off")) ||
        SIXEL_FAILED(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_DIFFUSION,
            "none")) ||
        (explicit_capacity && SIXEL_FAILED(sixel_encoder_setopt(encoder,
            SIXEL_OPTFLAG_COLORS, "256"))) ||
        SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame, output)) ||
        SIXEL_FAILED(sixel_decode_raw_with_options_mask(stream.bytes,
            (int)stream.size, 0, &pixels, &paint, &width, &height, &colors,
            &ncolors, &flags, NULL)) || width != 4 || height != 2) {
        goto end;
    }
    for (i = 0; i < 8; ++i) {
        expected = !linear && (hidden[i] || pattern[i] == key) ? 0 : 255;
        if (paint == NULL || paint[i] != expected) {
            goto end;
        }
        if (expected == 0) {
            continue;
        }
        /* SIXEL registers use percentages. Decode the expected 74%/54%
         * (linear palette) or 50%/25% (encoded palette) independently. */
        expected = linear ? (pattern[i] ? 138 : 189) :
                            (pattern[i] ? 64 : 128);
        for (c = 0; c < 3; ++c) {
            if (pixels[i] >= ncolors ||
                colors[3 * pixels[i] + c] != expected) {
                goto end;
            }
        }
    }
    result = EXIT_SUCCESS;
end:
    free(colors);
    free(paint);
    free(pixels);
    free(stream.bytes);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    sixel_frame_unref(frame);
    return result;
}

static int
test_linear_strip(void)
{
    unsigned char pixel[] = {128, 128, 128, 255};
    unsigned char const expected[] = {188, 188, 188};
    sixel_frame_t *frame = NULL;
    int result = EXIT_FAILURE;

    if (SIXEL_SUCCEEDED(sixel_frame_new(&frame, NULL)) &&
        SIXEL_SUCCEEDED(sixel_frame_borrow_pixels(frame,
            &(sixel_frame_pixels_request_t){
                pixel, NULL, 1, 1, SIXEL_PIXELFORMAT_RGBA8888,
                SIXEL_COLORSPACE_SRGB_LINEAR, -1, SIXEL_FRAME_PIXELS_U8
            }))) {
        if (SIXEL_SUCCEEDED(sixel_frame_strip_alpha(frame, NULL)) &&
            sixel_frame_get_colorspace(frame) == SIXEL_COLORSPACE_SRGB_LINEAR &&
            SIXEL_SUCCEEDED(sixel_frame_set_pixelformat(frame,
                SIXEL_PIXELFORMAT_RGB888)) &&
            memcmp(sixel_frame_get_pixels(frame), expected, 3) == 0) {
            result = EXIT_SUCCESS;
        }
    }
    sixel_frame_unref(frame);
    return result;
}

static int
test_analytic_independence(int p3)
{
    double const xy[] = {.3127, .329, .64, .33, .30, .60, .15, .06};
    double const gamma[] = {1, 1, 1};
    float const reference[] = {1, 1e-25f, 0};
    float identity[3];
    float blue[] = {.25f, .5f, 0, .25f, .5f, 1e7f};
    unsigned char const expected[] = {32, 129, 0, 32, 129, 255};
    sixel_color_def_t *definition;
    sixel_frame_t *frame = NULL;
    sixel_frame_interface_t *iface;
    sixel_frame_pixels_view_t view;
    int result = EXIT_FAILURE;

    memcpy(identity, reference, sizeof(identity));
    /* P3 and sRGB share blue. Increasing blue can only change blue. */
    definition = p3 ? sixel_cdef_name("display-p3") :
                      sixel_cdef_rgb(xy, SIXEL_COLOR_TRC_POWER, gamma);
    if (definition != NULL && SIXEL_SUCCEEDED(sixel_frame_new(&frame, NULL)) &&
        SIXEL_SUCCEEDED(sixel_frame_init_borrowed(frame,
            p3 ? blue : identity, p3 ? 2 : 1, 1,
            SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1))) {
        iface = sixel_frame_as_interface(frame);
        if (SIXEL_SUCCEEDED(iface->vtbl->set_rgb_definition(iface,
                definition)) &&
            SIXEL_SUCCEEDED(iface->vtbl->set_pixelformat(iface, p3 ?
                SIXEL_PIXELFORMAT_RGB888 : SIXEL_PIXELFORMAT_LINEARRGBFLOAT32))
            && SIXEL_SUCCEEDED(sixel_frame_get_work_pixels(iface, &view))) {
            result = p3 ? memcmp(view.pixels, expected, sizeof(expected)) :
                memcmp(view.pixels_float32, reference, sizeof(reference));
        }
    }
    sixel_frame_unref(frame);
    sixel_cdef_free(definition);
    return result != 0;
}

static int
test_linear_resize(int retry, int explicit_rgb)
{
    /* Compare stored samples at float precision, including on x87. */
    static float const original_pixels[] = {.0015f, .0015f, .0015f};
    float pixel[3];
    unsigned char const expected[] = {5, 5, 5, 5, 5, 5};
    sixel_allocator_t *allocator = NULL;
    sixel_frame_t *frame = NULL;
    sixel_color_def_t *srgb = NULL, *definition = NULL;
    sixel_frame_pixels_view_t view;
    sixel_frame_interface_t *iface;
    int result = EXIT_FAILURE;

    memcpy(pixel, original_pixels, sizeof(pixel));
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, profile_frame_malloc,
            calloc, realloc, free)) ||
        SIXEL_FAILED(sixel_frame_new(&frame, allocator)) ||
        SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixel, 1, 1,
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, NULL, -1))) {
        goto end;
    }
    iface = sixel_frame_as_interface(frame);
    if (explicit_rgb) {
        srgb = sixel_cdef_name("srgb");
        definition = sixel_cdef_linearized(srgb);
        if (definition == NULL || SIXEL_FAILED(
                iface->vtbl->set_rgb_definition(iface, definition))) {
            goto end;
        }
    }
    if (retry) {
        fail_six_bytes = 1;
        if (iface->vtbl->resize(iface, 2, 1, SIXEL_RES_NEAREST) !=
                SIXEL_BAD_ALLOCATION ||
            SIXEL_FAILED(sixel_frame_get_work_pixels(iface, &view)) ||
            view.pixels_float32 != pixel || view.width != 1 ||
            view.height != 1 || view.colorspace !=
                SIXEL_COLORSPACE_SRGB_LINEAR ||
            memcmp(pixel, original_pixels, sizeof(pixel)) != 0) {
            goto end;
        }
        fail_six_bytes = 0;
    }
    if (SIXEL_SUCCEEDED(iface->vtbl->resize(iface, 2, 1,
            SIXEL_RES_NEAREST)) &&
        SIXEL_SUCCEEDED(iface->vtbl->set_pixelformat(iface,
            SIXEL_PIXELFORMAT_RGB888)) &&
        SIXEL_SUCCEEDED(sixel_frame_get_work_pixels(iface, &view)) &&
        view.width == 2 && view.height == 1 &&
        memcmp(view.pixels, expected, sizeof(expected)) == 0) {
        result = EXIT_SUCCESS;
    }
end:
    fail_six_bytes = 0;
    sixel_frame_unref(frame);
    sixel_allocator_unref(allocator);
    sixel_cdef_free(definition);
    sixel_cdef_free(srgb);
    return result;
}

static int
test_packed_retry(void)
{
    unsigned char indices[] = {0xa0, 0x40};
    unsigned char palette[] = {0, 0, 0, 255, 0, 0};
    sixel_allocator_t *allocator;
    sixel_frame_t *frame;
    sixel_frame_interface_t *iface;
    sixel_frame_pixels_view_t view;
    int result;
    int i;

    result = EXIT_FAILURE;
    allocator = NULL;
    frame = NULL;
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, profile_frame_malloc,
            calloc, realloc, free)) ||
        SIXEL_FAILED(sixel_frame_new(&frame, allocator)) ||
        SIXEL_FAILED(sixel_frame_init_borrowed(frame, indices, 3, 2,
            SIXEL_PIXELFORMAT_PAL1, palette, -1))) {
        goto end;
    }
    iface = sixel_frame_as_interface(frame);
    fail_six_bytes = 1;
    if (iface->vtbl->set_pixelformat(iface, SIXEL_PIXELFORMAT_RGBFLOAT32)
            != SIXEL_BAD_ALLOCATION ||
        SIXEL_FAILED(sixel_frame_get_work_pixels(iface, &view)) ||
        view.pixelformat != SIXEL_PIXELFORMAT_PAL1 ||
        view.pixels != indices || view.palette != palette ||
        indices[0] != 0xa0 || indices[1] != 0x40) {
        goto end;
    }
    fail_six_bytes = 0;
    if (SIXEL_FAILED(iface->vtbl->set_pixelformat(iface,
            SIXEL_PIXELFORMAT_RGBFLOAT32)) ||
        SIXEL_FAILED(sixel_frame_get_work_pixels(iface, &view)) ||
        view.pixels_float32 == NULL) {
        goto end;
    }
    for (i = 0; i < 6; ++i) {
        if (view.pixels_float32[i * 3] != (i % 2 == 0) ||
            view.pixels_float32[i * 3 + 1] != 0 ||
            view.pixels_float32[i * 3 + 2] != 0) {
            goto end;
        }
    }
    result = EXIT_SUCCESS;
end:
    fail_six_bytes = 0;
    sixel_frame_unref(frame);
    sixel_allocator_unref(allocator);
    return result;
}

/* Decode using the reported palette count, as an application would.
 * Register 15 is the initial foreground even without a color command. */
static int
test_raw_default(char const *wire, int width, int height)
{
    unsigned char *pixels = NULL, *palette = NULL;
    unsigned char const *rgb;
    unsigned char const foreground[] = {204, 204, 204};
    sixel_frame_t *frame = NULL;
    int w, h, colors, i;
    int result = EXIT_FAILURE;

    if (SIXEL_FAILED(sixel_decode_raw((unsigned char *)wire, (int)strlen(wire),
            &pixels, &w, &h, &palette, &colors, NULL)) ||
        w != width || h != height || colors != 16 || pixels[0] != 15 ||
        memcmp(palette + 3 * 15, foreground, 3) != 0 ||
        SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
        SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels, w, h,
            SIXEL_PIXELFORMAT_PAL8, palette, colors)) ||
        SIXEL_FAILED(sixel_frame_set_pixelformat(frame,
            SIXEL_PIXELFORMAT_RGB888))) {
        goto end;
    }
    rgb = sixel_frame_get_pixels(frame);
    for (i = 0; i < w * h; ++i) {
        if (pixels[i] != 15 || memcmp(rgb + 3 * i, foreground, 3) != 0) {
            goto end;
        }
    }
    result = EXIT_SUCCESS;
end:
    sixel_frame_unref(frame);
    free(pixels);
    free(palette);
    return result;
}

/* Only the first pixel is painted. decode_raw reports two defined colors
 * and uses index 255 in the second pixel. A clone copies the declared
 * palette, so both paths must resolve the same unpainted default. */
static int
test_raw_palette(void)
{
    unsigned char wire[] = "\033Pq\"1;1;2;1#0;2;100;0;0@?\033\\";
    unsigned char const expected[] = {255, 0, 0, 253, 253, 253};
    unsigned char *pixels = NULL;
    unsigned char *palette = NULL;
    sixel_frame_t *frame = NULL;
    sixel_frame_interface_t *iface;
    sixel_frame_interface_t *clone = NULL;
    sixel_frame_pixels_view_t view;
    int width, height, colors;
    int result = EXIT_FAILURE;
    int i;

    if (SIXEL_FAILED(sixel_decode_raw(wire, (int)sizeof(wire) - 1,
            &pixels, &width, &height, &palette, &colors, NULL)) ||
        width != 2 || height != 1 || colors != 2 ||
        pixels[0] != 0 || pixels[1] != 255 ||
        SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
        SIXEL_FAILED(sixel_frame_init(frame, pixels, width, height,
            SIXEL_PIXELFORMAT_PAL8, palette, colors))) {
        goto end;
    }
    pixels = palette = NULL;
    iface = sixel_frame_as_interface(frame);
    if (SIXEL_FAILED(iface->vtbl->clone(iface, NULL, &clone)) ||
        SIXEL_FAILED(iface->vtbl->set_pixelformat(iface,
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32)) ||
        SIXEL_FAILED(sixel_frame_get_work_pixels(iface, &view)) ||
        view.pixels_float32 == NULL || view.pixels_float32[0] != 1 ||
        view.pixels_float32[1] != 0 || view.pixels_float32[2] != 0) {
        goto end;
    }
    /* Inverse sRGB transfer at 253 / 255. */
    for (i = 3; i < 6; ++i) {
        if (!(fabs(view.pixels_float32[i] - .9822505503) < 1e-6)) {
            goto end;
        }
    }
    if (SIXEL_SUCCEEDED(clone->vtbl->set_pixelformat(clone,
            SIXEL_PIXELFORMAT_RGB888)) &&
        SIXEL_SUCCEEDED(sixel_frame_get_work_pixels(clone, &view)) &&
        view.pixels != NULL && memcmp(view.pixels, expected, 6) == 0) {
        result = EXIT_SUCCESS;
    }
end:
    if (clone != NULL) {
        clone->vtbl->unref(clone);
    }
    sixel_frame_unref(frame);
    free(pixels);
    free(palette);
    return result;
}

/* Identify the retained palette by both allocation size and address. The
 * decoder also allocates scratch space which must not count as frame data. */
static void *byte_palette;

static void *
record_byte_palette(size_t size)
{
    void *p = malloc(size);

    if (size == 256u * 3u) {
        byte_palette = p;
    }
    return p;
}

static SIXELSTATUS
check_raw_storage(sixel_frame_t *frame, void *context)
{
    unsigned char const expected[] = {255, 0, 0, 253, 253, 253};
    sixel_frame_interface_t *iface = sixel_frame_as_interface(frame);
    sixel_frame_interface_t *clone = NULL;
    sixel_frame_pixels_view_t view;
    size_t bytes;
    SIXELSTATUS status = SIXEL_BAD_INPUT;

    if (SIXEL_FAILED(sixel_frame_get_work_pixels(iface, &view)) ||
        view.pixelformat != SIXEL_PIXELFORMAT_PAL8 || view.ncolors != 2 ||
        view.width != 2 || view.height != 1 || view.pixels == NULL ||
        view.pixels[0] != 0 || view.pixels[1] != 255 ||
        view.palette == NULL || view.palette != byte_palette ||
        SIXEL_FAILED(iface->vtbl->measure_storage(iface, &bytes)) ||
        bytes != 2u + 256u * 3u ||
        SIXEL_FAILED(iface->vtbl->clone(iface, NULL, &clone))) {
        return status;
    }
    if (SIXEL_SUCCEEDED(clone->vtbl->set_pixelformat(clone,
            SIXEL_PIXELFORMAT_RGB888)) &&
        SIXEL_SUCCEEDED(sixel_frame_get_work_pixels(clone, &view)) &&
        view.pixels != NULL && memcmp(view.pixels, expected, 6) == 0) {
        ++*(int *)context;
        status = SIXEL_OK;
    }
    clone->vtbl->unref(clone);
    return status;
}

static int
test_raw_storage(void)
{
    unsigned char const wire[] = "\033Pq\"1;1;2;1#0;2;100;0;0@?\033\\";
    sixel_allocator_t *allocator = NULL;
    sixel_loader_t *loader = NULL;
    sixel_chunk_t *chunk = NULL;
    int calls = 0;
    int use_palette = 1;
    int result = EXIT_FAILURE;

    byte_palette = NULL;
    if (SIXEL_SUCCEEDED(sixel_allocator_new(&allocator, record_byte_palette,
            calloc, realloc, free)) &&
        SIXEL_SUCCEEDED(sixel_loader_new(&loader, allocator)) &&
        SIXEL_SUCCEEDED(sixel_chunk_create_from_memory(&chunk, wire,
            sizeof(wire) - 1, NULL, allocator)) &&
        SIXEL_SUCCEEDED(sixel_loader_setopt(loader,
            SIXEL_LOADER_OPTION_LOADER_ORDER, "builtin!")) &&
        SIXEL_SUCCEEDED(sixel_loader_setopt(loader,
            SIXEL_LOADER_OPTION_USE_PALETTE, &use_palette)) &&
        SIXEL_SUCCEEDED(sixel_loader_setopt(loader,
            SIXEL_LOADER_OPTION_CONTEXT, &calls)) &&
        SIXEL_SUCCEEDED(sixel_loader_load_chunk(loader, chunk,
            check_raw_storage)) && calls == 1) {
        result = EXIT_SUCCESS;
    }
    if (chunk != NULL) {
        chunk->vtbl->unref(chunk);
    }
    sixel_loader_unref(loader);
    sixel_allocator_unref(allocator);
    byte_palette = NULL;
    return result;
}

static int
test_profile_colorspace_lut(int relative)
{
    unsigned char bytes[sizeof(color_paired_lut)];
    float pixels[] = {.1f, .2f, .3f, .5f, .5f, .5f};
    /* All mft2 vertices are constant. Decode the PCS XYZ words / 32768
     * and multiply by the inverse Bradford-adapted sRGB primary matrix.
     * A2B0 words: (8192, 16384, 24576); A2B1: (13107, 26214, 39321).
     * Negative and above-one linear coordinates must survive both paths. */
    double const perceptual_rgb[] = {-.3930784364, .7384529717, .9578093449};
    double const relative_rgb[] = {-.6289159017, 1.1815067261, 1.5324715678};
    double const *expected = relative ? relative_rgb : perceptual_rgb;
    sixel_frame_t *frame = NULL;
    sixel_frame_interface_t *iface;
    sixel_frame_pixels_view_t view;
    sixel_color_def_t *profile;
    int c;
    int result = EXIT_FAILURE;

    memcpy(bytes, color_paired_lut, sizeof(bytes));
    memcpy(bytes + 12, "spac", 4);
    if (relative) {
        /* Reuse the second 3-in/3-out LUT as a relative forward table.
         * The original A2B0 payload is identical in both test cases. */
        if (memcmp(bytes + 144, "B2A0", 4) != 0) {
            return EXIT_FAILURE;
        }
        memcpy(bytes + 144, "A2B1", 4);
    }
    profile = sixel_cdef_icc(bytes, sizeof(bytes));
    if (profile == NULL || SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
        SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels, 2, 1,
            SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1))) {
        goto end;
    }
    iface = sixel_frame_as_interface(frame);
    if (SIXEL_FAILED(iface->vtbl->set_rgb_definition(iface, profile)) ||
        SIXEL_FAILED(iface->vtbl->set_pixelformat(iface,
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32)) ||
        SIXEL_FAILED(sixel_frame_get_work_pixels(iface, &view)) ||
        view.pixels_float32 == NULL) {
        goto end;
    }
    for (c = 0; c < 6; ++c) {
        if (!(fabs(view.pixels_float32[c] - expected[c % 3]) < 1e-6)) {
            fprintf(stderr, "A2B%d component %d: got %.9g, expected %.9g\n",
                    relative, c, view.pixels_float32[c], expected[c % 3]);
            goto end;
        }
    }
    result = EXIT_SUCCESS;
end:
    sixel_frame_unref(frame);
    sixel_cdef_free(profile);
    return result;
}

static int
test_profile_byte_resize_retry(void)
{
    /* Compare stored samples at float precision, including on x87. */
    static float const original_pixels[] = {.4f, .5f, .6f};
    float pixel[3];
    /* Encoded Display P3 to gamma sRGB, rounded directly to RGB888. */
    unsigned char const expected[] = {95, 128, 156, 95, 128, 156};
    sixel_allocator_t *allocator = NULL;
    sixel_frame_t *frame = NULL;
    sixel_frame_interface_t *iface;
    sixel_frame_pixels_view_t view;
    sixel_color_def_t *p3;
    sixel_color_def_t const *original;
    int result = EXIT_FAILURE;

    memcpy(pixel, original_pixels, sizeof(pixel));
    p3 = sixel_cdef_name("display-p3");
    if (p3 == NULL || SIXEL_FAILED(sixel_allocator_new(&allocator,
            profile_frame_malloc, calloc, realloc, free)) ||
        SIXEL_FAILED(sixel_frame_new(&frame, allocator)) ||
        SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixel, 1, 1,
            SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1))) {
        goto end;
    }
    iface = sixel_frame_as_interface(frame);
    if (SIXEL_FAILED(iface->vtbl->set_rgb_definition(iface, p3)) ||
        SIXEL_FAILED(sixel_frame_get_work_pixels(iface, &view))) {
        goto end;
    }
    original = view.rgb_definition;
    fail_six_bytes = 1;
    if (iface->vtbl->resize(iface, 2, 1, SIXEL_RES_NEAREST) !=
            SIXEL_BAD_ALLOCATION ||
        SIXEL_FAILED(sixel_frame_get_work_pixels(iface, &view)) ||
        view.rgb_definition != original || view.pixels_float32 != pixel ||
        view.width != 1 || view.height != 1 ||
        memcmp(pixel, original_pixels, sizeof(pixel)) != 0) {
        goto end;
    }
    fail_six_bytes = 0;
    if (SIXEL_SUCCEEDED(iface->vtbl->resize(iface, 2, 1,
            SIXEL_RES_NEAREST)) &&
        SIXEL_SUCCEEDED(sixel_frame_get_work_pixels(iface, &view)) &&
        view.width == 2 && view.height == 1 && view.rgb_definition == NULL &&
        view.pixelformat == SIXEL_PIXELFORMAT_RGB888 && view.pixels != NULL &&
        memcmp(view.pixels, expected, sizeof(expected)) == 0) {
        result = EXIT_SUCCESS;
    }
end:
    fail_six_bytes = 0;
    sixel_frame_unref(frame);
    sixel_allocator_unref(allocator);
    sixel_cdef_free(p3);
    return result;
}

/* Decode SMPTE-C with exponent 2.2, transform its D65 primaries to
 * sRGB, then apply the sRGB transfer. The byte source has blue 64/255. */
static int
test_smptec_to_srgb(int byte_source, int target)
{
    void *source_pixels;
    int pixelformat;
    sixel_frame_pixels_kind_t kind;
    static double const expected[][3] = {
        {.78376865, .21796907, .06316951},
        {.78402677, .21994704, .23991341}
    };
    static unsigned char const quantized[][3] = {{200, 56, 16}, {200, 56, 61}};
    unsigned char bytes[] = {204, 51, 64};
    float pixel[] = {.8f, .2f, .1f};
    sixel_frame_t *frame = NULL;
    sixel_frame_interface_t *iface;
    sixel_frame_pixels_view_t view;
    int result = EXIT_FAILURE;
    int c;

    source_pixels = byte_source ? (void *)bytes : (void *)pixel;
    pixelformat = byte_source
        ? SIXEL_PIXELFORMAT_RGB888 : SIXEL_PIXELFORMAT_RGBFLOAT32;
    kind = byte_source ? SIXEL_FRAME_PIXELS_U8 : SIXEL_FRAME_PIXELS_FLOAT32;
    if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
        SIXEL_FAILED(sixel_frame_borrow_pixels(frame,
            &(sixel_frame_pixels_request_t){
                source_pixels, NULL, 1, 1,
                pixelformat,
                SIXEL_COLORSPACE_SMPTEC, -1,
                kind
            }))) {
        goto end;
    }
    iface = sixel_frame_as_interface(frame);
    if (SIXEL_FAILED(sixel_frame_convert_workformat(frame, target)) ||
        SIXEL_FAILED(sixel_frame_get_work_pixels(iface, &view)) ||
        view.colorspace != SIXEL_COLORSPACE_SRGB ||
        view.pixelformat != target || view.rgb_definition != NULL) {
        goto end;
    }
    if (target == SIXEL_PIXELFORMAT_RGB888) {
        if (view.pixels == NULL ||
            memcmp(view.pixels, quantized[byte_source], 3) != 0) {
            goto end;
        }
    } else {
        if (view.pixels_float32 == NULL) {
            goto end;
        }
        for (c = 0; c < 3; ++c) {
            if (!(fabs(view.pixels_float32[c] - expected[byte_source][c])
                    < 2e-7)) {
                goto end;
            }
        }
    }
    result = EXIT_SUCCESS;
end:
    sixel_frame_unref(frame);
    return result;
}

/* With identical D65 and blue xy, changing only the blue excitation from
 * a neutral cannot move red or green in sRGB. Blue clips at both ends.
 * The final red-primary sample has no such invariant. */
static int
test_shared_primary(int two_axes)
{
    float samples[] = {
        .5f, .5f, 1, .5f, .5f, 0,
        0x1.fffffep-2f, 0x1.fffffep-2f, 1,
        0x1.000002p-1f, 0x1.000002p-1f, 0,
        1, .5f, .5f
    };
    unsigned char const expected[] = {
        128, 128, 255, 128, 128, 0, 127, 127, 255, 128, 128, 0,
        255, 118, 123
    };
    /* Equal D65 whites and shared red/blue axes make the green row of
     * the linear transform (0,1,0), even with three unequal channels. */
    double const xy[] = {.3127, .329, .64, .33, .21, .71, .15, .06};
    double const gamma[] = {1, 1, 1};
    float two_samples[] = {
        .75f, .5f, .25f, .75f, 0x1.fffffep-2f, .25f,
        .75f, 0x1.000002p-1f, .25f
    };
    unsigned char const two_expected[] = {
        210, 128, 59, 210, 127, 59, 210, 128, 59
    };
    sixel_color_def_t *p3;
    sixel_frame_t *frame = NULL;
    sixel_frame_interface_t *iface;
    sixel_frame_pixels_view_t view;
    int result = EXIT_FAILURE;

    p3 = two_axes ? sixel_cdef_rgb(xy, SIXEL_COLOR_TRC_SRGB, gamma) :
                    sixel_cdef_name("display-p3");
    if (p3 == NULL || SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
        SIXEL_FAILED(sixel_frame_init_borrowed(frame,
            two_axes ? two_samples : samples, two_axes ? 3 : 5, 1,
            SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1))) {
        goto end;
    }
    iface = sixel_frame_as_interface(frame);
    if (SIXEL_SUCCEEDED(iface->vtbl->set_rgb_definition(iface, p3)) &&
        SIXEL_SUCCEEDED(iface->vtbl->set_pixelformat(iface,
            SIXEL_PIXELFORMAT_RGB888)) &&
        SIXEL_SUCCEEDED(sixel_frame_get_work_pixels(iface, &view)) &&
        view.pixels != NULL &&
        memcmp(view.pixels, two_axes ? two_expected : expected,
               two_axes ? sizeof(two_expected) : sizeof(expected)) == 0) {
        result = EXIT_SUCCESS;
    }
end:
    sixel_frame_unref(frame);
    sixel_cdef_free(p3);
    return result;
}

static SIXELSTATUS
check_tga_storage(sixel_frame_t *frame, void *context)
{
    unsigned char const expected[] = {255, 0, 0, 0, 255, 0};
    sixel_frame_interface_t *iface = sixel_frame_as_interface(frame);
    sixel_frame_pixels_view_t view;
    size_t bytes;

    if (SIXEL_FAILED(sixel_frame_get_work_pixels(iface, &view)) ||
        view.width != 2 || view.height != 1 || view.pixels == NULL ||
        view.pixelformat != SIXEL_PIXELFORMAT_RGB888 ||
        memcmp(view.pixels, expected, sizeof(expected)) != 0 ||
        SIXEL_FAILED(iface->vtbl->measure_storage(iface, &bytes)) ||
        bytes != 8) {
        return SIXEL_BAD_INPUT;
    }
    ++*(int *)context;
    return SIXEL_OK;
}

static int
test_tga_storage(void)
{
    /* Uncompressed 2-by-1 BGRA, eight alpha bits, opaque red and green.
     * The loader compacts eight adopted bytes to six logical RGB bytes. */
    unsigned char const wire[] = {
        0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 0, 0, 2, 0, 1, 0, 32, 40,
        0, 0, 255, 255, 0, 255, 0, 255
    };
    sixel_allocator_t *allocator = NULL;
    sixel_loader_t *loader = NULL;
    sixel_chunk_t *chunk = NULL;
    int calls = 0;
    int result = EXIT_FAILURE;

    if (SIXEL_SUCCEEDED(sixel_allocator_new(&allocator, NULL, NULL, NULL,
            NULL)) &&
        SIXEL_SUCCEEDED(sixel_loader_new(&loader, allocator)) &&
        SIXEL_SUCCEEDED(sixel_chunk_create_from_memory(&chunk, wire,
            sizeof(wire), NULL, allocator)) &&
        SIXEL_SUCCEEDED(sixel_loader_setopt(loader,
            SIXEL_LOADER_OPTION_LOADER_ORDER, "builtin!")) &&
        SIXEL_SUCCEEDED(sixel_loader_setopt(loader,
            SIXEL_LOADER_OPTION_CONTEXT, &calls)) &&
        SIXEL_SUCCEEDED(sixel_loader_load_chunk(loader, chunk,
            check_tga_storage)) && calls == 1) {
        result = EXIT_SUCCESS;
    }
    if (chunk != NULL) {
        chunk->vtbl->unref(chunk);
    }
    sixel_loader_unref(loader);
    sixel_allocator_unref(allocator);
    return result;
}

/* The second pixel is transparent. libpng compacts an eight-byte RGBA
 * allocation to six RGB bytes and retains a separate two-byte mask. */
static SIXELSTATUS
check_png_storage(sixel_frame_t *frame, void *context)
{
    unsigned char const expected[] = {255, 0, 0, 0, 255, 0};
    sixel_frame_interface_t *iface = sixel_frame_as_interface(frame);
    sixel_frame_pixels_view_t view;
    sixel_frame_transparency_t coverage;
    size_t bytes;

    if (SIXEL_FAILED(sixel_frame_get_work_pixels(iface, &view)) ||
        view.width != 2 || view.height != 1 || view.pixels == NULL ||
        view.pixelformat != SIXEL_PIXELFORMAT_RGB888 ||
        memcmp(view.pixels, expected, sizeof(expected)) != 0 ||
        SIXEL_FAILED(iface->vtbl->get_transparency(iface, &coverage)) ||
        coverage.transparent_mask_size != 2u ||
        coverage.transparent_mask == NULL ||
        coverage.transparent_mask[0] != 0u ||
        coverage.transparent_mask[1] != 1u ||
        SIXEL_FAILED(iface->vtbl->measure_storage(iface, &bytes)) ||
        bytes != 10u) {
        return SIXEL_BAD_INPUT;
    }
    ++*(int *)context;
    return SIXEL_OK;
}

static int
test_png_storage(void)
{
    static unsigned char const wire[] = {
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00,
        0x00, 0x0d, 0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x02,
        0x00, 0x00, 0x00, 0x01, 0x08, 0x06, 0x00, 0x00, 0x00, 0xf4,
        0x22, 0x7f, 0x8a, 0x00, 0x00, 0x00, 0x0f, 0x49, 0x44, 0x41,
        0x54, 0x78, 0x9c, 0x63, 0xf8, 0xcf, 0xc0, 0xf0, 0x1f, 0x08,
        0x19, 0x00, 0x0f, 0xf9, 0x02, 0xfe, 0x2d, 0x47, 0x81, 0xf4,
        0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42,
        0x60, 0x82,
    };
    sixel_allocator_t *allocator = NULL;
    sixel_loader_t *loader = NULL;
    sixel_chunk_t *chunk = NULL;
    int calls = 0;
    int result = EXIT_FAILURE;

    if (SIXEL_SUCCEEDED(sixel_allocator_new(&allocator, NULL, NULL, NULL,
            NULL)) &&
        SIXEL_SUCCEEDED(sixel_loader_new(&loader, allocator)) &&
        SIXEL_SUCCEEDED(sixel_chunk_create_from_memory(&chunk, wire,
            sizeof(wire), NULL, allocator)) &&
        SIXEL_SUCCEEDED(sixel_loader_setopt(loader,
            SIXEL_LOADER_OPTION_LOADER_ORDER, "libpng!")) &&
        SIXEL_SUCCEEDED(sixel_loader_setopt(loader,
            SIXEL_LOADER_OPTION_CONTEXT, &calls)) &&
        SIXEL_SUCCEEDED(sixel_loader_load_chunk(loader, chunk,
            check_png_storage)) && calls == 1) {
        result = EXIT_SUCCESS;
    }
    if (chunk != NULL) {
        chunk->vtbl->unref(chunk);
    }
    sixel_loader_unref(loader);
    sixel_allocator_unref(allocator);
    return result;
}

static int
test_set_dimension_storage(int height)
{
    unsigned char *pixels = calloc(6u, 1u);
    sixel_frame_t *frame = NULL;
    sixel_frame_interface_t *iface, *clone = NULL;
    size_t bytes;
    int result = EXIT_FAILURE;

    if (pixels == NULL || SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
        SIXEL_FAILED(sixel_frame_init(frame, pixels, height ? 1 : 2,
            height ? 2 : 1, SIXEL_PIXELFORMAT_RGB888, NULL, -1))) {
        goto end;
    }
    pixels = NULL;
    iface = sixel_frame_as_interface(frame);
    if (height) {
        sixel_frame_set_height(frame, 1);
    } else {
        sixel_frame_set_width(frame, 1);
    }
    if (sixel_frame_get_width(frame) == 1 &&
        sixel_frame_get_height(frame) == 1 &&
        SIXEL_SUCCEEDED(iface->vtbl->measure_storage(iface, &bytes)) &&
        bytes == 6u && SIXEL_SUCCEEDED(iface->vtbl->clone(iface, NULL,
            &clone)) &&
        SIXEL_SUCCEEDED(clone->vtbl->measure_storage(clone, &bytes)) &&
        bytes == 3u) {
        result = EXIT_SUCCESS;
    }
end:
    if (clone != NULL) {
        clone->vtbl->unref(clone);
    }
    sixel_frame_unref(frame);
    free(pixels);
    return result;
}

static int
test_packed_rgb_swap(int bits)
{
    /* Red (31), green (21), blue (3), followed by full blue. */
    unsigned char input[] = {0xfa, 0xa3, 0, 0x1f};
    unsigned char swapped[] = {0x1a, 0xbf, 0xf8, 0};
    unsigned char green = bits == 565 ? 84 : 168;
    unsigned char const rgb[] = {248, green, 24, 0, 0, 248};
    int source = bits == 565 ? SIXEL_PIXELFORMAT_RGB565
                             : SIXEL_PIXELFORMAT_RGB555;
    int target = bits == 565 ? SIXEL_PIXELFORMAT_BGR565
                             : SIXEL_PIXELFORMAT_BGR555;
    sixel_frame_t *frame = NULL;
    unsigned char original[sizeof(input)];
    int result = EXIT_FAILURE;

    if (bits == 555) {
        input[0] = 0xfe;
        swapped[0] = 0x8e;
        swapped[2] = 0x7c;
    }
    memcpy(original, input, sizeof(input));
    if (SIXEL_SUCCEEDED(sixel_frame_new(&frame, NULL)) &&
        SIXEL_SUCCEEDED(sixel_frame_init_borrowed(frame, input, 2, 1,
            source, NULL, -1)) &&
        SIXEL_SUCCEEDED(sixel_frame_set_pixelformat(frame, target)) &&
        sixel_frame_format(frame) == target &&
        memcmp(input, original, sizeof(input)) == 0 &&
        memcmp(sixel_frame_get_pixels(frame), swapped, sizeof(swapped)) == 0 &&
        SIXEL_SUCCEEDED(sixel_frame_set_pixelformat(frame, source)) &&
        memcmp(sixel_frame_get_pixels(frame), original, sizeof(original))
            == 0 &&
        SIXEL_SUCCEEDED(sixel_frame_set_pixelformat(frame, target)) &&
        SIXEL_SUCCEEDED(sixel_frame_set_pixelformat(frame,
            SIXEL_PIXELFORMAT_RGB888)) &&
        memcmp(sixel_frame_get_pixels(frame), rgb, sizeof(rgb)) == 0) {
        result = EXIT_SUCCESS;
    }
    sixel_frame_unref(frame);
    return result;
}

/* Distinct red, green and blue test channel order and the green field
 * width. The second pixel checks the exact full-green endpoint. */
static int
test_packed_rgb_matrix(void)
{
    static int const formats[] = {
        SIXEL_PIXELFORMAT_RGB888, SIXEL_PIXELFORMAT_BGR888,
        SIXEL_PIXELFORMAT_RGB555, SIXEL_PIXELFORMAT_BGR555,
        SIXEL_PIXELFORMAT_RGB565, SIXEL_PIXELFORMAT_BGR565
    };
    unsigned char input[][6] = {
        {251, 173, 19, 0, 255, 0}, {19, 173, 251, 0, 255, 0},
        {0x7e, 0xa2, 0x03, 0xe0}, {0x0a, 0xbf, 0x03, 0xe0},
        {0xfd, 0x62, 0x07, 0xe0}, {0x15, 0x7f, 0x07, 0xe0}
    };
    static unsigned char const unpacked[][6] = {
        {248, 168, 16, 0, 248, 0}, {16, 168, 248, 0, 248, 0},
        {248, 172, 16, 0, 252, 0}, {16, 172, 248, 0, 252, 0}
    };
    unsigned char const *expected;
    sixel_frame_t *frame = NULL;
    int source, target, width, ok = 0;

    if (sixel_frame_new(&frame, NULL) != SIXEL_OK) {
        return EXIT_FAILURE;
    }
    for (source = 0; source < 6; ++source) {
        for (target = source < 2 ? 2 : 0; target < 6; ++target) {
            width = target < 2 ? 6 : 4;
            expected = target < 2
                ? unpacked[source < 4 ? target : target + 2]
                : input[target];
            if (sixel_frame_init_borrowed(frame, input[source], 2, 1,
                    formats[source], NULL, -1) != SIXEL_OK ||
                sixel_frame_set_pixelformat(frame, formats[target])
                    != SIXEL_OK ||
                sixel_frame_format(frame) != formats[target] ||
                memcmp(sixel_frame_get_pixels(frame), expected,
                       (size_t)width) != 0) {
                fprintf(stderr, "packed RGB layouts %d to %d\n",
                        formats[source], formats[target]);
                goto done;
            }
        }
    }
    ok = 1;
done:
    sixel_frame_unref(frame);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int
test_packed_rgb_reject(void)
{
    unsigned char gray[] = {128, 128};
    sixel_frame_t *frame = NULL;
    int ok;

    ok = sixel_frame_new(&frame, NULL) == SIXEL_OK &&
        sixel_frame_init_borrowed(frame, gray, 2, 1,
            SIXEL_PIXELFORMAT_G8, NULL, -1) == SIXEL_OK &&
        SIXEL_FAILED(sixel_frame_set_pixelformat(frame,
            SIXEL_PIXELFORMAT_BGR565)) &&
        sixel_frame_format(frame) == SIXEL_PIXELFORMAT_G8 &&
        sixel_frame_get_pixels(frame) == gray &&
        gray[0] == 128 && gray[1] == 128;
    sixel_frame_unref(frame);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int
test_reshape_storage(void)
{
    unsigned char *pixels = calloc(18u, 1u);
    sixel_frame_t *frame = NULL;
    sixel_frame_interface_t *iface;
    size_t bytes;
    int ok;

    if (pixels == NULL || sixel_frame_new(&frame, NULL) != SIXEL_OK ||
        sixel_frame_init(frame, pixels, 2, 3, SIXEL_PIXELFORMAT_RGB888,
                         NULL, -1) != SIXEL_OK) {
        free(pixels);
        sixel_frame_unref(frame);
        return EXIT_FAILURE;
    }
    iface = sixel_frame_as_interface(frame);
    sixel_frame_set_width(frame, 3);
    ok = iface->vtbl->measure_storage(iface, &bytes) == SIXEL_OK &&
        bytes == 18u;
    sixel_frame_set_height(frame, 2);
    ok = ok && iface->vtbl->measure_storage(iface, &bytes) == SIXEL_OK &&
        bytes == 18u && sixel_frame_get_width(frame) == 3 &&
        sixel_frame_get_height(frame) == 2 &&
        sixel_frame_get_pixels(frame) == pixels;
    sixel_frame_unref(frame);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int
test_borrowed_adoption_storage(void)
{
    unsigned char *pixels = calloc(12u, 1u);
    sixel_frame_t *frame = NULL;
    sixel_frame_interface_t *iface;
    size_t bytes;
    int ok;

    if (pixels == NULL || sixel_frame_new(&frame, NULL) != SIXEL_OK ||
        sixel_frame_init_borrowed(frame, pixels, 2, 1,
            SIXEL_PIXELFORMAT_RGB888, NULL, -1) != SIXEL_OK) {
        free(pixels);
        sixel_frame_unref(frame);
        return EXIT_FAILURE;
    }
    iface = sixel_frame_as_interface(frame);
    ok = iface->vtbl->measure_storage(iface, &bytes) == SIXEL_OK &&
        bytes == 6u;
    if (sixel_frame_init(frame, pixels, 4, 1, SIXEL_PIXELFORMAT_RGB888,
                         NULL, -1) == SIXEL_OK) {
        ok = ok && iface->vtbl->measure_storage(iface, &bytes) == SIXEL_OK &&
            bytes == 12u && sixel_frame_get_pixels(frame) == pixels;
    } else {
        free(pixels);
        ok = 0;
    }
    sixel_frame_unref(frame);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int
test_implicit_palette_storage(void)
{
    unsigned char indices[] = {0, 1, 3, 2};
    unsigned char palette[12] = {0};
    sixel_frame_t *frame = NULL;
    sixel_frame_interface_t *iface;
    size_t bytes;
    int ok;

    if (sixel_frame_new(&frame, NULL) != SIXEL_OK ||
        sixel_frame_init_borrowed(frame, indices, 4, 1,
            SIXEL_PIXELFORMAT_PAL8, palette, -1) != SIXEL_OK) {
        sixel_frame_unref(frame);
        return EXIT_FAILURE;
    }
    iface = sixel_frame_as_interface(frame);
    ok = iface->vtbl->measure_storage(iface, &bytes) == SIXEL_OK &&
        bytes == 16u;
    sixel_frame_set_width(frame, 2);
    ok = ok && iface->vtbl->measure_storage(iface, &bytes) == SIXEL_OK &&
        bytes == 16u && sixel_frame_get_palette(frame) == palette &&
        sixel_frame_get_ncolors(frame) == -1;
    sixel_frame_unref(frame);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* Use a distinct exit status so a generic runner failure stays red. */
enum { PALETTE_EXPECTED_FAILURE = 23 };
static int
test_mutable_palette_storage(void)
{
    unsigned char indices[] = {0, 1};
    unsigned char palette[12] = {0};
    sixel_frame_t *frame = NULL;
    sixel_frame_interface_t *iface, *copy = NULL;
    size_t before, after, cloned;
    int result = 2;

    if (sixel_frame_new(&frame, NULL) != SIXEL_OK ||
        sixel_frame_init_borrowed(frame, indices, 2, 1,
            SIXEL_PIXELFORMAT_PAL8, palette, -1) != SIXEL_OK) {
        goto end;
    }
    iface = sixel_frame_as_interface(frame);
    if (iface->vtbl->measure_storage(iface, &before) != SIXEL_OK ||
            before != 8u) {
        goto end;
    }
    indices[1] = 3;
    if (iface->vtbl->measure_storage(iface, &after) != SIXEL_OK ||
        iface->vtbl->clone(iface, NULL, &copy) != SIXEL_OK ||
        copy->vtbl->measure_storage(copy, &cloned) != SIXEL_OK ||
        cloned != 14u) {
        goto end;
    }
    if (after == 14u) {
        result = 0;
    } else if (after == before) {
        result = PALETTE_EXPECTED_FAILURE;
    }
end:
    if (copy != NULL) {
        copy->vtbl->unref(copy);
    }
    sixel_frame_unref(frame);
    return result;
}

static int
test_attached_palette_storage(void)
{
    unsigned char indices[] = {0, 1, 3, 2};
    unsigned char palette[12] = {0};
    sixel_frame_t *frame = NULL;
    sixel_frame_interface_t *iface;
    size_t before, after;
    int result = 2;

    if (sixel_frame_new(&frame, NULL) != SIXEL_OK ||
        sixel_frame_init_borrowed(frame, indices, 4, 1,
            SIXEL_PIXELFORMAT_PAL8, NULL, -1) != SIXEL_OK) {
        goto end;
    }
    sixel_frame_set_palette(frame, palette);
    iface = sixel_frame_as_interface(frame);
    if (iface->vtbl->measure_storage(iface, &before) != SIXEL_OK ||
            before != 16u) {
        goto end;
    }
    sixel_frame_set_width(frame, 2);
    if (iface->vtbl->measure_storage(iface, &after) != SIXEL_OK ||
        sixel_frame_get_palette(frame) != palette ||
        sixel_frame_get_ncolors(frame) != -1) {
        goto end;
    }
    if (after == before) {
        result = 0;
    } else if (after == 10u) {
        result = PALETTE_EXPECTED_FAILURE;
    }
end:
    sixel_frame_unref(frame);
    return result;
}

static int
test_repeated_implicit_palette_init(void)
{
    enum { WIDTH = 1024, HEIGHT = 1024, REPETITIONS = 24 };
    enum { TIMER_PROBES = 16384 };
    int const counts[] = {16, -1};
    double const timing_margin = 0.002;
    unsigned char palette[16 * 3] = {0};
    unsigned char *indices = calloc(WIDTH * HEIGHT, 1u);
    sixel_frame_t *frame = NULL;
    clock_t start, finish;
    double elapsed[2];
    int mode, i;
    int result = 2;

    if (indices == NULL || sixel_frame_new(&frame, NULL) != SIXEL_OK) {
        goto end;
    }
    /* CLOCKS_PER_SEC gives the units, which may be finer than the clock's
     * effective resolution. Require an observed step within half the
     * timing margin before interpreting short samples, including zeros. */
    start = clock();
    finish = start;
    for (i = 0; i < TIMER_PROBES && finish == start; ++i) {
        finish = clock();
    }
    if (start == (clock_t)-1 || finish == (clock_t)-1 || finish <= start ||
        (double)(finish - start) / CLOCKS_PER_SEC > timing_margin / 2.0) {
        result = 77;
        goto end;
    }
    /* Keep the highest used entry below 255 so a full scan cannot exit
     * early. Warm each mode before timing repeated same-pointer calls. */
    indices[WIDTH * HEIGHT - 1] = 15;
    for (mode = 0; mode < 2; ++mode) {
        if (sixel_frame_init_borrowed(frame, indices, WIDTH, HEIGHT,
                SIXEL_PIXELFORMAT_PAL8, palette, counts[mode]) != SIXEL_OK) {
            goto end;
        }
        start = clock();
        for (i = 0; i < REPETITIONS; ++i) {
            if (sixel_frame_init_borrowed(frame, indices, WIDTH, HEIGHT,
                    SIXEL_PIXELFORMAT_PAL8, palette, counts[mode])
                        != SIXEL_OK) {
                goto end;
            }
        }
        finish = clock();
        if (start == (clock_t)-1 || finish == (clock_t)-1 || finish < start) {
            result = 77;
            goto end;
        }
        elapsed[mode] = (double)(finish - start) / CLOCKS_PER_SEC;
    }
    /* Compare CPU time to the same input with an explicit count. Leave
     * enough margin for timer granularity and fast constant-time paths. */
    fprintf(stderr, "PAL8 reinit CPU seconds: declared=%.6f implicit=%.6f\n",
            elapsed[0], elapsed[1]);
    result = elapsed[1] > 8.0 * elapsed[0] + timing_margin
        ? PALETTE_EXPECTED_FAILURE : 0;
end:
    sixel_frame_unref(frame);
    free(indices);
    return result;
}

static int
test_strip_storage(int format, unsigned char *background)
{
    unsigned char *pixels;
    sixel_frame_t *frame = NULL;
    sixel_frame_interface_t *iface, *clone = NULL;
    size_t bytes;
    int result = EXIT_FAILURE;

    pixels = calloc(16, 4);
    if (pixels == NULL || SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
        SIXEL_FAILED(sixel_frame_init(frame, pixels, 4, 4, format,
                                      NULL, -1))) {
        goto end;
    }
    pixels = NULL;
    iface = sixel_frame_as_interface(frame);
    /* The adopted 64-byte buffer survives compaction and crop. Only a new
     * allocation for the clone or for conversion can discard that slack. */
    if (SIXEL_SUCCEEDED(sixel_frame_strip_alpha(frame, background)) &&
        SIXEL_SUCCEEDED(iface->vtbl->measure_storage(iface, &bytes)) &&
        bytes == 64 && SIXEL_SUCCEEDED(iface->vtbl->clip(iface, 1, 1, 1, 1)) &&
        SIXEL_SUCCEEDED(iface->vtbl->measure_storage(iface, &bytes)) &&
        bytes == 64 && SIXEL_SUCCEEDED(iface->vtbl->clone(iface, NULL,
            &clone)) &&
        SIXEL_SUCCEEDED(clone->vtbl->measure_storage(clone, &bytes)) &&
        bytes == 3 && SIXEL_SUCCEEDED(iface->vtbl->set_pixelformat(iface,
            SIXEL_PIXELFORMAT_RGBFLOAT32)) &&
        SIXEL_SUCCEEDED(iface->vtbl->measure_storage(iface, &bytes)) &&
        bytes == 3 * sizeof(float)) {
        result = EXIT_SUCCESS;
    }
end:
    if (clone != NULL) {
        clone->vtbl->unref(clone);
    }
    sixel_frame_unref(frame);
    free(pixels);
    return result;
}

static int
test_byte_tie(char const *name)
{
    double const d60_xy[] = {.32168, .33767, .64, .33, .30, .60, .15, .06};
    double const gamma[] = {1, 1, 1};
    /* 0.5 * 255 is exactly 127.5. Its two adjacent binary32 values
     * straddle that same boundary without a tolerance. */
    float pixels[] = {
        .5f, .5f, .5f,
        0x1.fffffep-2f, .5f, 0x1.000002p-1f,
        0x1.fffffep-2f, 0x1.fffffep-2f, 0x1.fffffep-2f,
        0x1.000002p-1f, 0x1.000002p-1f, 0x1.000002p-1f
    };
    int const d60 = strcmp(name, "d60") == 0;
    /* For the unequal second triplet, D60-to-D65 Bradford adaptation
     * gives green * 255 = 127.499999968, just below the rounding tie. */
    unsigned char green = d60 ? 127 : 128;
    unsigned char const expected[] = {
        128, 128, 128, 127, green, 128,
        127, 127, 127, 128, 128, 128
    };
    sixel_color_def_t *definition;
    sixel_frame_t *frame = NULL;
    sixel_frame_interface_t *iface;
    sixel_frame_pixels_view_t view;
    int result = EXIT_FAILURE;

    if (d60) {
        definition = sixel_cdef_rgb(d60_xy, SIXEL_COLOR_TRC_SRGB, gamma);
    } else {
        definition = sixel_cdef_name(name);
    }
    if (definition == NULL || SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
        SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels, 4, 1,
            SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1))) {
        goto end;
    }
    iface = sixel_frame_as_interface(frame);
    if (SIXEL_SUCCEEDED(iface->vtbl->set_rgb_definition(iface, definition)) &&
        SIXEL_SUCCEEDED(iface->vtbl->set_pixelformat(iface,
            SIXEL_PIXELFORMAT_RGB888)) &&
        SIXEL_SUCCEEDED(sixel_frame_get_work_pixels(iface, &view)) &&
        view.pixelformat == SIXEL_PIXELFORMAT_RGB888 && view.pixels != NULL &&
        memcmp(view.pixels, expected, sizeof(expected)) == 0) {
        result = EXIT_SUCCESS;
    }
end:
    sixel_frame_unref(frame);
    sixel_cdef_free(definition);
    return result;
}

/* A public format round trip preserves both borrowed bytes and color state. */
static int
check_public_float_layout(sixel_frame_t *frame)
{
    sixel_frame_interface_t *iface;
    sixel_frame_pixels_view_t before, after;
    float saved[3];
    int layout;

    iface = sixel_frame_as_interface(frame);
    layout = sixel_frame_get_pixelformat(frame);
    if (layout != SIXEL_PIXELFORMAT_RGBFLOAT32 ||
            iface->vtbl->get_pixels(iface, &before) != SIXEL_OK ||
            before.pixelformat != layout || !before.pixels_float32) {
        return 0;
    }
    memcpy(saved, before.pixels_float32, sizeof(saved));
    return sixel_frame_set_pixelformat(frame, layout) == SIXEL_OK &&
        iface->vtbl->set_pixelformat(iface, layout) == SIXEL_OK &&
        iface->vtbl->get_pixels(iface, &after) == SIXEL_OK &&
        after.pixelformat == layout && after.colorspace == before.colorspace &&
        after.rgb_definition == before.rgb_definition &&
        after.pixels_float32 == before.pixels_float32 &&
        memcmp(saved, after.pixels_float32, sizeof(saved)) == 0;
}

static int
test_profile_frame(char const *mode)
{
    static double const srgb_red[] = {0.43604, 0.22249, 0.01392};
    float pixels[6] = {0, 1, 0, 1, 0, 0};
    double input[3];
    double xyz[3];
    double const *expected;
    sixel_color_def_t *definition;
    sixel_frame_t *frame;
    sixel_frame_interface_t *iface;
    sixel_frame_interface_t *clone;
    sixel_frame_pixels_view_t view;
    size_t footprint;
    int icc;
    int srgb;
    int retry;
    int c;
    int result;

    icc = strcmp(mode, "icc") == 0;
    srgb = strcmp(mode, "srgb") == 0;
    retry = strcmp(mode, "retry") == 0;
    result = EXIT_FAILURE;
    frame = NULL;
    clone = NULL;
    definition = icc ? sixel_color_def_new_icc(color_icc, sizeof(color_icc)) :
        sixel_color_def_new_named(srgb ? "srgb" : "display-p3");
    if (definition == NULL || SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
        SIXEL_FAILED(sixel_frame_init_rgbf32_borrowed(
            frame, pixels, 2, 1, definition))) {
        goto end;
    }
    if (!check_public_float_layout(frame)) {
        goto end;
    }
    iface = sixel_frame_as_interface(frame);
    if (icc && (SIXEL_FAILED(iface->vtbl->measure_storage(iface,
            &footprint)) || footprint < sizeof(color_icc) + sizeof(pixels))) {
        goto end;
    }
    sixel_color_def_free(definition);
    definition = NULL;
    if (retry) {
        /* The first green pixel converts before the invalid red sample. */
        pixels[3] = NAN;
        if (SIXEL_SUCCEEDED(iface->vtbl->set_pixelformat(iface,
                SIXEL_PIXELFORMAT_LINEARRGBFLOAT32)) ||
            SIXEL_FAILED(sixel_frame_get_work_pixels(iface, &view)) ||
            view.rgb_definition == NULL || view.pixels_float32 != pixels ||
            pixels[0] != 0 || pixels[1] != 1 || pixels[2] != 0 ||
            !isnan(pixels[3])) {
            goto end;
        }
        pixels[3] = 1;
        if (SIXEL_FAILED(iface->vtbl->set_pixelformat(iface,
                SIXEL_PIXELFORMAT_LINEARRGBFLOAT32)) ||
            SIXEL_FAILED(sixel_frame_get_work_pixels(iface, &view)) ||
            view.rgb_definition != NULL || view.pixels_float32 == NULL) {
            goto end;
        }
        for (c = 0; c < 3; ++c) {
            if (!(fabs(view.pixels_float32[3 + c] -
                    color_vectors[0][9 + c]) < 1e-5)) {
                goto end;
            }
        }
        result = EXIT_SUCCESS;
        goto end;
    }
    if (SIXEL_FAILED(iface->vtbl->clone(iface, NULL, &clone))) {
        goto end;
    }
    sixel_frame_unref(frame);
    frame = NULL;
    /* Mirroring moves red from the last column to the first. */
    if (SIXEL_FAILED(loader_frame_apply_orientation((sixel_frame_t *)clone,
            2)) || SIXEL_FAILED(clone->vtbl->clip(clone, 0, 0, 1, 1)) ||
        SIXEL_FAILED(sixel_frame_get_work_pixels(clone, &view)) ||
        view.colorspace != SIXEL_FRAME_COLORSPACE_PROFILE_RGB ||
        view.rgb_definition == NULL || view.pixels_float32 == NULL) {
        goto end;
    }
    for (c = 0; c < 3; ++c) {
        input[c] = view.pixels_float32[c];
    }
    expected = srgb ? srgb_red : color_vectors[0] + 6;
    if (!sixel_cdef_to_xyz(view.rgb_definition, input, xyz,
                           SIXEL_CMS_INTENT_RELATIVE_COLORIMETRIC)) {
        goto end;
    }
    for (c = 0; c < 3; ++c) {
        if (!(fabs(xyz[c] - expected[c]) < 0.0002)) {
            goto end;
        }
    }
    if (SIXEL_FAILED(clone->vtbl->set_pixelformat(clone,
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32)) ||
        SIXEL_FAILED(sixel_frame_get_work_pixels(clone, &view)) ||
        view.rgb_definition != NULL || view.pixels_float32 == NULL) {
        goto end;
    }
    for (c = 0; c < 3; ++c) {
        if (!(fabs(view.pixels_float32[c] -
                (srgb ? input[c] : color_vectors[0][9 + c])) <
                (icc ? 5e-4 : 1e-5))) {
            goto end;
        }
    }
    result = EXIT_SUCCESS;
end:
    sixel_color_def_free(definition);
    sixel_frame_unref(frame);
    if (clone != NULL) {
        clone->vtbl->unref(clone);
    }
    return result;
}

static int
test_rgbf32_public_retry(void)
{
    static double const expected[] = {0.4821, 0.5, 0.41245};
    float gray[3] = {0.5f, 0.5f, 0.5f};
    float replacement[3] = {1.0f, 0.0f, 0.0f};
    double sample[3] = {0.5, 0.5, 0.5};
    double xyz[3];
    sixel_frame_t *frame = NULL;
    sixel_color_def_t *linear;
    sixel_color_def_t *gamma;
    sixel_frame_interface_t *iface;
    sixel_frame_pixels_view_t view;
    int channel;
    int result = EXIT_FAILURE;

    linear = sixel_color_def_new_named("srgb-linear");
    gamma = sixel_color_def_new_named("srgb");
    if (linear == NULL || gamma == NULL ||
            SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
            SIXEL_FAILED(sixel_frame_init_rgbf32_borrowed(
                frame, gray, 1, 1, linear))) {
        goto end;
    }
    if (!check_public_float_layout(frame)) {
        goto end;
    }
    sixel_color_def_free(linear);
    linear = NULL;
    /* A failed replacement must retain both the borrow and its definition. */
    iface = sixel_frame_as_interface(frame);
    if (SIXEL_SUCCEEDED(sixel_frame_init_rgbf32_borrowed(
                frame, replacement, 0, 1, gamma)) ||
            SIXEL_FAILED(sixel_frame_get_work_pixels(iface, &view)) ||
            view.width != 1 || view.height != 1 ||
            view.pixels_float32 != gray || view.rgb_definition == NULL ||
            !sixel_cdef_to_xyz(view.rgb_definition, sample, xyz,
                               SIXEL_CMS_INTENT_RELATIVE_COLORIMETRIC)) {
        goto end;
    }
    for (channel = 0; channel < 3; ++channel) {
        if (!(fabs(xyz[channel] - expected[channel]) < 1e-5)) {
            goto end;
        }
    }
    if (SIXEL_FAILED(sixel_frame_init_rgbf32_borrowed(
                frame, replacement, 1, 1, gamma)) ||
            SIXEL_FAILED(sixel_frame_get_work_pixels(iface, &view)) ||
            view.pixels_float32 != replacement ||
            !sixel_cdef_same_rgb(view.rgb_definition, gamma)) {
        goto end;
    }
    /* Internal opponent coordinates share the public float storage layout. */
    if (SIXEL_FAILED(sixel_frame_borrow_pixels(frame,
                &(sixel_frame_pixels_request_t){
                    gray, NULL, 1, 1, SIXEL_PIXELFORMAT_RGBFLOAT32,
                    SIXEL_COLORSPACE_OKLAB, -1, SIXEL_FRAME_PIXELS_FLOAT32
                })) ||
            sixel_frame_get_pixelformat(frame) !=
                SIXEL_PIXELFORMAT_RGBFLOAT32 ||
            sixel_frame_format(frame) != SIXEL_PIXELFORMAT_OKLABFLOAT32 ||
            !check_public_float_layout(frame)) {
        goto end;
    }
    gray[0] = gray[1] = gray[2] = .25f;
    if (sixel_frame_borrow_pixels(frame, &(sixel_frame_pixels_request_t){
            gray, NULL, 1, 1, SIXEL_PIXELFORMAT_RGBFLOAT32,
            SIXEL_COLORSPACE_SRGB_LINEAR, -1, SIXEL_FRAME_PIXELS_FLOAT32
        }) != SIXEL_OK || !check_public_float_layout(frame) ||
            sixel_frame_convert_pixels(frame, SIXEL_PIXELFORMAT_RGBFLOAT32,
                                       SIXEL_COLORSPACE_SRGB) != SIXEL_OK ||
            fabs(gray[0] - 0.5370987) >= 1e-7) {
        goto end;
    }
    result = EXIT_SUCCESS;
end:
    sixel_color_def_free(gamma);
    sixel_color_def_free(linear);
    sixel_frame_unref(frame);
    return result;
}

static int
test_rgbf32_owned_slice(void)
{
    static float const expected[] = {0, 1, 0, 1, 0, 0};
    float replacement[] = {0, 0, 1};
    float *pixels;
    sixel_frame_t *frame = NULL;
    sixel_color_def_t *linear, *gamma;
    sixel_frame_interface_t *iface;
    sixel_frame_pixels_request_t request;
    sixel_frame_pixels_view_t before, after;
    int offset;
    int result = EXIT_FAILURE;

    linear = sixel_color_def_new_named("srgb-linear");
    gamma = sixel_color_def_new_named("srgb");
    if (!linear || !gamma || sixel_frame_new(&frame, NULL) != SIXEL_OK) {
        goto end;
    }
    pixels = malloc(sizeof(expected));
    if (pixels == NULL) {
        goto end;
    }
    memcpy(pixels, expected, sizeof(expected));
    iface = sixel_frame_as_interface(frame);
    request = (sixel_frame_pixels_request_t){
        pixels, NULL, 2, 1, SIXEL_PIXELFORMAT_RGBFLOAT32,
        SIXEL_COLORSPACE_SRGB_LINEAR, -1, SIXEL_FRAME_PIXELS_FLOAT32
    };
    if (iface->vtbl->init_pixels(iface, &request) != SIXEL_OK) {
        free(pixels);
        goto end;
    }
    if (iface->vtbl->set_rgb_definition(iface, linear) != SIXEL_OK ||
            iface->vtbl->get_pixels(iface, &before) != SIXEL_OK) {
        goto end;
    }
    for (offset = 0; offset < 6; offset += 3) {
        if (sixel_frame_init_rgbf32_borrowed(
                frame, pixels + offset, 1, 1, gamma) != SIXEL_BAD_ARGUMENT ||
            iface->vtbl->get_pixels(iface, &after) != SIXEL_OK ||
            after.width != 2 || after.height != 1 ||
            after.pixelformat != before.pixelformat ||
            after.colorspace != before.colorspace ||
            after.rgb_definition != before.rgb_definition ||
            after.pixels_float32 != pixels ||
            memcmp(after.pixels_float32, expected, sizeof(expected)) != 0) {
            goto end;
        }
    }
    if (sixel_frame_init_rgbf32_borrowed(frame, replacement, 1, 1, gamma) !=
            SIXEL_OK ||
        iface->vtbl->get_pixels(iface, &after) != SIXEL_OK ||
        after.pixels_float32 != replacement ||
        !sixel_cdef_same_rgb(after.rgb_definition, gamma)) {
        goto end;
    }
    result = EXIT_SUCCESS;
end:
    sixel_frame_unref(frame);
    sixel_color_def_free(gamma);
    sixel_color_def_free(linear);
    return result;
}

static int
test_rgbf32_input_direction(void)
{
    unsigned char bytes[sizeof(color_paired_lut)];
    float gray[3] = {.25f, .25f, .25f};
    float unused[6] = {0};
    sixel_frame_t *frame = NULL;
    sixel_color_def_t *srgb, *forward, *reverse;
    sixel_frame_interface_t *iface;
    sixel_frame_pixels_view_t before, after;
    int result = EXIT_FAILURE;

    /* Preserve each tag's data and replace only its table signature. */
    memcpy(bytes, color_paired_lut, sizeof(bytes));
    memcpy(bytes + 12, "spac", 4);
    memcpy(bytes + 132, "priv", 4);
    reverse = sixel_color_def_new_icc(bytes, sizeof(bytes));
    memcpy(bytes + 132, "A2B0", 4);
    memcpy(bytes + 144, "priv", 4);
    forward = sixel_color_def_new_icc(bytes, sizeof(bytes));
    srgb = sixel_color_def_new_named("srgb");
    if (!reverse || !forward || !srgb ||
            sixel_frame_new(&frame, NULL) != SIXEL_OK ||
            sixel_frame_init_rgbf32_borrowed(frame, gray, 1, 1, srgb)) {
        goto end;
    }
    iface = sixel_frame_as_interface(frame);
    if (iface->vtbl->get_pixels(iface, &before) != SIXEL_OK ||
            sixel_frame_init_rgbf32_borrowed(frame, unused, 2, 1, reverse)
                != SIXEL_BAD_INPUT ||
            iface->vtbl->set_rgb_definition(iface, reverse)
                != SIXEL_BAD_INPUT ||
            iface->vtbl->get_pixels(iface, &after) != SIXEL_OK ||
            after.width != 1 || after.height != 1 ||
            after.pixels_float32 != gray || gray[0] != .25f ||
            after.rgb_definition != before.rgb_definition ||
            sixel_frame_init_rgbf32_borrowed(frame, gray, 1, 1, forward) ||
            sixel_frame_set_pixelformat(frame, SIXEL_PIXELFORMAT_RGB888)) {
        goto end;
    }
    result = EXIT_SUCCESS;
end:
    sixel_frame_unref(frame);
    sixel_color_def_free(srgb);
    sixel_color_def_free(forward);
    sixel_color_def_free(reverse);
    return result;
}

/* Return the paint decision and active register color of each cell. */
static int
read_four_cells(unsigned char *samples, int format, unsigned char *palette,
                int key, int option, char const *value, unsigned char rgba[16])
{
    sixel_frame_t *frame = NULL;
    sixel_encoder_t *encoder = NULL;
    sixel_output_t *output = NULL;
    playback_test_buffer_t wire = {0};
    unsigned char *indices = NULL, *mask = NULL, *colors = NULL;
    unsigned int flags;
    int w, h, n, i;
    int result = EXIT_FAILURE;

    if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
        SIXEL_FAILED(sixel_frame_init_borrowed(frame, samples, 4, 1, format,
            palette, palette != NULL ? 2 : -1)) ||
        SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
        SIXEL_FAILED(sixel_output_new(&output, playback_test_write,
            &wire, NULL)) ||
        SIXEL_FAILED(sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_GPU_POLICY,
            option == SIXEL_OPTFLAG_MONOCHROME ? "auto" : "off")) ||
        (option && SIXEL_FAILED(sixel_encoder_setopt(encoder, option,
                                                     value)))) {
        goto end;
    }
    sixel_frame_set_transparent(frame, key);
    if (SIXEL_FAILED(sixel_encoder_encode_frame(encoder, frame, output)) ||
        SIXEL_FAILED(sixel_decode_raw_with_options_mask(wire.bytes,
            (int)wire.size, 0, &indices, &mask, &w, &h, &colors, &n,
            &flags, NULL)) || w != 4 || h != 1) {
        goto end;
    }
    if (option == SIXEL_OPTFLAG_MONOCHROME &&
        memchr(wire.bytes, '#', wire.size) != NULL) {
        goto end;
    }
    memset(rgba, 0, 16);
    for (i = 0; i < 4; ++i) {
        if (mask == NULL || mask[i]) {
            /* Monochrome paints with the terminal foreground, whose RGB
             * color is independent of the encoded image. */
            if (option != SIXEL_OPTFLAG_MONOCHROME) {
                if (indices[i] >= n) {
                    goto end;
                }
                memcpy(rgba + 4 * i, colors + 3 * indices[i], 3);
            }
            rgba[4 * i + 3] = 255;
        }
    }
    result = EXIT_SUCCESS;
end:
    free(indices);
    free(mask);
    free(colors);
    free(wire.bytes);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    sixel_frame_unref(frame);
    return result;
}

static int
test_keyed_registers(void)
{
    unsigned char samples[] = {0, 1, 0, 1};
    unsigned char palette[] = {255, 0, 0, 0, 255, 0};
    unsigned char expected[16], actual[16];
    int key, i;

    for (key = 0; key < 2; ++key) {
        memset(expected, 0, sizeof(expected));
        for (i = 1 - key; i < 4; i += 2) {
            expected[4 * i + samples[i]] = 255;
            expected[4 * i + 3] = 255;
        }
        if (read_four_cells(samples, SIXEL_PIXELFORMAT_PAL8, palette, key,
                            0, NULL, actual) ||
            memcmp(expected, actual, sizeof(actual)) != 0) {
            return EXIT_FAILURE;
        }
    }
    return EXIT_SUCCESS;
}

static int
test_packed_float_output(void)
{
    static int const formats[] = {
        SIXEL_PIXELFORMAT_RGB555, SIXEL_PIXELFORMAT_BGR555,
        SIXEL_PIXELFORMAT_RGB565, SIXEL_PIXELFORMAT_BGR565
    };
    unsigned char rgb[] = {248, 0, 0, 0, 128, 0, 0, 0, 248, 80, 64, 32};
    unsigned char packed[8], expected[16], actual[16];
    unsigned int r, g, b, word;
    int i, f, mode, option;
    char const *value;

    for (mode = 0; mode < 2; ++mode) {
        option = mode ? SIXEL_OPTFLAG_PRECISION_POLICY :
                        SIXEL_OPTFLAG_WORKING_COLORSPACE;
        value = mode ? "float32" : "oklab";
        if (read_four_cells(rgb, SIXEL_PIXELFORMAT_RGB888, NULL, -1,
                            option, value, expected)) {
            return EXIT_FAILURE;
        }
        for (f = 0; f < 4; ++f) {
            for (i = 0; i < 4; ++i) {
                r = rgb[i * 3 + (f % 2 ? 2 : 0)] >> 3;
                b = rgb[i * 3 + (f % 2 ? 0 : 2)] >> 3;
                g = rgb[i * 3 + 1] >> (f < 2 ? 3 : 2);
                word = (r << (f < 2 ? 10 : 11)) | (g << 5) | b;
                packed[2 * i] = (unsigned char)(word >> 8);
                packed[2 * i + 1] = (unsigned char)word;
            }
            if (read_four_cells(packed, formats[f], NULL, -1, option,
                                value, actual) ||
                memcmp(actual, expected, sizeof(actual)) != 0) {
                return EXIT_FAILURE;
            }
        }
    }
    return EXIT_SUCCESS;
}

static int
test_linear_unpack(int format, int bits)
{
    unsigned char rows[] = {0, 0};
    unsigned char palette[] = {128, 128, 128, 64, 64, 64};
    unsigned char expected[] = {0, 1, 1, 0};
    unsigned char bytes[] = {188, 188, 188, 137, 137, 137,
                             137, 137, 137, 188, 188, 188};
    sixel_frame_t *frame = NULL;
    int result = EXIT_FAILURE;

    rows[0] = (unsigned char)(1 << (8 - 2 * bits));
    rows[1] = (unsigned char)(1 << (8 - bits));
    if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
        SIXEL_FAILED(sixel_frame_borrow_pixels(frame,
            &(sixel_frame_pixels_request_t){
                rows, palette, 2, 2, format, SIXEL_COLORSPACE_SRGB_LINEAR,
                2, SIXEL_FRAME_PIXELS_U8
            }))) {
        goto end;
    }
    if (SIXEL_SUCCEEDED(sixel_frame_set_pixelformat(frame,
            SIXEL_PIXELFORMAT_PAL8)) &&
        sixel_frame_format(frame) == SIXEL_PIXELFORMAT_PAL8 &&
        sixel_frame_get_colorspace(frame) == SIXEL_COLORSPACE_SRGB_LINEAR &&
        sixel_frame_get_palette(frame) == palette &&
        memcmp(sixel_frame_get_pixels(frame), expected, 4) == 0 &&
        SIXEL_SUCCEEDED(sixel_frame_set_pixelformat(frame,
            SIXEL_PIXELFORMAT_RGB888)) &&
        memcmp(sixel_frame_get_pixels(frame), bytes, sizeof(bytes)) == 0) {
        result = EXIT_SUCCESS;
    }
end:
    sixel_frame_unref(frame);
    return result;
}

static int
test_float_bgr(int profile)
{
    float source[] = {1, .5f, .25f, .25f, 1, 0};
    unsigned char expected[] = {64, 128, 255, 0, 255, 64};
    sixel_frame_t *frame = NULL;
    sixel_color_def_t *srgb = NULL;
    sixel_frame_interface_t *iface;
    int result = EXIT_FAILURE;

    if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
        SIXEL_FAILED(sixel_frame_init_borrowed(frame, source, 2, 1,
            SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1))) {
        goto end;
    }
    iface = sixel_frame_as_interface(frame);
    if (profile) {
        srgb = sixel_cdef_name("srgb");
        if (srgb == NULL ||
            SIXEL_FAILED(iface->vtbl->set_rgb_definition(iface, srgb))) {
            goto end;
        }
    }
    if (SIXEL_SUCCEEDED(sixel_frame_set_pixelformat(frame,
            SIXEL_PIXELFORMAT_BGR888)) &&
        sixel_frame_format(frame) == SIXEL_PIXELFORMAT_BGR888 &&
        memcmp(sixel_frame_get_pixels(frame), expected, sizeof(expected)) == 0
        && sixel_frame_get_colorspace(frame) == SIXEL_COLORSPACE_SRGB) {
        result = EXIT_SUCCESS;
    }
end:
    sixel_cdef_free(srgb);
    sixel_frame_unref(frame);
    return result;
}

static int
test_permuted_primaries(int linear, int cycle)
{
    double xy[] = {.3127, .329, .30, .60, .64, .33, .15, .06};
    double gamma[] = {1, 1, 1};
    float samples[] = {1, .5f, .5f, 1e7f, .25f, .5f, 1, 1e-25f, 0};
    unsigned char encoded[] = {128, 255, 128, 64, 255, 128, 0, 255, 0};
    unsigned char cycled[] = {128, 255, 128, 128, 255, 64, 0, 255, 0};
    float expected[] = {.5f, 1, .5f, .25f, 1e7f, .5f, 1e-25f, 1, 0};
    float rotated[] = {.5f, 1, .5f, .5f, 1e7f, .25f, 0, 1, 1e-25f};
    sixel_color_def_t *def;
    sixel_frame_t *frame = NULL;
    sixel_frame_interface_t *iface;
    int result = EXIT_FAILURE;

    /* G,B,R source order also exercises a permutation with no fixed axis. */
    if (cycle) {
        xy[4] = .15;
        xy[5] = .06;
        xy[6] = .64;
        xy[7] = .33;
    }
    def = sixel_cdef_rgb(xy, linear ? SIXEL_COLOR_TRC_POWER :
                                     SIXEL_COLOR_TRC_SRGB, gamma);
    if (def != NULL && SIXEL_SUCCEEDED(sixel_frame_new(&frame, NULL)) &&
        SIXEL_SUCCEEDED(sixel_frame_init_borrowed(frame, samples, 3, 1,
            SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1))) {
        iface = sixel_frame_as_interface(frame);
        if (SIXEL_SUCCEEDED(iface->vtbl->set_rgb_definition(iface, def)) &&
            SIXEL_SUCCEEDED(iface->vtbl->set_pixelformat(iface, linear ?
                SIXEL_PIXELFORMAT_LINEARRGBFLOAT32 :
                SIXEL_PIXELFORMAT_RGB888))) {
            result = linear ? memcmp(sixel_frame_get_pixels(frame),
                cycle ? rotated : expected, sizeof(expected)) :
                memcmp(sixel_frame_get_pixels(frame),
                       cycle ? cycled : encoded, sizeof(encoded));
        }
    }
    sixel_cdef_free(def);
    sixel_frame_unref(frame);
    return result != 0;
}

static int
test_palette_recount(int count_first)
{
    unsigned char *indices = calloc(1, 1), *palette = calloc(4, 3);
    unsigned char *replacement;
    sixel_frame_t *frame = NULL;
    sixel_frame_interface_t *iface;
    size_t bytes;
    int result = EXIT_FAILURE;

    if (indices == NULL || palette == NULL ||
        SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
        SIXEL_FAILED(sixel_frame_init(frame, indices, 1, 1,
            SIXEL_PIXELFORMAT_PAL8, count_first ? NULL : palette,
            count_first ? -1 : 4))) {
        free(indices);
        free(palette);
        goto end;
    }
    if (count_first) {
        sixel_frame_set_ncolors(frame, 4);
        sixel_frame_set_palette(frame, palette);
    }
    iface = sixel_frame_as_interface(frame);
    if (SIXEL_FAILED(iface->vtbl->measure_storage(iface, &bytes)) ||
        bytes != 13) {
        goto end;
    }
    sixel_frame_set_ncolors(frame, 1);
    if (sixel_frame_get_ncolors(frame) == 1 &&
        sixel_frame_get_palette(frame) == palette &&
        SIXEL_SUCCEEDED(iface->vtbl->measure_storage(iface, &bytes)) &&
        bytes == 13) {
        result = EXIT_SUCCESS;
    }
    if (count_first && result == EXIT_SUCCESS) {
        /* A smaller replacement must drop the old allocation extent. */
        replacement = calloc(1, 3);
        result = EXIT_FAILURE;
        if (replacement != NULL) {
            sixel_frame_set_palette(frame, replacement);
            free(palette);
            if (SIXEL_SUCCEEDED(iface->vtbl->measure_storage(iface, &bytes))
                && bytes == 4) {
                result = EXIT_SUCCESS;
            }
        }
    }
end:
    sixel_frame_unref(frame);
    return result;
}

static int
test_byte_bgr(int indexed)
{
    unsigned char rgb[] = {255, 0, 0, 23, 47, 89};
    unsigned char indices[] = {0, 1};
    unsigned char expected[] = {0, 0, 255, 89, 47, 23};
    unsigned char original[] = {255, 0, 0, 23, 47, 89};
    sixel_frame_t *frame = NULL;
    int ok;

    ok = SIXEL_SUCCEEDED(sixel_frame_new(&frame, NULL)) &&
        SIXEL_SUCCEEDED(sixel_frame_init_borrowed(frame,
            indexed ? indices : rgb, 2, 1, indexed ? SIXEL_PIXELFORMAT_PAL8 :
            SIXEL_PIXELFORMAT_RGB888, indexed ? rgb : NULL, indexed ? 2 : -1))
        && SIXEL_SUCCEEDED(sixel_frame_set_pixelformat(frame,
            SIXEL_PIXELFORMAT_BGR888)) &&
        sixel_frame_format(frame) == SIXEL_PIXELFORMAT_BGR888 &&
        memcmp(sixel_frame_get_pixels(frame), expected, sizeof(expected)) == 0
        && memcmp(rgb, original, sizeof(rgb)) == 0;
    sixel_frame_unref(frame);
    return !ok;
}

static int
test_padded_output(void)
{
    static int const formats[] = {
        SIXEL_PIXELFORMAT_XRGB8888, SIXEL_PIXELFORMAT_RGBX8888,
        SIXEL_PIXELFORMAT_XBGR8888, SIXEL_PIXELFORMAT_BGRX8888
    };
    unsigned char rgb[] = {255, 0, 0, 0, 255, 0, 0, 0, 255, 80, 64, 32};
    unsigned char padded[16], expected[16], actual[16];
    int options[] = {0, SIXEL_OPTFLAG_PRECISION_POLICY,
                     SIXEL_OPTFLAG_WORKING_COLORSPACE};
    char const *values[] = {NULL, "float32", "oklab"};
    int f, i, j, mode, offset;

    for (mode = 0; mode < 3; ++mode) {
        if (read_four_cells(rgb, SIXEL_PIXELFORMAT_RGB888, NULL, -1,
                            options[mode], values[mode], expected)) {
            return 1;
        }
        for (f = 0; f < 4; ++f) {
            offset = 1 - f % 2;
            for (i = 0; i < 4; ++i) {
                /* Zero and full padding must both leave the cell opaque. */
                padded[4 * i + (offset ? 0 : 3)] = (unsigned char)(85 * i);
                for (j = 0; j < 3; ++j) {
                    padded[4 * i + offset + j] =
                        rgb[3 * i + (f < 2 ? j : 2 - j)];
                }
            }
            if (read_four_cells(padded, formats[f], NULL, -1, options[mode],
                                values[mode], actual) ||
                memcmp(expected, actual, sizeof(actual)) != 0) {
                return 1;
            }
        }
    }
    return 0;
}

static int
test_auto_mono(void)
{
    unsigned char samples[] = {0, 1, 0, 1};
    unsigned char palette[] = {0, 0, 0, 255, 255, 255};
    unsigned char rgba[16];

    /* read_four_cells also checks for a register-free monochrome stream. */
    return read_four_cells(samples, SIXEL_PIXELFORMAT_PAL8, palette, -1,
        SIXEL_OPTFLAG_MONOCHROME, NULL, rgba) ||
        rgba[3] != 0 || rgba[7] != 255 || rgba[11] != 0 || rgba[15] != 255;
}

static int
test_collinear_primary(int adjacent)
{
    double xy[] = {.3127, .329, .504, .438, .30, .60, .15, .06};
    double gamma[] = {1, 1, 1};
    float samples[257 * 3];
    unsigned char const *out;
    sixel_color_def_t *def;
    sixel_frame_interface_t *iface;
    sixel_frame_t *frame = NULL;
    int i, ok;

    /* The red xy is exactly 3/5 sRGB red plus 2/5 sRGB green in binary64.
     * Its blue contribution is zero at every intensity. One step below
     * that edge has a positive blue contribution, visible at high gain. */
    if (adjacent) {
        xy[3] = nextafter(xy[3], 0.0);
    }
    for (i = 0; i <= 256; ++i) {
        samples[i * 3] = i == 256 ? 1e8f : (float)i / 255.0f;
        samples[i * 3 + 1] = .25f;
        samples[i * 3 + 2] = .5f;
    }
    def = sixel_cdef_rgb(xy, SIXEL_COLOR_TRC_SRGB, gamma);
    ok = def != NULL && SIXEL_SUCCEEDED(sixel_frame_new(&frame, NULL)) &&
        SIXEL_SUCCEEDED(sixel_frame_init_borrowed(frame, samples, 257, 1,
            SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1));
    if (ok) {
        iface = sixel_frame_as_interface(frame);
        ok = SIXEL_SUCCEEDED(iface->vtbl->set_rgb_definition(iface, def)) &&
            SIXEL_SUCCEEDED(sixel_frame_set_pixelformat(frame,
                SIXEL_PIXELFORMAT_RGB888));
    }
    if (ok) {
        out = sixel_frame_get_pixels(frame);
        for (i = adjacent ? 256 : 0; i <= 256; ++i) {
            ok = ok && out[3 * i + 2] == (adjacent ? 255 : 128);
        }
    }
    sixel_cdef_free(def);
    sixel_frame_unref(frame);
    return !ok;
}

static int
test_ycbcr422_definition(void)
{
    unsigned char pixels[4] = {16, 128, 235, 128};
    sixel_frame_t *first = NULL, *next = NULL;
    sixel_color_def_t *rgb = NULL;
    sixel_color_def_t const *a = NULL, *b = NULL;
    sixel_ycbcr_def_t *definition = NULL, *invalid = NULL;
    size_t stride;
    int encoding, range, location, pass = 0;

    rgb = sixel_color_def_new_named("srgb-linear");
    invalid = sixel_ycbcr_def_new(0, SIXEL_YCBCR_RANGE_LIMITED,
                                  SIXEL_CHROMA_LOCATION_LEFT, rgb);
    if (invalid != NULL) {
        goto end;
    }
    invalid = sixel_ycbcr_def_new(SIXEL_YCBCR_ENCODING_BT709, 0,
                                  SIXEL_CHROMA_LOCATION_LEFT, rgb);
    if (invalid != NULL) {
        goto end;
    }
    invalid = sixel_ycbcr_def_new(SIXEL_YCBCR_ENCODING_BT709,
                                  SIXEL_YCBCR_RANGE_FULL, 0, rgb);
    if (invalid != NULL) {
        goto end;
    }
    invalid = sixel_ycbcr_def_new(SIXEL_YCBCR_ENCODING_BT709,
        SIXEL_YCBCR_RANGE_FULL, SIXEL_CHROMA_LOCATION_CENTER, NULL);
    if (invalid != NULL) {
        goto end;
    }
    definition = sixel_ycbcr_def_new(SIXEL_YCBCR_ENCODING_BT709,
        SIXEL_YCBCR_RANGE_FULL, SIXEL_CHROMA_LOCATION_CENTER, rgb);
    sixel_color_def_free(rgb);
    rgb = NULL;
    if (SIXEL_FAILED(sixel_frame_new(&first, NULL)) ||
        SIXEL_FAILED(sixel_frame_new(&next, NULL)) ||
        SIXEL_FAILED(sixel_frame_init_ycbcr422_borrowed(first, pixels, 4,
            2, 1, 4, SIXEL_PIXELFORMAT_YUYV, definition)) ||
        SIXEL_FAILED(sixel_frame_init_ycbcr422_borrowed(next, pixels, 4,
            2, 1, 4, SIXEL_PIXELFORMAT_UYVY, definition))) {
        goto end;
    }
    sixel_ycbcr_def_free(definition);
    definition = NULL;
    if (SIXEL_FAILED(sixel_frame_get_ycbcr422(first, &stride, &encoding,
            &range, &location, &a)) ||
        SIXEL_FAILED(sixel_frame_get_ycbcr422(next, &stride, &encoding,
            &range, &location, &b)) || a == NULL || a != b) {
        goto end;
    }
    sixel_frame_unref(first);
    first = NULL;
    rgb = sixel_color_def_new_named("srgb-linear");
    pass = rgb != NULL && sixel_cdef_same_rgb(rgb, b);
end:
    sixel_ycbcr_def_free(invalid);
    sixel_ycbcr_def_free(definition);
    sixel_frame_unref(first);
    sixel_frame_unref(next);
    sixel_color_def_free(rgb);
    return pass ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* Both layouts use unaligned borrowed bytes. The sentinel sits between
 * rows. Nine bytes expose exactly the last active byte. */
static int
test_ycbcr422(char const *scenario, int format, int encoding, int range,
              int location)
{
    unsigned char data[] = {0, 1, 2, 3, 4, 99, 5, 6, 7, 8, 99};
    unsigned char expected[] = {1, 2, 3, 4, 5, 6, 7, 8};
    unsigned char *input = data + 1;
    sixel_frame_t *frame = NULL;
    sixel_frame_interface_t *iface, *clone = NULL;
    sixel_color_def_t *rgb = NULL;
    sixel_color_def_t const *got_rgb = NULL;
    sixel_ycbcr_def_t *definition = NULL;
    sixel_frame_t *observed;
    sixel_encoder_t *encoder = NULL;
    size_t stride, retained, compact;
    unsigned char *owned;
    int got_encoding, got_range, got_location;
    int pass = 0;

    rgb = sixel_color_def_new_named("display-p3");
    definition = sixel_ycbcr_def_new(encoding, range, location, rgb);
    if (definition == NULL || SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
        SIXEL_FAILED(sixel_frame_init_ycbcr422_borrowed(frame, input, 9,
            2, 2, 5, format, definition))) {
        goto end;
    }
    iface = sixel_frame_as_interface(frame);
    observed = frame;
    if (strcmp(scenario, "borrow") == 0) {
        if (sixel_frame_get_pixels(frame) != input) {
            goto end;
        }
        input[5] = 77;
        if (sixel_frame_get_pixels(frame)[5] != 77) {
            goto end;
        }
    } else if (strcmp(scenario, "clone") == 0) {
        /* A caller may retain final padding as well as the row gap. */
        if (SIXEL_FAILED(sixel_frame_init_ycbcr422_borrowed(frame, input, 10,
                2, 2, 5, format, definition)) ||
            SIXEL_FAILED(iface->vtbl->clone(iface, NULL, &clone)) ||
            SIXEL_FAILED(iface->vtbl->measure_storage(iface, &retained)) ||
            SIXEL_FAILED(clone->vtbl->measure_storage(clone, &compact)) ||
            retained != compact + 2u) {
            goto end;
        }
        observed = (sixel_frame_t *)clone;
        owned = sixel_frame_get_pixels(observed);
        /* A borrow of a clone-owned row would dangle after replacement. */
        if (SIXEL_SUCCEEDED(sixel_frame_init_ycbcr422_borrowed(observed,
                owned + 4, 4, 2, 1, 4, format, definition)) ||
            sixel_frame_get_pixels(observed) != owned) {
            goto end;
        }
        sixel_frame_unref(frame);
        frame = NULL;
        memset(input, 0, 9);
        if (memcmp(sixel_frame_get_pixels(observed), expected, 8) != 0) {
            goto end;
        }
    } else if (strcmp(scenario, "bounds") == 0) {
        /* One row never reads its trailing padding. Two rows need nine
         * bytes here, although the packed pixel count is only eight. */
        if (SIXEL_FAILED(sixel_frame_init_ycbcr422_borrowed(frame, input,
                4, 2, 1, SIZE_MAX, format, definition)) ||
            SIXEL_SUCCEEDED(sixel_frame_init_ycbcr422_borrowed(frame, input,
                8, 2, 2, 5, format, definition)) ||
            SIXEL_SUCCEEDED(sixel_frame_init_ycbcr422_borrowed(frame, input,
                9, 2, 2, 3, format, definition)) ||
            SIXEL_SUCCEEDED(sixel_frame_init_ycbcr422_borrowed(frame, input,
                SIZE_MAX, 2, 2, SIZE_MAX, format, definition)) ||
            SIXEL_SUCCEEDED(sixel_frame_init_ycbcr422_borrowed(frame, input,
                9, 3, 1, 9, format, definition)) ||
            SIXEL_FAILED(sixel_frame_init_ycbcr422_borrowed(frame, input,
                9, 2, 2, 5, format, definition))) {
            goto end;
        }
        sixel_frame_set_width(frame, 4);
        sixel_frame_set_height(frame, 3);
    } else if (strcmp(scenario, "retry") == 0) {
        /* Invalid replacement has different storage, pitch and geometry. */
        sixel_frame_set_delay(frame, 45);
        if (SIXEL_SUCCEEDED(sixel_frame_init_ycbcr422_borrowed(frame,
                expected, 3, 2, 1, 4, SIXEL_PIXELFORMAT_YUYV, definition)) ||
            sixel_frame_get_delay(frame) != 45 ||
            sixel_frame_get_pixels(frame) != input ||
            memcmp(data, (unsigned char[]){0,1,2,3,4,99,5,6,7,8}, 10) ||
            SIXEL_SUCCEEDED(sixel_frame_init_borrowed(frame, input, 2, 2,
                format, NULL, -1))) {
            goto end;
        }
    } else if (strcmp(scenario, "unsupported") == 0) {
        if (SIXEL_FAILED(sixel_encoder_new(&encoder, NULL)) ||
            sixel_encoder_encode_bytes(encoder, input, 2, 2, format,
                NULL, -1) != SIXEL_BAD_ARGUMENT ||
            SIXEL_SUCCEEDED(sixel_frame_clip(frame, 0, 0, 1, 1)) ||
            SIXEL_SUCCEEDED(sixel_frame_resize(frame, 1, 1,
                                               SIXEL_RES_NEAREST)) ||
            SIXEL_SUCCEEDED(sixel_frame_set_pixelformat(frame,
                SIXEL_PIXELFORMAT_RGB888))) {
            goto end;
        }
    } else {
        goto end;
    }
    /* The getter remains valid even after the stream owner is released. */
    sixel_ycbcr_def_free(definition);
    definition = NULL;
    if (SIXEL_FAILED(sixel_frame_get_ycbcr422(observed, &stride,
            &got_encoding, &got_range, &got_location, &got_rgb)) ||
        stride != (clone == NULL ? 5u : 4u) ||
        got_encoding != encoding || got_range != range ||
        got_location != location || !sixel_cdef_same_rgb(got_rgb, rgb) ||
        sixel_frame_get_pixelformat(observed) != format ||
        sixel_frame_get_colorspace(observed) != SIXEL_FRAME_COLORSPACE_YCBCR ||
        sixel_frame_get_width(observed) != 2 ||
        sixel_frame_get_height(observed) != 2) {
        goto end;
    }
    if (strcmp(scenario, "retry") == 0) {
        if (SIXEL_FAILED(sixel_frame_init_borrowed(frame, expected, 1, 1,
                SIXEL_PIXELFORMAT_RGB888, NULL, -1)) ||
            sixel_frame_get_pixels(frame) != expected ||
            SIXEL_SUCCEEDED(sixel_frame_get_ycbcr422(frame, &stride,
                &got_encoding, &got_range, &got_location, &got_rgb))) {
            goto end;
        }
    }
    pass = 1;
end:
    if (clone != NULL) {
        clone->vtbl->unref(clone);
    }
    sixel_encoder_unref(encoder);
    sixel_frame_unref(frame);
    sixel_ycbcr_def_free(definition);
    sixel_color_def_free(rgb);
    return pass ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int
test_ycbcr422_rgb(int matrix, int limited, int centered, int uyvy, int nearest)
{
    /* Chroma values reconstructed by hand in code units relative to 128.
     * Include both row edges and a change of direction in the second row. */
    static double const chroma[][8][2] = {
        {{-112,112}, {-8,16}, {96,-80}, {96,-80},
         {32,-32}, {-16,16}, {-64,64}, {-64,64}},
        {{-112,112}, {-60,64}, {44,-32}, {96,-80},
         {32,-32}, {8,-8}, {-40,40}, {-64,64}},
        {{-112,112}, {-112,112}, {96,-80}, {96,-80},
         {32,-32}, {32,-32}, {-64,64}, {-64,64}}
    };
    /* Reference inverse matrices applied to normalized color differences.
     * Green coefficients are expanded independently of the frame kernel. */
    static double const inverse[][4] = {
        {1.402, -0.344136286, -0.714136286, 1.772},
        {1.5748, -0.187324273, -0.468124273, 1.8556},
        {1.4746, -0.164553127, -0.571353127, 1.8814}
    };
    static int const encodings[] = {
        SIXEL_YCBCR_ENCODING_BT601, SIXEL_YCBCR_ENCODING_BT709,
        SIXEL_YCBCR_ENCODING_BT2020_NCL
    };
    static unsigned char const luma[] = {0, 255, 126, 235, 16, 126, 192, 64};
    unsigned char yuyv[] = {99, 0, 16, 255, 240, 126, 224, 235, 48, 255, 1,
                             16, 160, 126, 96, 192, 64, 64, 192};
    unsigned char uyvy_bytes[] = {99, 16, 0, 240, 255, 224, 126, 48, 235,
                                   255, 1, 160, 16, 96, 126, 64, 192, 192, 64};
    unsigned char *input = uyvy ? uyvy_bytes : yuyv;
    unsigned char original[19];
    sixel_frame_t *frame = NULL;
    sixel_frame_interface_t *iface;
    sixel_frame_pixels_view_t view;
    sixel_color_def_t *rgb;
    sixel_ycbcr_def_t *definition;
    double y, cb, cr, expected[3];
    int i, c, location, range, format;
    int pass = 0;

    memcpy(original, input, sizeof(original));
    location = centered ? SIXEL_CHROMA_LOCATION_CENTER :
                          SIXEL_CHROMA_LOCATION_LEFT;
    range = limited ? SIXEL_YCBCR_RANGE_LIMITED : SIXEL_YCBCR_RANGE_FULL;
    format = uyvy ? SIXEL_PIXELFORMAT_UYVY : SIXEL_PIXELFORMAT_YUYV;
    rgb = sixel_color_def_new_named("display-p3");
    definition = sixel_ycbcr_def_new(encodings[matrix], range, location, rgb);
    if (definition == NULL || SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
        SIXEL_FAILED(sixel_frame_init_ycbcr422_borrowed(frame, input + 1, 18,
            4, 2, 10, format, definition))) {
        goto end;
    }
    sixel_ycbcr_def_free(definition);
    definition = NULL;
    iface = sixel_frame_as_interface(frame);
    if (SIXEL_FAILED(sixel_frame_set_pixelformat(frame,
                     SIXEL_PIXELFORMAT_RGBFLOAT32)) ||
        SIXEL_FAILED(iface->vtbl->get_pixels(iface, &view)) ||
        view.pixels_float32 == NULL ||
        view.pixelformat != SIXEL_PIXELFORMAT_RGBFLOAT32 ||
        view.colorspace != SIXEL_FRAME_COLORSPACE_PROFILE_RGB ||
        !sixel_cdef_same_rgb(view.rgb_definition, rgb) ||
        memcmp(input, original, sizeof(original)) != 0) {
        goto end;
    }
    for (i = 0; i < 8; ++i) {
        y = limited ? (luma[i] - 16) / 219.0 : luma[i] / 255.0;
        cb = chroma[nearest ? 2 : centered][i][0] / (limited ? 224.0 : 255.0);
        cr = chroma[nearest ? 2 : centered][i][1] / (limited ? 224.0 : 255.0);
        expected[0] = y + inverse[matrix][0] * cr;
        expected[1] = y + inverse[matrix][1] * cb + inverse[matrix][2] * cr;
        expected[2] = y + inverse[matrix][3] * cb;
        for (c = 0; c < 3; ++c) {
            if (!(fabs(view.pixels_float32[3 * i + c] - expected[c]) < 2e-7)) {
                fprintf(stderr, "pixel %d channel %d: %.9g expected %.9g\n",
                        i, c, view.pixels_float32[3 * i + c], expected[c]);
                goto end;
            }
        }
    }
    pass = 1;
end:
    sixel_frame_unref(frame);
    sixel_ycbcr_def_free(definition);
    sixel_color_def_free(rgb);
    return pass ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int fail_ycbcr422_pixels;

static void *
ycbcr422_test_malloc(size_t size)
{
    return fail_ycbcr422_pixels && size == 6 * sizeof(float) ?
        NULL : malloc(size);
}

static int
test_ycbcr422_rgb_transfer(int retry)
{
    unsigned char input[] = {126, 128, 235, 128};
    unsigned char const expected[] = {188, 188, 188, 255, 255, 255};
    sixel_frame_t *frame = NULL;
    sixel_color_def_t *rgb;
    sixel_ycbcr_def_t *definition;
    sixel_allocator_t *allocator = NULL;
    SIXELSTATUS status;
    int pass = 0;

    rgb = sixel_color_def_new_named("srgb-linear");
    definition = sixel_ycbcr_def_new(SIXEL_YCBCR_ENCODING_BT709,
        SIXEL_YCBCR_RANGE_LIMITED, SIXEL_CHROMA_LOCATION_CENTER, rgb);
    if (definition == NULL ||
        SIXEL_FAILED(sixel_allocator_new(&allocator, ycbcr422_test_malloc,
                                         calloc, realloc, free)) ||
        SIXEL_FAILED(sixel_frame_new(&frame, allocator)) ||
        SIXEL_FAILED(sixel_frame_init_ycbcr422_borrowed(frame, input, 4,
            2, 1, SIZE_MAX, SIXEL_PIXELFORMAT_YUYV, definition))) {
        goto end;
    }
    if (retry) {
        fail_ycbcr422_pixels = 1;
        status = sixel_frame_set_pixelformat(frame,
                                              SIXEL_PIXELFORMAT_RGBFLOAT32);
        fail_ycbcr422_pixels = 0;
        if (status != SIXEL_BAD_ALLOCATION ||
            sixel_frame_get_pixels(frame) != input ||
            sixel_frame_get_pixelformat(frame) != SIXEL_PIXELFORMAT_YUYV ||
            sixel_frame_get_colorspace(frame) != SIXEL_FRAME_COLORSPACE_YCBCR) {
            goto end;
        }
    }
    /* Mid-gray comes from linear 110/219. Applying the sRGB transfer once
     * at the subsequent byte conversion rounds to 188. The white is 255. */
    pass = SIXEL_SUCCEEDED(sixel_frame_set_pixelformat(frame,
                SIXEL_PIXELFORMAT_RGBFLOAT32)) &&
           SIXEL_SUCCEEDED(sixel_frame_set_pixelformat(frame,
                SIXEL_PIXELFORMAT_RGB888)) &&
           memcmp(sixel_frame_get_pixels(frame), expected, sizeof(expected))
               == 0;
end:
    sixel_frame_unref(frame);
    sixel_allocator_unref(allocator);
    sixel_ycbcr_def_free(definition);
    sixel_color_def_free(rgb);
    return pass ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int
test_ycbcr422_rgb_cases(char const *scenario)
{
    sixel_color_def_t *rgb;
    sixel_ycbcr_def_t *def;
    int matrix, limited, centered, uyvy, nearest, rejected;

    if (strcmp(scenario, "transfer") == 0 || strcmp(scenario, "retry") == 0) {
        return test_ycbcr422_rgb_transfer(strcmp(scenario, "retry") == 0);
    }
    if (strcmp(scenario, "invalid") == 0) {
        rgb = sixel_color_def_new_named("srgb");
        def = sixel_ycbcr_def_new(
            SIXEL_YCBCR_ENCODING_BT709, SIXEL_YCBCR_RANGE_LIMITED,
            SIXEL_CHROMA_LOCATION_LEFT, rgb);
        rejected = rgb != NULL && def == NULL;

        sixel_ycbcr_def_free(def);
        sixel_color_def_free(rgb);
        return rejected ? EXIT_SUCCESS : EXIT_FAILURE;
    }
    nearest = strcmp(scenario, "nearest") == 0;
    matrix = strcmp(scenario, "bt601") == 0 ? 0 :
             strcmp(scenario, "bt2020") == 0 ? 2 : 1;
    for (limited = 0; limited < 2; ++limited) {
        for (centered = 0; centered < 2; ++centered) {
            for (uyvy = 0; uyvy < 2; ++uyvy) {
                if (test_ycbcr422_rgb(matrix, limited, centered, uyvy,
                                      nearest) != EXIT_SUCCESS) {
                    return EXIT_FAILURE;
                }
            }
        }
    }
    return EXIT_SUCCESS;
}

enum {
    Y422_DIRECT,
    Y422_CROP,
    Y422_PLAYBACK,
    Y422_PALETTE,
    Y422_PALETTE_REJECT,
    Y422_DELTA,
    Y422_DEFERRED_DELTA
};

typedef struct y422_capture {
    playback_test_buffer_t stream;
    size_t painted[2];
    int completed;
    int failed;
    int deferred_prepared;
} y422_capture_t;

static void
y422_playback_done(sixel_playback_result_t const *result, void *private)
{
    y422_capture_t *capture = private;

    if (result->status != SIXEL_OK || capture->completed >= 2) {
        capture->failed = 1;
        return;
    }
    capture->painted[capture->completed++] = result->painted_pixels;
}

/* The reference has manually interpolated chroma codes per pixel and
 * independent BT.709 inverse matrix coefficients. Pre-crop the reference
 * in RGB, so it also detects resetting chroma phase at an odd crop origin. */
static SIXELSTATUS
y422_fixture(sixel_frame_t **frame, int layout, int crop, int variant,
             sixel_ycbcr_def_t *definition, sixel_color_def_t *rgb,
             unsigned char bytes[18], float samples[24],
             sixel_ycbcr422_counts_t *operations)
{
    static unsigned char const yuyv[] = {
        70, 112, 106, 150, 130, 160, 175, 108, 255, 0,
        84, 144, 110, 100, 142, 116, 160, 150
    };
    static double const cb_codes[] = {-16, -4, 20, 32, 16, 9, -5, -12};
    static double const cr_codes[] = {22, 11.5, -9.5, -20, -28, -15.5, 9.5, 22};
    unsigned char luma[] = {70, 106, 130, 175, 84, 110, 142, 160};
    float *out = samples;
    double y, cb, cr;
    int i, offset;
    SIXELSTATUS status;

    status = sixel_frame_new(frame, NULL);
    if (status != SIXEL_OK) {
        return status;
    }
    memcpy(bytes, yuyv, sizeof(yuyv));
    if (variant == 1) {
        luma[1] = 152;
    }
    for (i = 0; i < 8; ++i) {
        if (variant == 2) {
            luma[i] += 30;
        }
        offset = 10 * (i / 4) + 2 * (i % 4);
        bytes[offset] = luma[i];
        if (layout == SIXEL_PIXELFORMAT_UYVY) {
            bytes[offset] = yuyv[offset + 1];
            bytes[offset + 1] = luma[i];
        }
        if (crop && i % 4 == 0) {
            continue;
        }
        y = luma[i] / 255.0;
        cb = cb_codes[i] / 255.0;
        cr = cr_codes[i] / 255.0;
        *out++ = (float)(y + 1.5748 * cr);
        *out++ = (float)(y - 0.18732427293 * cb - 0.46812427293 * cr);
        *out++ = (float)(y + 1.8556 * cb);
    }
    if (layout != SIXEL_PIXELFORMAT_RGBFLOAT32) {
        sixel_frame_observe_ycbcr422(*frame, operations);
        return sixel_frame_init_ycbcr422_borrowed(*frame, bytes, sizeof(yuyv),
            4, 2, 10, layout, definition);
    }
    return sixel_frame_init_rgbf32_borrowed(*frame, samples,
                                            crop ? 3 : 4, 2, rgb);
}

static void
y422_deferred_prepared(void *context, uint64_t sequence, SIXELSTATUS status)
{
    y422_capture_t *capture = context;

    /* The ordinary prepare-ahead helper starts with sequence two. */
    if (sequence == 1 && status == SIXEL_OK) {
        capture->deferred_prepared = 1;
    }
}

static int
y422_capture_run(y422_capture_t *capture, int layout, int mode, int ahead)
{
    sixel_frame_t *image[2] = {NULL, NULL};
    sixel_frame_t *source = NULL;
    sixel_color_def_t *rgb;
    sixel_ycbcr_def_t *definition, *different = NULL;
    sixel_encoder_t *encoder = NULL;
    sixel_output_t *output = NULL;
    sixel_playback_t *playback = NULL;
    sixel_playback_config_t config;
    sixel_playback_options_t options;
    sixel_playback_stats_t stats = {0};
    sixel_ycbcr422_counts_t operations[3] = {{0}};
    unsigned char bytes[3][18] = {{0}}, original[3][18];
    float samples[3][24];
    struct sixel_encoder_prepare_test_hooks hooks = {0};
    SIXELSTATUS status;
    int i, j, luma, count, packed, deferred, pass = 0;

    packed = layout != SIXEL_PIXELFORMAT_RGBFLOAT32;
    count = mode >= Y422_PLAYBACK ? 2 : 1;
    deferred = mode == Y422_DEFERRED_DELTA;
    rgb = sixel_color_def_new_named("srgb");
    definition = sixel_ycbcr_def_new(SIXEL_YCBCR_ENCODING_BT709,
        SIXEL_YCBCR_RANGE_FULL, SIXEL_CHROMA_LOCATION_CENTER, rgb);
    if (definition == NULL || sixel_encoder_new(&encoder, NULL) ||
        sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_GPU_POLICY,
                             deferred ? "auto" : "off") ||
        sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_PALETTE_TYPE, "rgb") ||
        sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_COLORS, "16") ||
        sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_DIFFUSION, "none") ||
        sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_SAMPLING_POLICY,
                             deferred ? "adaptive-grid" : "full-frame") ||
        sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_THREADING_POLICY,
            ahead ? "greedy:B2:prepare=ahead" : "greedy:B2:prepare=serial") ||
        sixel_output_new(&output, playback_test_write, &capture->stream,
                         NULL)) {
        goto end;
    }
    if (mode == Y422_CROP && packed &&
        sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_CROP, "3x2+1+0")) {
        goto end;
    }
    if ((mode == Y422_DELTA || deferred) &&
        sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_UPDATE_POLICY,
                             "delta:T0")) {
        goto end;
    }
    if (deferred) {
        if (sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_PRECISION_POLICY,
                                 "float32")) {
            goto end;
        }
        hooks.context = capture;
        hooks.complete = y422_deferred_prepared;
        encoder->prepare_test_hooks = &hooks;
    }
    for (i = 0; i < count; ++i) {
        if (y422_fixture(&image[i], layout, mode == Y422_CROP && !packed,
                         i, definition, rgb, bytes[i], samples[i],
                         &operations[i])) {
            goto end;
        }
        if (deferred) {
            /* Opaque neutral luma changes uniformly between the frames. */
            luma = 100 + 20 * i;
            for (j = 0; j < 24; ++j) {
                samples[i][j] = luma / 255.0f;
            }
            for (j = 0; j < 18; ++j) {
                bytes[i][j] = j % 2 == 0 ? luma : 128;
            }
        }
        sixel_frame_set_frame_no(image[i], i);
        sixel_frame_set_multiframe(image[i], count > 1);
    }
    if (mode == Y422_PALETTE_REJECT) {
        different = sixel_ycbcr_def_new(SIXEL_YCBCR_ENCODING_BT709,
            SIXEL_YCBCR_RANGE_LIMITED, SIXEL_CHROMA_LOCATION_CENTER, rgb);
        if (different == NULL) {
            goto end;
        }
    }
    if ((mode == Y422_PALETTE || mode == Y422_PALETTE_REJECT) &&
        y422_fixture(&source, layout, 0, 2,
                      different != NULL ? different : definition,
                      rgb, bytes[2], samples[2], &operations[2])) {
        goto end;
    }
    memcpy(original, bytes, sizeof(original));

    if (mode >= Y422_PLAYBACK) {
        if (sixel_playback_config_init(&config, sizeof(config)) ||
            sixel_playback_options_init(&options, sizeof(options))) {
            goto end;
        }
        config.done = y422_playback_done;
        config.done_context = capture;
        if (sixel_playback_new(&playback, encoder, output, &config, NULL)) {
            goto end;
        }
        options.palette_source = source;
        if (mode == Y422_PALETTE_REJECT) {
            status = sixel_playback_submit(playback, image[0],
                                           SIXEL_PLAYBACK_WAIT, -1, &options);
            stats.struct_size = sizeof(stats);
            if (status != SIXEL_BAD_ARGUMENT ||
                sixel_playback_get_stats(playback, &stats) ||
                stats.depth != 0 ||
                stats.submitted != 0 || capture->completed != 0 ||
                capture->stream.size != 0) {
                goto end;
            }
            options.palette_source = NULL;
        }
        for (i = 0; i < count; ++i) {
            if (sixel_playback_submit(playback, image[i], SIXEL_PLAYBACK_WAIT,
                                       -1, &options)) {
                goto end;
            }
        }
        if (sixel_playback_drain(playback, -1) || capture->failed ||
            capture->completed != count) {
            goto end;
        }
    } else if (sixel_encoder_encode_frame(encoder, image[0], output)) {
        goto end;
    }

    if (packed) {
        for (i = 0; i < count; ++i) {
            if (operations[i].packed_copies != 0 ||
                operations[i].rgb_conversions != 1) {
                fprintf(stderr,
                        "Image %d: %u packed copies, %u conversions\n",
                        i, (unsigned)operations[i].packed_copies,
                        (unsigned)operations[i].rgb_conversions);
                goto end;
            }
            if (sixel_frame_get_pixels(image[i]) != bytes[i] ||
                sixel_frame_get_pixelformat(image[i]) != layout ||
                memcmp(bytes[i], original[i], sizeof(bytes[i])) != 0) {
                goto end;
            }
        }
        /* The same palette source receives one conversion per admission.
         * A range mismatch must leave its bytes and counters untouched. */
        if (source != NULL &&
            (operations[2].packed_copies != 0 ||
             operations[2].rgb_conversions != (mode == Y422_PALETTE ? 2 : 0) ||
             sixel_frame_get_pixels(source) != bytes[2] ||
             sixel_frame_get_pixelformat(source) != layout ||
             memcmp(bytes[2], original[2], sizeof(bytes[2])) != 0)) {
            goto end;
        }
    }
    pass = capture->stream.size > 0;
end:
    if (!pass) {
        fprintf(stderr, "packed 4:2:2 %d (layout %d): %s\n", mode, layout,
                sixel_helper_get_additional_message());
    }
    sixel_playback_unref(playback);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    sixel_frame_unref(image[0]);
    sixel_frame_unref(image[1]);
    sixel_frame_unref(source);
    sixel_ycbcr_def_free(different);
    sixel_ycbcr_def_free(definition);
    sixel_color_def_free(rgb);
    return pass;
}

static int
y422_capture_pixels(y422_capture_t const *capture, unsigned char rgba[2][32],
                    int width, int count)
{
    sixel_sequence_decoder_t *decoder = NULL;
    unsigned char const *pixels;
    int actual_width, height, i;
    size_t end;
    SIXELSTATUS status;
    int pass = 0;

    if (sixel_sequence_open(&decoder, capture->stream.bytes,
                            capture->stream.size, 1, NULL)) {
        goto end;
    }
    for (i = 0; i < count; ++i) {
        status = sixel_sequence_next(decoder, &pixels, &actual_width,
                                     &height, &end);
        if (status != SIXEL_OK || actual_width != width || height != 2) {
            fprintf(stderr, "Decode frame %d: %s\n", i,
                    sixel_helper_get_additional_message());
            goto end;
        }
        memcpy(rgba[i], pixels, (size_t)width * 2u * 4u);
    }
    pass = sixel_sequence_next(decoder, &pixels, &actual_width, &height,
                                &end) == SIXEL_FALSE;
    if (!pass) {
        fprintf(stderr, "Expected end of capture: %s\n",
                sixel_helper_get_additional_message());
    }
end:
    sixel_sequence_close(decoder);
    return pass;
}

static int
test_y422_deferred_delta(int layout)
{
#if SIXEL_ENABLE_THREADS && SIXEL_ENCODER_PREPARE_AHEAD_SUPPORTED
    y422_capture_t capture = {0};
    unsigned char pixels[2][32];
    int i, j, pass = 0;

    if (!y422_capture_run(&capture, layout, Y422_DEFERRED_DELTA, 1) ||
        !capture.deferred_prepared ||
        !y422_capture_pixels(&capture, pixels, 4, 2)) {
        goto end;
    }
    /* SIXEL registers round each channel to an integral percentage. */
    for (i = 0; i < 2; ++i) {
        for (j = 0; j < 32; ++j) {
            if (j % 4 == 3 ? pixels[i][j] != 255 :
                abs(pixels[i][j] - (100 + 20 * i)) > 2) {
                goto end;
            }
        }
    }
    pass = 1;
end:
    free(capture.stream.bytes);
    return pass ? EXIT_SUCCESS : EXIT_FAILURE;
#else
    (void)layout;
    return 77;
#endif
}

static int
test_y422_encode(char const *mode_arg)
{
    static char const *const names[] = {
        "encode", "odd-crop", "playback", "palette-source",
        "palette-mismatch", "delta"
    };
    y422_capture_t packed = {0}, reference = {0}, control = {0};
    unsigned char actual[2][32] = {{0}}, expected[2][32] = {{0}};
    unsigned char without_palette[2][32] = {{0}};
    int mode, width, count, layout, reference_mode, ahead;
    int pass = 0;

    for (mode = 0; mode <= Y422_DELTA; ++mode) {
        if (strcmp(mode_arg, names[mode]) == 0) {
            break;
        }
    }
    if (mode > Y422_DELTA) {
        return EXIT_FAILURE;
    }
    width = mode == Y422_CROP ? 3 : 4;
    count = mode >= Y422_PLAYBACK ? 2 : 1;
    reference_mode = mode == Y422_PALETTE_REJECT ? Y422_PLAYBACK : mode;
    if (!y422_capture_run(&reference, SIXEL_PIXELFORMAT_RGBFLOAT32,
                          reference_mode, 0) ||
        !y422_capture_pixels(&reference, expected, width, count)) {
        goto end;
    }
    if (mode == Y422_PALETTE &&
        (!y422_capture_run(&control, SIXEL_PIXELFORMAT_RGBFLOAT32,
                          Y422_PLAYBACK, 0) ||
         !y422_capture_pixels(&control, without_palette, width, count) ||
         memcmp(expected, without_palette, sizeof(expected)) == 0)) {
        goto end;
    }
    for (layout = SIXEL_PIXELFORMAT_YUYV; layout <= SIXEL_PIXELFORMAT_UYVY;
         ++layout) {
        for (ahead = 0; ahead < count; ++ahead) {
            if (!y422_capture_run(&packed, layout, mode, ahead) ||
                !y422_capture_pixels(&packed, actual, width, count) ||
                memcmp(actual, expected, sizeof(actual)) != 0) {
                fprintf(stderr, "Packed %d mode %d: decoded pixels differ\n",
                        layout, mode);
                goto end;
            }
            if (mode == Y422_DELTA &&
                (packed.painted[0] != 8 || packed.painted[1] != 1 ||
                 memcmp(packed.painted, reference.painted,
                        sizeof(packed.painted)) != 0 ||
                 memcmp(actual[0] + 4, actual[1] + 4, 4) == 0)) {
                fprintf(stderr, "Delta painted %zu then %zu pixels\n",
                        packed.painted[0], packed.painted[1]);
                goto end;
            }
            free(packed.stream.bytes);
            memset(&packed, 0, sizeof(packed));
        }
    }
    pass = 1;
end:
    free(control.stream.bytes);
    free(reference.stream.bytes);
    free(packed.stream.bytes);
    return pass ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* Positive controls keep the count assertions from passing with an
 * observer that misses the actual copy or the conversion loop. */
static int
test_y422_operation_observer(void)
{
    sixel_frame_t *frame = NULL;
    sixel_frame_interface_t *iface, *clone = NULL;
    sixel_color_def_t *rgb;
    sixel_ycbcr_def_t *definition;
    sixel_ycbcr422_counts_t operations = {0};
    unsigned char bytes[18];
    float samples[24];
    int pass = 0;

    rgb = sixel_color_def_new_named("srgb");
    definition = sixel_ycbcr_def_new(SIXEL_YCBCR_ENCODING_BT709,
        SIXEL_YCBCR_RANGE_FULL, SIXEL_CHROMA_LOCATION_CENTER, rgb);
    if (definition == NULL ||
        y422_fixture(&frame, SIXEL_PIXELFORMAT_YUYV, 0, 0, definition, rgb,
                     bytes, samples, &operations)) {
        goto end;
    }
    if (operations.packed_copies != 0 || operations.rgb_conversions != 0) {
        goto end;
    }
    iface = sixel_frame_as_interface(frame);
    if (iface->vtbl->clone(iface, NULL, &clone) || clone == NULL ||
        operations.packed_copies != 1 || operations.rgb_conversions != 0 ||
        sixel_frame_set_pixelformat(frame, SIXEL_PIXELFORMAT_RGBFLOAT32) ||
        sixel_frame_set_pixelformat(frame, SIXEL_PIXELFORMAT_RGBFLOAT32) ||
        operations.packed_copies != 1 || operations.rgb_conversions != 1) {
        goto end;
    }
    pass = 1;
end:
    if (clone != NULL) {
        clone->vtbl->unref(clone);
    }
    sixel_frame_unref(frame);
    sixel_ycbcr_def_free(definition);
    sixel_color_def_free(rgb);
    return pass ? EXIT_SUCCESS : EXIT_FAILURE;
}

static SIXELSTATUS
y422_fixed_playback(y422_capture_t *capture, sixel_frame_t *image,
                    sixel_frame_t *source, int adaptive)
{
    sixel_encoder_t *encoder = NULL;
    sixel_output_t *output = NULL;
    sixel_playback_t *playback = NULL;
    sixel_playback_config_t config;
    sixel_playback_options_t options;
    SIXELSTATUS status = SIXEL_FALSE;

    if (sixel_encoder_new(&encoder, NULL) ||
        (adaptive ?
            sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_SAMPLING_POLICY,
                                 "adaptive-grid") :
            sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_BUILTIN_PALETTE,
                                 "vt340color")) ||
        sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_GPU_POLICY, "off") ||
        sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_DIFFUSION, "none") ||
        sixel_output_new(&output, playback_test_write, &capture->stream,
                         NULL) ||
        sixel_playback_config_init(&config, sizeof(config)) ||
        sixel_playback_options_init(&options, sizeof(options))) {
        goto end;
    }
    config.done = y422_playback_done;
    config.done_context = capture;
    status = sixel_playback_new(&playback, encoder, output, &config, NULL);
    if (status == SIXEL_OK) {
        options.palette_source = source;
        status = sixel_playback_submit(playback, image, SIXEL_PLAYBACK_WAIT,
                                       -1, &options);
    }
    if (status == SIXEL_OK) {
        status = sixel_playback_drain(playback, -1);
    }
end:
    sixel_playback_unref(playback);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    return status;
}

static int
test_y422_large_palette_source(int adaptive)
{
    enum { width = 4096, height = 3072, stride = width * 2 };
    y422_capture_t with = {0}, without = {0};
    sixel_ycbcr422_counts_t source_operations = {0}, image_operations = {0};
    sixel_frame_t *image = NULL, *source = NULL;
    sixel_color_def_t *rgb;
    sixel_ycbcr_def_t *definition;
    unsigned char image_bytes[18];
    float samples[24];
    unsigned char *source_bytes = NULL;
    size_t source_size = (size_t)stride * height;
    SIXELSTATUS status;
    int pass = 0;

    rgb = sixel_color_def_new_named("srgb");
    definition = sixel_ycbcr_def_new(SIXEL_YCBCR_ENCODING_BT709,
        SIXEL_YCBCR_RANGE_FULL, SIXEL_CHROMA_LOCATION_CENTER, rgb);
    /* Packed storage fits in 24 MiB. Its 144 MiB RGB expansion would
     * exceed the frame allocation limit even though no sampling is needed. */
    source_bytes = calloc(1, source_size);
    if (definition == NULL || source_bytes == NULL ||
        sixel_frame_new(&source, NULL) ||
        y422_fixture(&image, SIXEL_PIXELFORMAT_YUYV, 0, 0, definition,
                     rgb, image_bytes, samples, &image_operations)) {
        goto end;
    }
    sixel_frame_observe_ycbcr422(source, &source_operations);
    if (sixel_frame_init_ycbcr422_borrowed(source, source_bytes, source_size,
            width, height, stride, SIXEL_PIXELFORMAT_YUYV, definition) ||
        (!adaptive && y422_fixed_playback(&without, image, NULL, 0))) {
        goto end;
    }
    status = y422_fixed_playback(&with, image, source, adaptive);
    if (status != SIXEL_OK) {
        fprintf(stderr, "Palette playback with source failed: 0x%x\n",
                (unsigned)status);
        goto end;
    }
    if (adaptive) {
        pass = !with.failed && with.completed == 1 && with.stream.size != 0 &&
            source_operations.packed_copies == 0 &&
            source_operations.rgb_conversions == 1 &&
            source_operations.rgb_pixels > 0 &&
            source_operations.rgb_pixels < 16384 &&
            image_operations.rgb_conversions == 1;
        goto end;
    }
    pass = !with.failed && !without.failed &&
        with.completed == 1 && without.completed == 1 &&
        with.stream.size != 0 && with.stream.size == without.stream.size &&
        memcmp(with.stream.bytes, without.stream.bytes, with.stream.size)
            == 0 &&
        source_operations.rgb_conversions == 0 &&
        source_operations.packed_copies == 0 &&
        image_operations.rgb_conversions == 2;
    if (!pass) {
        fprintf(stderr, "Bytes with/without source: %zu/%zu, "
                        "source copies/conversions: %u/%u, "
                        "image conversions: %u, completions: %d/%d\n",
                with.stream.size, without.stream.size,
                (unsigned)source_operations.packed_copies,
                (unsigned)source_operations.rgb_conversions,
                (unsigned)image_operations.rgb_conversions,
                with.completed, without.completed);
    }
end:
    free(with.stream.bytes);
    free(without.stream.bytes);
    sixel_frame_unref(source);
    sixel_frame_unref(image);
    sixel_ycbcr_def_free(definition);
    sixel_color_def_free(rgb);
    free(source_bytes);
    return pass ? EXIT_SUCCESS : EXIT_FAILURE;
}

static int
test_y422_grid_icc(void)
{
    sixel_frame_t *packed = NULL, *sample = NULL, *reference = NULL;
    sixel_color_def_t *rgb;
    sixel_ycbcr_def_t *definition;
    sixel_frame_interface_t *iface;
    sixel_frame_pixels_view_t view;
    sixel_ycbcr422_counts_t operations = {0};
    unsigned char bytes[18], reference_bytes[18];
    float scratch[24], expected[24];
    int pass = 0;

    rgb = sixel_color_def_new_icc(color_icc, sizeof(color_icc));
    definition = sixel_ycbcr_def_new(SIXEL_YCBCR_ENCODING_BT709,
        SIXEL_YCBCR_RANGE_FULL, SIXEL_CHROMA_LOCATION_CENTER, rgb);
    if (definition == NULL ||
        y422_fixture(&packed, SIXEL_PIXELFORMAT_YUYV, 0, 0, definition,
                     rgb, bytes, scratch, &operations) ||
        y422_fixture(&reference, SIXEL_PIXELFORMAT_RGBFLOAT32, 0, 0,
                     definition, rgb, reference_bytes, expected, NULL) ||
        sixel_frame_sample_ycbcr422(packed, NULL, 3, &sample)) {
        goto end;
    }
    sixel_ycbcr_def_free(definition);
    definition = NULL;
    sixel_frame_unref(packed);
    packed = NULL;
    iface = sixel_frame_as_interface(sample);
    if (iface->vtbl->get_pixels(iface, &view) ||
        view.width != 2 || view.height != 1 || view.rgb_definition != rgb ||
        view.pixels_float32 == NULL || operations.rgb_pixels != 2 ||
        operations.rgb_conversions != 1 || operations.packed_copies != 0) {
        goto end;
    }
    /* Coordinates are x=0 and x=3 in the original raster. */
    pass = fabsf(view.pixels_float32[0] - expected[0]) < 2e-7f &&
        fabsf(view.pixels_float32[1] - expected[1]) < 2e-7f &&
        fabsf(view.pixels_float32[2] - expected[2]) < 2e-7f &&
        fabsf(view.pixels_float32[3] - expected[9]) < 2e-7f &&
        fabsf(view.pixels_float32[4] - expected[10]) < 2e-7f &&
        fabsf(view.pixels_float32[5] - expected[11]) < 2e-7f;
end:
    sixel_frame_unref(sample);
    sixel_frame_unref(reference);
    sixel_frame_unref(packed);
    sixel_ycbcr_def_free(definition);
    sixel_color_def_free(rgb);
    return pass ? EXIT_SUCCESS : EXIT_FAILURE;
}

#if SIXEL_ENABLE_THREADS && SIXEL_ENCODER_PREPARE_AHEAD_SUPPORTED
enum {
    Y422_QUEUED_CANCEL,
    Y422_PREPARED_CANCEL,
    Y422_RETRY_DELTA,
    Y422_RETRY_REFERENCE,
    Y422_SAMPLE_ALLOC_FAIL,
    Y422_LATEST_RETRY,
    Y422_LATEST_REFERENCE
};

enum {
    Y422_LATEST_SOURCE_WIDTH = 32,
    Y422_LATEST_SOURCE_HEIGHT = 16,
    /* A target of 32 gives step four in both axes. */
    Y422_LATEST_SOURCE_SAMPLES = 32
};

struct y422_lifecycle {
    struct gpu_ahead_fixture gate;
    y422_capture_t *capture;
    unsigned char *image[2], *palette[2];
    sixel_ycbcr422_counts_t image_ops[2], palette_ops[2];
    SIXELSTATUS result[2];
    int callbacks, retried, second_builds;
};

static int y422_alloc_failures;

static void *
y422_fail_grid_malloc(size_t bytes)
{
    /* The source is 14x11 with step one. The displayed frame is 4x2. */
    if (bytes == 14u * 11u * 3u * sizeof(float)) {
        ++y422_alloc_failures;
        return NULL;
    }
    return malloc(bytes);
}

static int
y422_lifecycle_write(char *data, int size, void *context)
{
    struct y422_lifecycle *state = context;

    gpu_ahead_release(&state->gate, &state->gate.writer_entered);
    if (!gpu_ahead_wait(&state->gate, &state->gate.writer_open)) {
        return -1;
    }
    return playback_test_write(data, size, &state->capture->stream);
}

static void
y422_lifecycle_prepared(void *context, uint64_t sequence, SIXELSTATUS status)
{
    struct y422_lifecycle *state = context;

    if (sequence == 2) {
        state->gate.prepared_status = status;
        gpu_ahead_release(&state->gate, &state->gate.helper_complete);
    }
}

static int
y422_predict_key(void *context, uint64_t sequence, int predicted)
{
    (void)context;
    return sequence == 2 ? 0 : predicted;
}

static void
y422_retried(void *context, uint64_t sequence)
{
    struct y422_lifecycle *state = context;

    state->retried += sequence == 2;
}

static void
y422_fault_first_second_build(void *context, uint64_t sequence,
                              sixel_palette_job_test_hooks_t *hooks)
{
    struct y422_lifecycle *state = context;

    if (sequence == 2 && state->second_builds++ == 0) {
        hooks->completion_status = SIXEL_BAD_ALLOCATION;
        hooks->completion_retryable = 1;
    }
}

static void
y422_release_capture(sixel_playback_result_t const *result, void *context)
{
    struct y422_lifecycle *state = context;
    size_t slot = state->callbacks++;

    if (slot < 2) {
        state->result[slot] = result->status;
        state->capture->painted[slot] = result->painted_pixels;
        /* Recycle both caller-owned allocations at the public boundary. */
        free(state->image[slot]);
        free(state->palette[slot]);
        state->image[slot] = NULL;
        state->palette[slot] = NULL;
    }
}

static int
y422_lifecycle_submit(struct y422_lifecycle *state,
    sixel_playback_t *playback, int index, int fail_source, int latest)
{
    sixel_frame_t *image = NULL, *palette = NULL;
    sixel_color_def_t *rgb = NULL;
    sixel_ycbcr_def_t *definition = NULL;
    sixel_playback_options_t options;
    float scratch[24];
    size_t bytes;
    int width, height;
    size_t i;
    int ok = 0;

    rgb = sixel_color_def_new_named("srgb");
    definition = sixel_ycbcr_def_new(SIXEL_YCBCR_ENCODING_BT709,
        SIXEL_YCBCR_RANGE_FULL, SIXEL_CHROMA_LOCATION_CENTER, rgb);
    width = latest ? Y422_LATEST_SOURCE_WIDTH : 14;
    height = latest ? Y422_LATEST_SOURCE_HEIGHT : 11;
    bytes = fail_source || latest ? (size_t)width * height * 2u : 18u;
    state->image[index] = malloc(18u);
    state->palette[index] = calloc(1u, bytes);
    if (definition == NULL || state->image[index] == NULL ||
        state->palette[index] == NULL ||
        y422_fixture(&image, SIXEL_PIXELFORMAT_YUYV, 0, index, definition,
            rgb, state->image[index], scratch, &state->image_ops[index]) ||
        sixel_playback_options_init(&options, sizeof(options))) {
        goto end;
    }
    if (fail_source || latest) {
        if (latest) {
            memset(state->palette[index], 128, bytes);
            for (i = 0; i < bytes; i += 2u) {
                state->palette[index][i] = i % (2u * width) < (size_t)width
                    ? 80 : 160;
            }
        }
        if (sixel_frame_new(&palette, NULL)) {
            goto end;
        }
        sixel_frame_observe_ycbcr422(palette, &state->palette_ops[index]);
        if (sixel_frame_init_ycbcr422_borrowed(palette, state->palette[index],
                bytes, width, height, (size_t)width * 2u,
                SIXEL_PIXELFORMAT_YUYV, definition)) {
            goto end;
        }
    } else if (y422_fixture(&palette, SIXEL_PIXELFORMAT_YUYV, 0, 2,
                   definition, rgb, state->palette[index], scratch,
                   &state->palette_ops[index])) {
        goto end;
    }
    sixel_frame_set_frame_no(image, index);
    sixel_frame_set_multiframe(image, 1);
    options.palette_source = palette;
    if (latest) {
        /* Leave the first frame outside the retained rectangle so that the
         * second frame's predicted omission key becomes stale. */
        options.flags = SIXEL_PLAYBACK_OPTION_ORIGIN;
        options.origin_x = index == 0;
    }
    ok = sixel_playback_submit(playback, image, SIXEL_PLAYBACK_WAIT, 5000,
                                &options) == SIXEL_OK;
end:
    sixel_frame_unref(image);
    sixel_frame_unref(palette);
    sixel_ycbcr_def_free(definition);
    sixel_color_def_free(rgb);
    return ok;
}

static int
y422_lifecycle_run(int mode, y422_capture_t *capture)
{
    struct y422_lifecycle state = {0};
    struct sixel_encoder_prepare_test_hooks hooks = {0};
    struct sixel_encoder_palette_test_hooks palette_hooks = {0};
    sixel_playback_config_t config;
    sixel_allocator_t *allocator = NULL;
    sixel_encoder_t *encoder = NULL;
    sixel_output_t *output = NULL;
    sixel_playback_t *playback = NULL;
    SIXELSTATUS status;
    int i, count, conversions, ahead, cancel, fail_source, latest;
    int initialized = 0;
    int pass = 0;

    state.capture = capture;
    latest = mode == Y422_LATEST_RETRY || mode == Y422_LATEST_REFERENCE;
    ahead = mode == Y422_PREPARED_CANCEL || mode == Y422_RETRY_DELTA ||
        mode == Y422_LATEST_RETRY;
    cancel = mode == Y422_QUEUED_CANCEL || mode == Y422_PREPARED_CANCEL;
    fail_source = mode == Y422_SAMPLE_ALLOC_FAIL;
    count = fail_source ? 1 : 2;
    if (sixel_mutex_init(&state.gate.mutex)) {
        return 0;
    }
    if (sixel_cond_init(&state.gate.cond)) {
        sixel_mutex_destroy(&state.gate.mutex);
        return 0;
    }
    initialized = 1;
    state.gate.writer_open = fail_source || (!ahead && !cancel);
    y422_alloc_failures = 0;
    if ((fail_source && sixel_allocator_new(&allocator, y422_fail_grid_malloc,
                                             NULL, NULL, NULL)) ||
        sixel_encoder_new(&encoder, allocator) ||
        sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_THREADING_POLICY,
            mode == Y422_LATEST_RETRY ? "greedy:B8:prepare=ahead" :
            ahead ? "greedy:B2:prepare=ahead" : "greedy:B2:prepare=serial") ||
        sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_GPU_POLICY, "off") ||
        sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_SAMPLING_POLICY,
                             latest ? "auto" : "adaptive-grid") ||
        sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_DIFFUSION, "none") ||
        sixel_encoder_setopt(encoder, SIXEL_OPTFLAG_COLORS, "16") ||
        sixel_encoder_setopt(encoder,
                             SIXEL_OPTFLAG_UPDATE_POLICY, "delta:T0") ||
        (latest && sixel_encoder_set_6delta_plane_size(encoder, 4, 2)) ||
        sixel_output_new(&output, y422_lifecycle_write, &state, NULL) ||
        sixel_playback_config_init(&config, sizeof(config))) {
        goto end;
    }
    hooks.context = &state;
    hooks.complete = y422_lifecycle_prepared;
    if (mode == Y422_RETRY_DELTA) {
        hooks.predict_key = y422_predict_key;
    }
    hooks.retry = y422_retried;
    if (mode == Y422_LATEST_RETRY) {
        palette_hooks.context = &state;
        palette_hooks.build = y422_fault_first_second_build;
        encoder->palette_test_hooks = &palette_hooks;
    }
    if (latest) {
        config.palette_mode = SIXEL_PLAYBACK_PALETTE_LATEST;
    }
    encoder->prepare_test_hooks = &hooks;
    config.done = y422_release_capture;
    config.done_context = &state;
    if (sixel_playback_new(&playback, encoder, output, &config, NULL) ||
        !y422_lifecycle_submit(&state, playback, 0, fail_source, latest)) {
        goto end;
    }
    if (!fail_source) {
        if (!gpu_ahead_wait(&state.gate, &state.gate.writer_entered) ||
            !y422_lifecycle_submit(&state, playback, 1, 0, latest) ||
            (ahead && (!gpu_ahead_wait(&state.gate,
                                      &state.gate.helper_complete) ||
                       state.gate.prepared_status != SIXEL_OK)) ||
            (cancel && sixel_playback_cancel(playback))) {
            goto end;
        }
        gpu_ahead_release(&state.gate, &state.gate.writer_open);
    }
    status = sixel_playback_drain(playback, 5000);
    if (status != (fail_source ? SIXEL_BAD_ALLOCATION : SIXEL_OK) ||
        state.callbacks != count ||
        state.result[0] != status ||
        (!fail_source && state.result[1] !=
            (cancel ? SIXEL_INTERRUPTED : SIXEL_OK)) ||
        (fail_source && y422_alloc_failures != 1) ||
        (mode == Y422_RETRY_DELTA && state.retried != 1) ||
        (mode == Y422_LATEST_RETRY &&
            (state.retried != 1 || state.second_builds != 2 ||
             capture->painted[1] != 8))) {
        fprintf(stderr, "Drain 0x%x, frames 0x%x/0x%x, retries %d, builds %d, "
                        "second source %u pixels\n",
                status, state.result[0], state.result[1], state.retried,
                state.second_builds, (unsigned)state.palette_ops[1].rgb_pixels);
        goto end;
    }
    for (i = 0; i < count; ++i) {
        conversions = i == 1 && mode == Y422_QUEUED_CANCEL ? 0 : 1;
        if (state.image[i] != NULL || state.palette[i] != NULL ||
            state.image_ops[i].rgb_conversions != (unsigned int)conversions ||
            state.palette_ops[i].rgb_conversions !=
                (unsigned int)(fail_source ? 0 : conversions) ||
            state.image_ops[i].packed_copies != 0 ||
            state.palette_ops[i].packed_copies != 0 ||
            (latest && state.palette_ops[i].rgb_pixels !=
                Y422_LATEST_SOURCE_SAMPLES)) {
            goto end;
        }
    }
    pass = 1;
end:
    if (initialized) {
        gpu_ahead_release(&state.gate, &state.gate.writer_open);
    }
    sixel_playback_unref(playback);
    sixel_output_unref(output);
    sixel_encoder_unref(encoder);
    sixel_allocator_unref(allocator);
    for (i = 0; i < count; ++i) {
        free(state.image[i]);
        free(state.palette[i]);
    }
    sixel_cond_destroy(&state.gate.cond);
    sixel_mutex_destroy(&state.gate.mutex);
    return pass;
}
#endif

static int
test_y422_lifecycle(char const *name)
{
#if SIXEL_ENABLE_THREADS && SIXEL_ENCODER_PREPARE_AHEAD_SUPPORTED
    y422_capture_t actual = {0}, reference = {0};
    unsigned char actual_pixels[2][32], reference_pixels[2][32];
    int mode, pass;

    if (strcmp(name, "queued") == 0) {
        mode = Y422_QUEUED_CANCEL;
    } else if (strcmp(name, "prepared") == 0) {
        mode = Y422_PREPARED_CANCEL;
    } else if (strcmp(name, "allocation") == 0) {
        mode = Y422_SAMPLE_ALLOC_FAIL;
    } else if (strcmp(name, "latest-retry") == 0) {
        mode = Y422_LATEST_RETRY;
    } else {
        mode = Y422_RETRY_DELTA;
    }
    pass = y422_lifecycle_run(mode, &actual);
    if (pass && mode == Y422_RETRY_DELTA) {
        pass = y422_lifecycle_run(Y422_RETRY_REFERENCE, &reference) &&
            y422_capture_pixels(&actual, actual_pixels, 4, 2) &&
            y422_capture_pixels(&reference, reference_pixels, 4, 2) &&
            memcmp(actual_pixels, reference_pixels, sizeof(actual_pixels)) == 0;
    }
    if (pass && mode == Y422_LATEST_RETRY) {
        pass = y422_lifecycle_run(Y422_LATEST_REFERENCE, &reference) &&
            y422_capture_pixels(&actual, actual_pixels, 4, 2) &&
            y422_capture_pixels(&reference, reference_pixels, 4, 2) &&
            memcmp(actual_pixels, reference_pixels, sizeof(actual_pixels)) == 0;
    }
    if (!pass) {
        fprintf(stderr, "Packed playback %s failed: %s\n", name,
                sixel_helper_get_additional_message());
    }
    free(actual.stream.bytes);
    free(reference.stream.bytes);
    return pass ? EXIT_SUCCESS : EXIT_FAILURE;
#else
    (void)name;
    return 77;
#endif
}

int
test_frame_0014_clone_float(int argc, char **argv)
{
    static int const formats[] = {
        SIXEL_PIXELFORMAT_RGBA8888, SIXEL_PIXELFORMAT_ARGB8888,
        SIXEL_PIXELFORMAT_BGRA8888, SIXEL_PIXELFORMAT_ABGR8888,
        SIXEL_PIXELFORMAT_GA88, SIXEL_PIXELFORMAT_AG88
    };
    float pixels[30];
    float original[30];
    sixel_frame_t *frame;
    sixel_frame_interface_t *iface;
    sixel_frame_interface_t *clone;
    sixel_frame_pixels_view_t view;
    SIXELSTATUS status;
    size_t i;
    int c;
    unsigned char background[] = {10, 20, 30};
    int result;

    if (argc == 3 && strcmp(argv[1], "ycbcr422-deferred-delta") == 0) {
        return test_y422_deferred_delta(strcmp(argv[2], "yuyv") == 0
            ? SIXEL_PIXELFORMAT_YUYV : SIXEL_PIXELFORMAT_RGBFLOAT32);
    }
    if (argc == 3 && strcmp(argv[1], "ycbcr422-lifecycle") == 0) {
        return test_y422_lifecycle(argv[2]);
    }
    if (argc == 2 && strcmp(argv[1], "ycbcr422-fixed-palette-source") == 0) {
        return test_y422_large_palette_source(0);
    }
    if (argc == 2 && strcmp(argv[1], "ycbcr422-adaptive-source") == 0) {
        return test_y422_large_palette_source(1);
    }
    if (argc == 2 && strcmp(argv[1], "ycbcr422-grid-icc") == 0) {
        return test_y422_grid_icc();
    }
    if (argc == 2 && strcmp(argv[1], "ycbcr422-operation-observer") == 0) {
        return test_y422_operation_observer();
    }
    if (argc == 3 && strcmp(argv[1], "ycbcr422-encode") == 0) {
        return test_y422_encode(argv[2]);
    }
    if (argc == 3 && strcmp(argv[1], "ycbcr422-rgb") == 0) {
        return test_ycbcr422_rgb_cases(argv[2]);
    }
    if (argc == 3 && strcmp(argv[1], "ycbcr422") == 0) {
        if (strcmp(argv[2], "definition") == 0) {
            return test_ycbcr422_definition();
        }
        return test_ycbcr422(argv[2], SIXEL_PIXELFORMAT_YUYV,
                   SIXEL_YCBCR_ENCODING_BT601, SIXEL_YCBCR_RANGE_FULL,
                   SIXEL_CHROMA_LOCATION_CENTER) ||
               test_ycbcr422(argv[2], SIXEL_PIXELFORMAT_UYVY,
                   SIXEL_YCBCR_ENCODING_BT2020_NCL, SIXEL_YCBCR_RANGE_LIMITED,
                   SIXEL_CHROMA_LOCATION_LEFT) ||
               test_ycbcr422(argv[2], SIXEL_PIXELFORMAT_YUYV,
                   SIXEL_YCBCR_ENCODING_BT709, SIXEL_YCBCR_RANGE_LIMITED,
                   SIXEL_CHROMA_LOCATION_LEFT);
    }
    if (argc == 2 && strcmp(argv[1], "keyed-registers") == 0) {
        return test_keyed_registers();
    }
    if (argc == 2 && strcmp(argv[1], "linear-unpack") == 0) {
        return test_linear_unpack(SIXEL_PIXELFORMAT_PAL1, 1) ||
               test_linear_unpack(SIXEL_PIXELFORMAT_PAL2, 2) ||
               test_linear_unpack(SIXEL_PIXELFORMAT_PAL4, 4);
    }
    if (argc == 2 && strcmp(argv[1], "float-bgr") == 0) {
        return test_float_bgr(0) || test_float_bgr(1);
    }
    if (argc == 2 && strcmp(argv[1], "packed-float-output") == 0) {
        return test_packed_float_output();
    }
    if (argc == 2 && strcmp(argv[1], "permuted-primaries") == 0) {
        return test_permuted_primaries(0, 0) || test_permuted_primaries(1, 0) ||
               test_permuted_primaries(0, 1) || test_permuted_primaries(1, 1);
    }
    if (argc == 2 && strcmp(argv[1], "palette-recount") == 0) {
        return test_palette_recount(0);
    }
    if (argc == 2 && strcmp(argv[1], "packed-rgb-matrix") == 0) {
        return test_packed_rgb_matrix();
    }
    if (argc == 2 && strcmp(argv[1], "packed-rgb-reject") == 0) {
        return test_packed_rgb_reject();
    }
    if (argc == 2 && strcmp(argv[1], "reshape-storage") == 0) {
        return test_reshape_storage();
    }
    if (argc == 2 && strcmp(argv[1], "gray-saturation-matrix") == 0) {
        return icc_numeric_gray_intent_trc_test(0);
    }
    if (argc == 2 && strcmp(argv[1], "gray-perceptual-matrix") == 0) {
        return icc_numeric_gray_intent_trc_test(1);
    }
    if (argc == 2 && strcmp(argv[1], "borrowed-adoption-storage") == 0) {
        return test_borrowed_adoption_storage();
    }
    if (argc == 2 && strcmp(argv[1], "implicit-palette-storage") == 0) {
        return test_implicit_palette_storage();
    }
    if (argc == 2 && strcmp(argv[1], "mutable-palette-storage") == 0) {
        return test_mutable_palette_storage();
    }
    if (argc == 2 && strcmp(argv[1], "attached-palette-storage") == 0) {
        return test_attached_palette_storage();
    }
    if (argc == 2 && strcmp(argv[1], "repeated-implicit-palette-init") == 0) {
        return test_repeated_implicit_palette_init();
    }
    if (argc == 2 && strcmp(argv[1], "saturation-matrix") == 0) {
        return icc_numeric_frame_saturation_matrix_test();
    }
    if (argc == 2 && strcmp(argv[1], "rgb565-swap") == 0) {
        return test_packed_rgb_swap(565);
    }
    if (argc == 2 && strcmp(argv[1], "rgb555-swap") == 0) {
        return test_packed_rgb_swap(555);
    }
    if (argc == 2 && strcmp(argv[1], "png-storage") == 0) {
        return test_png_storage();
    }
    if (argc == 2 && strcmp(argv[1], "width-storage") == 0) {
        return test_set_dimension_storage(0);
    }
    if (argc == 2 && strcmp(argv[1], "height-storage") == 0) {
        return test_set_dimension_storage(1);
    }
    if (argc == 2 && strcmp(argv[1], "profiled-range") == 0) {
        return icc_numeric_frame_storage_fallback_test();
    }
    if (argc == 2 && strcmp(argv[1], "byte-bgr") == 0) {
        return test_byte_bgr(0) || test_byte_bgr(1);
    }
    if (argc == 2 && strcmp(argv[1], "padded-rgb") == 0) {
        return test_padded_output();
    }
    if (argc == 2 && strcmp(argv[1], "auto-mono") == 0) {
        return test_auto_mono();
    }
    if (argc == 2 && strcmp(argv[1], "collinear") == 0) {
        return test_collinear_primary(0) || test_collinear_primary(1);
    }
    if (argc == 2 && strcmp(argv[1], "default-pixel") == 0) {
        return test_raw_default("\033P0;1q\"1;1;1;1@\033\\", 1, 1);
    }
    if (argc == 2 && strcmp(argv[1], "default-band") == 0) {
        return test_raw_default("\033P0;1q\"1;1;1;6~\033\\", 1, 6);
    }
    if (argc == 2 && strcmp(argv[1], "default-aspect") == 0) {
        return test_raw_default("\033P0;1q$\"2;1;1;2@\033\\", 1, 2);
    }
    if (argc == 2 && strcmp(argv[1], "default-workers") == 0) {
        return test_raw_default("\033P0;1q\"1;1;32;12!32~-!32~\033\\", 32, 12);
    }
    if (argc == 2 && strcmp(argv[1], "count-first") == 0) {
        return test_palette_recount(1);
    }
    if (argc == 2 && strcmp(argv[1], "indexed-direct-mask") == 0) {
        for (i = 0; i < 2; ++i) {
            for (c = -1; c <= 1; c += 2) {
                if (test_indexed_delivery(SIXEL_PIXELFORMAT_PAL1, c, 0,
                        SIXEL_PIXELFORMAT_PAL1, (int)i) ||
                    test_indexed_delivery(SIXEL_PIXELFORMAT_PAL8, c, 0,
                        SIXEL_PIXELFORMAT_PAL8, (int)i)) {
                    return EXIT_FAILURE;
                }
            }
        }
        return EXIT_SUCCESS;
    }
    if (argc == 2 && (strcmp(argv[1], "palette-linear-byte") == 0 ||
                      strcmp(argv[1], "palette-linear-float") == 0)) {
        c = strcmp(argv[1], "palette-linear-byte") == 0 ?
            SIXEL_PIXELFORMAT_RGB888 : SIXEL_PIXELFORMAT_RGBFLOAT32;
        return test_indexed_delivery(SIXEL_PIXELFORMAT_PAL1, -1, 1,
                   SIXEL_PIXELFORMAT_PAL1, 0) ||
               test_indexed_delivery(SIXEL_PIXELFORMAT_PAL1, -1, 1, c, 0) ||
               test_indexed_delivery(SIXEL_PIXELFORMAT_PAL8, -1, 1, c, 0);
    }
    if (argc == 2 && strcmp(argv[1], "linear-dark-resize") == 0) {
        return test_linear_resize(0, 0) || test_linear_resize(0, 1);
    }
    if (argc == 2 && strcmp(argv[1], "linear-resize-retry") == 0) {
        return test_linear_resize(1, 0);
    }
    if (argc == 2 && strcmp(argv[1], "linear-identity") == 0) {
        return test_analytic_independence(0);
    }
    if (argc == 2 && strcmp(argv[1], "p3-blue") == 0) {
        return test_analytic_independence(1);
    }
    if (argc == 2 && strcmp(argv[1], "linear-strip") == 0) {
        return test_linear_strip();
    }
    if (argc == 2 && strcmp(argv[1], "crop-packed-alpha") == 0) {
        for (i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i) {
            if (test_converted_paint(0, formats[i],
                    SIXEL_PIXELFORMAT_RGBFLOAT32, 3) ||
                test_converted_paint(1, formats[i],
                    SIXEL_PIXELFORMAT_RGB888, 2)) {
                return EXIT_FAILURE;
            }
        }
        return EXIT_SUCCESS;
    }
    if (argc == 2 && strcmp(argv[1], "composite-channels") == 0) {
        for (i = 0; i < 4; ++i) {
            if (test_alpha_composite(formats[i], 0) ||
                test_alpha_composite(formats[i], 1)) {
                return EXIT_FAILURE;
            }
        }
        return EXIT_SUCCESS;
    }
    if (argc == 2 && strcmp(argv[1], "linear-packed") == 0) {
        return test_linear_packed_gray(SIXEL_PIXELFORMAT_G2) ||
               test_linear_packed_gray(SIXEL_PIXELFORMAT_G4);
    }
    if (argc == 2 && strcmp(argv[1], "implicit-palette-crop") == 0) {
        return test_implicit_palette_crop();
    }
    if (argc == 2 && strcmp(argv[1], "shared-red-blue") == 0) {
        return test_shared_primary(1);
    }
    if (argc == 2 && strcmp(argv[1], "shared-blue") == 0) {
        return test_shared_primary(0);
    }
    if (argc == 2 && strcmp(argv[1], "tga-storage") == 0) {
        return test_tga_storage();
    }
    if (argc == 2 && strcmp(argv[1], "strip-storage") == 0) {
        for (i = 0; i < 4; ++i) {
            if (test_strip_storage(formats[i], NULL) ||
                test_strip_storage(formats[i], background)) {
                return EXIT_FAILURE;
            }
        }
        return EXIT_SUCCESS;
    }
    if (argc == 2 && strcmp(argv[1], "indexed-paint") == 0) {
        return test_converted_paint(0, SIXEL_PIXELFORMAT_PAL8,
                                    SIXEL_PIXELFORMAT_RGBFLOAT32, 4);
    }
    if (argc == 2 && strcmp(argv[1], "indexed-byte-mask") == 0) {
        return test_converted_paint(1, SIXEL_PIXELFORMAT_PAL8,
                                    SIXEL_PIXELFORMAT_RGB888, 4);
    }
    if (argc == 2 && strcmp(argv[1], "rgba-byte-mask") == 0) {
        for (i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i) {
            if (test_converted_paint(0, formats[i],
                    SIXEL_PIXELFORMAT_RGB888, 4) != EXIT_SUCCESS ||
                test_converted_paint(1, formats[i],
                    SIXEL_PIXELFORMAT_RGB888, 4) != EXIT_SUCCESS) {
                return EXIT_FAILURE;
            }
        }
        return EXIT_SUCCESS;
    }
    if (argc == 2 && strcmp(argv[1], "rgba-float-mask") == 0) {
        for (i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i) {
            if (test_converted_paint(0, formats[i],
                    SIXEL_PIXELFORMAT_RGBFLOAT32, 4) != EXIT_SUCCESS ||
                test_converted_paint(1, formats[i],
                    SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, 4) != EXIT_SUCCESS) {
                return EXIT_FAILURE;
            }
        }
        return EXIT_SUCCESS;
    }
    if (argc == 2 && strcmp(argv[1], "packed-byte-mask") == 0) {
        return test_converted_paint(1, SIXEL_PIXELFORMAT_PAL1,
                                    SIXEL_PIXELFORMAT_RGB888, 4);
    }
    if (argc == 2 && strcmp(argv[1], "implicit-palette") == 0) {
        return test_implicit_palette();
    }
    if (argc == 2 && strcmp(argv[1], "indexed-mask") == 0) {
        return test_converted_paint(1, SIXEL_PIXELFORMAT_PAL8,
                                    SIXEL_PIXELFORMAT_RGBFLOAT32, 4);
    }
    if (argc == 2 && strcmp(argv[1], "packed-storage") == 0) {
        return test_packed_rows(0);
    }
    if (argc == 2 && strcmp(argv[1], "packed-clone") == 0) {
        return test_packed_rows(1);
    }
    if (argc == 2 && strcmp(argv[1], "crop-storage") == 0) {
        return test_crop_storage(0);
    }
    if (argc == 2 && strcmp(argv[1], "crop-mask-storage") == 0) {
        return test_crop_storage(1);
    }
    if (argc == 2 && strcmp(argv[1], "crop-alpha-storage") == 0) {
        return test_crop_storage(2);
    }
    if (argc == 3 &&
        (strcmp(argv[1], "smptec-byte") == 0 ||
         strcmp(argv[1], "smptec-float") == 0) &&
        (strcmp(argv[2], "rgb888") == 0 ||
         strcmp(argv[2], "rgbfloat32") == 0)) {
        return test_smptec_to_srgb(strcmp(argv[1], "smptec-byte") == 0,
            strcmp(argv[2], "rgb888") == 0 ? SIXEL_PIXELFORMAT_RGB888 :
                                             SIXEL_PIXELFORMAT_RGBFLOAT32);
    }
    if (argc == 2 && strcmp(argv[1], "srgb-byte-tie") == 0) {
        return test_byte_tie("srgb");
    }
    if (argc == 2 && strcmp(argv[1], "p3-byte-tie") == 0) {
        return test_byte_tie("display-p3");
    }
    if (argc == 2 && strcmp(argv[1], "d60-byte-tie") == 0) {
        return test_byte_tie("d60");
    }
    if (argc == 2 && strcmp(argv[1], "raw-storage") == 0) {
        return test_raw_storage();
    }
    if (argc == 2 && strcmp(argv[1], "segmented-storage") == 0) {
        return icc_segmented_frame_storage_test();
    }
    if (argc == 2 && strcmp(argv[1], "profile-storage") == 0) {
        return icc_mpet_frame_storage_test();
    }
    if (argc == 2 && strcmp(argv[1], "raw-palette") == 0) {
        return test_raw_palette();
    }
    if (argc == 2 && strcmp(argv[1], "profile-lut") == 0) {
        return test_profile_colorspace_lut(0);
    }
    if (argc == 2 && strcmp(argv[1], "relative-lut") == 0) {
        return test_profile_colorspace_lut(1);
    }
    if (argc == 2 && strcmp(argv[1], "byte-resize-retry") == 0) {
        return test_profile_byte_resize_retry();
    }
    if (argc == 2 && strcmp(argv[1], "public-icc-direction") == 0) {
        return test_rgbf32_input_direction();
    }
    if (argc == 2 && strcmp(argv[1], "public-rgbf32-owned-slice") == 0) {
        return test_rgbf32_owned_slice();
    }
    if (argc == 2 && strcmp(argv[1], "public-rgbf32-retry") == 0) {
        return test_rgbf32_public_retry();
    }
    if (argc == 2 && strcmp(argv[1], "packed-retry") == 0) {
        return test_packed_retry();
    }
    if (argc == 2 && (strcmp(argv[1], "srgb") == 0 ||
                      strcmp(argv[1], "p3") == 0 ||
                      strcmp(argv[1], "icc") == 0 ||
                      strcmp(argv[1], "retry") == 0)) {
        return test_profile_frame(argv[1]);
    }
    if (argc != 1) {
        fprintf(stderr, "Unknown frame storage test arguments\n");
        return EXIT_FAILURE;
    }
    for (i = 0; i < 10; ++i) {
        for (c = 0; c < 3; ++c) {
            pixels[i * 3 + c] = (float)color_opponents[i][c];
        }
    }
    memcpy(original, pixels, sizeof(pixels));
    frame = NULL;
    clone = NULL;
    result = EXIT_FAILURE;
    status = sixel_frame_new(&frame, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_frame_init_borrowed(frame, pixels, 10, 1,
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, NULL, -1);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    iface = sixel_frame_as_interface(frame);
    status = iface->vtbl->clone(iface, NULL, &clone);
    if (SIXEL_FAILED(status) || clone == NULL) {
        goto end;
    }
    status = sixel_frame_get_work_pixels(clone, &view);
    if (SIXEL_FAILED(status) || view.pixels != NULL
            || view.pixels_float32 == NULL || view.pixels_float32 == pixels
            || view.pixelformat != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32
            || view.colorspace != SIXEL_COLORSPACE_SRGB_LINEAR
            || memcmp(view.pixels_float32, original, sizeof(original)) != 0) {
        goto end;
    }
    status = clone->vtbl->set_pixelformat(clone, SIXEL_PIXELFORMAT_RGB888);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_frame_get_work_pixels(clone, &view);
    if (SIXEL_FAILED(status) || view.pixels_float32 != NULL
            || view.pixels == NULL
            || view.pixelformat != SIXEL_PIXELFORMAT_RGB888
            || view.colorspace != SIXEL_COLORSPACE_SRGB
            || view.pixels[0] != 255 || view.pixels[1] != 0
            || view.pixels[2] != 0) {
        goto end;
    }
    status = sixel_frame_get_work_pixels(iface, &view);
    if (SIXEL_FAILED(status) || view.pixels != NULL
            || view.pixels_float32 != pixels
            || view.pixelformat != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32
            || view.colorspace != SIXEL_COLORSPACE_SRGB_LINEAR
            || memcmp(view.pixels_float32, original, sizeof(original)) != 0) {
        goto end;
    }
    result = EXIT_SUCCESS;
end:
    if (clone != NULL) {
        clone->vtbl->unref(clone);
    }
    sixel_frame_unref(frame);
    return result;
}
