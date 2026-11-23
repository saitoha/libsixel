/* SPDX-License-Identifier: MIT */
/* A byte derivative belongs to a clone, never to the canonical float frame. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sixel.h>
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
        SIXEL_SUCCEEDED(clone->vtbl->get_pixels(clone, &view)) &&
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

    if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
        SIXEL_FAILED(sixel_frame_init_borrowed(frame, width == 4 ? g2 : g4,
            width, 1, format, NULL, -1))) {
        goto end;
    }
    iface = sixel_frame_as_interface(frame);
    sixel_frame_set_colorspace(frame, SIXEL_COLORSPACE_LINEAR);
    if (SIXEL_FAILED(iface->vtbl->set_pixelformat(iface,
            SIXEL_PIXELFORMAT_RGB888)) ||
        SIXEL_FAILED(iface->vtbl->get_pixels(iface, &view)) ||
        view.colorspace != SIXEL_COLORSPACE_GAMMA || view.pixels == NULL) {
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
        sixel_frame_get_pixelformat(frame) == SIXEL_PIXELFORMAT_RGB888 &&
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
                SIXEL_FAILED(clone->vtbl->get_pixels(clone, &view)) ||
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
        SIXEL_FAILED(iface->vtbl->get_pixels(iface, &view)) ||
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
    if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
        SIXEL_FAILED(sixel_frame_init_borrowed(frame,
            format == SIXEL_PIXELFORMAT_PAL8 ? indices : packed, 4, 2,
            format, palette, 2))) {
        goto end;
    }
    iface = sixel_frame_as_interface(frame);
    if (linear) {
        sixel_frame_set_colorspace(frame, SIXEL_COLORSPACE_LINEAR);
    } else {
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
            SIXEL_FAILED(iface->vtbl->get_pixels(iface, &view)) ||
            view.pixelformat != target ||
            view.colorspace != SIXEL_COLORSPACE_GAMMA) {
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
        SIXEL_SUCCEEDED(sixel_frame_init_borrowed(frame, pixel, 1, 1,
            SIXEL_PIXELFORMAT_RGBA8888, NULL, -1))) {
        sixel_frame_set_colorspace(frame, SIXEL_COLORSPACE_LINEAR);
        if (SIXEL_SUCCEEDED(sixel_frame_strip_alpha(frame, NULL)) &&
            sixel_frame_get_colorspace(frame) == SIXEL_COLORSPACE_LINEAR &&
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
            && SIXEL_SUCCEEDED(iface->vtbl->get_pixels(iface, &view))) {
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
    float pixel[] = {.0015f, .0015f, .0015f};
    unsigned char const expected[] = {5, 5, 5, 5, 5, 5};
    sixel_allocator_t *allocator = NULL;
    sixel_frame_t *frame = NULL;
    sixel_color_def_t *srgb = NULL, *definition = NULL;
    sixel_frame_pixels_view_t view;
    sixel_frame_interface_t *iface;
    int result = EXIT_FAILURE;

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
            SIXEL_FAILED(iface->vtbl->get_pixels(iface, &view)) ||
            view.pixels_float32 != pixel || view.width != 1 ||
            view.height != 1 || view.colorspace != SIXEL_COLORSPACE_LINEAR ||
            pixel[0] != .0015f || pixel[1] != .0015f || pixel[2] != .0015f) {
            goto end;
        }
        fail_six_bytes = 0;
    }
    if (SIXEL_SUCCEEDED(iface->vtbl->resize(iface, 2, 1,
            SIXEL_RES_NEAREST)) &&
        SIXEL_SUCCEEDED(iface->vtbl->set_pixelformat(iface,
            SIXEL_PIXELFORMAT_RGB888)) &&
        SIXEL_SUCCEEDED(iface->vtbl->get_pixels(iface, &view)) &&
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
        SIXEL_FAILED(iface->vtbl->get_pixels(iface, &view)) ||
        view.pixelformat != SIXEL_PIXELFORMAT_PAL1 ||
        view.pixels != indices || view.palette != palette ||
        indices[0] != 0xa0 || indices[1] != 0x40) {
        goto end;
    }
    fail_six_bytes = 0;
    if (SIXEL_FAILED(iface->vtbl->set_pixelformat(iface,
            SIXEL_PIXELFORMAT_RGBFLOAT32)) ||
        SIXEL_FAILED(iface->vtbl->get_pixels(iface, &view)) ||
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
        SIXEL_FAILED(iface->vtbl->get_pixels(iface, &view)) ||
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
        SIXEL_SUCCEEDED(clone->vtbl->get_pixels(clone, &view)) &&
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

    if (SIXEL_FAILED(iface->vtbl->get_pixels(iface, &view)) ||
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
        SIXEL_SUCCEEDED(clone->vtbl->get_pixels(clone, &view)) &&
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
        SIXEL_FAILED(iface->vtbl->get_pixels(iface, &view)) ||
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
    float pixel[] = {.4f, .5f, .6f};
    /* Encoded Display P3 to gamma sRGB, rounded directly to RGB888. */
    unsigned char const expected[] = {95, 128, 156, 95, 128, 156};
    sixel_allocator_t *allocator = NULL;
    sixel_frame_t *frame = NULL;
    sixel_frame_interface_t *iface;
    sixel_frame_pixels_view_t view;
    sixel_color_def_t *p3;
    sixel_color_def_t const *original;
    int result = EXIT_FAILURE;

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
        SIXEL_FAILED(iface->vtbl->get_pixels(iface, &view))) {
        goto end;
    }
    original = view.rgb_definition;
    fail_six_bytes = 1;
    if (iface->vtbl->resize(iface, 2, 1, SIXEL_RES_NEAREST) !=
            SIXEL_BAD_ALLOCATION ||
        SIXEL_FAILED(iface->vtbl->get_pixels(iface, &view)) ||
        view.rgb_definition != original || view.pixels_float32 != pixel ||
        view.width != 1 || view.height != 1 ||
        pixel[0] != .4f || pixel[1] != .5f || pixel[2] != .6f) {
        goto end;
    }
    fail_six_bytes = 0;
    if (SIXEL_SUCCEEDED(iface->vtbl->resize(iface, 2, 1,
            SIXEL_RES_NEAREST)) &&
        SIXEL_SUCCEEDED(iface->vtbl->get_pixels(iface, &view)) &&
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

    if (SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
        SIXEL_FAILED(sixel_frame_init_borrowed(frame,
            byte_source ? (void *)bytes : (void *)pixel, 1, 1,
            byte_source ? SIXEL_PIXELFORMAT_RGB888 :
                          SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, NULL, -1))) {
        goto end;
    }
    sixel_frame_set_colorspace(frame, SIXEL_COLORSPACE_SMPTEC);
    iface = sixel_frame_as_interface(frame);
    if (SIXEL_FAILED(iface->vtbl->set_pixelformat(iface, target)) ||
        SIXEL_FAILED(iface->vtbl->get_pixels(iface, &view)) ||
        view.colorspace != SIXEL_COLORSPACE_GAMMA ||
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
        SIXEL_SUCCEEDED(iface->vtbl->get_pixels(iface, &view)) &&
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

    if (SIXEL_FAILED(iface->vtbl->get_pixels(iface, &view)) ||
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

    if (SIXEL_FAILED(iface->vtbl->get_pixels(iface, &view)) ||
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
    unsigned char const rgb[] = {248, bits == 565 ? 84 : 168, 24,
                                 0, 0, 248};
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
        sixel_frame_get_pixelformat(frame) == target &&
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
                sixel_frame_get_pixelformat(frame) != formats[target] ||
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
        sixel_frame_get_pixelformat(frame) == SIXEL_PIXELFORMAT_G8 &&
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
    unsigned char const expected[] = {
        128, 128, 128, 127, d60 ? 127 : 128, 128,
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
        SIXEL_SUCCEEDED(iface->vtbl->get_pixels(iface, &view)) &&
        view.pixelformat == SIXEL_PIXELFORMAT_RGB888 && view.pixels != NULL &&
        memcmp(view.pixels, expected, sizeof(expected)) == 0) {
        result = EXIT_SUCCESS;
    }
end:
    sixel_frame_unref(frame);
    sixel_cdef_free(definition);
    return result;
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
            SIXEL_FAILED(iface->vtbl->get_pixels(iface, &view)) ||
            view.rgb_definition == NULL || view.pixels_float32 != pixels ||
            pixels[0] != 0 || pixels[1] != 1 || pixels[2] != 0 ||
            !isnan(pixels[3])) {
            goto end;
        }
        pixels[3] = 1;
        if (SIXEL_FAILED(iface->vtbl->set_pixelformat(iface,
                SIXEL_PIXELFORMAT_LINEARRGBFLOAT32)) ||
            SIXEL_FAILED(iface->vtbl->get_pixels(iface, &view)) ||
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
        SIXEL_FAILED(clone->vtbl->get_pixels(clone, &view)) ||
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
        SIXEL_FAILED(clone->vtbl->get_pixels(clone, &view)) ||
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

    linear = sixel_color_def_new_named("linear-srgb");
    gamma = sixel_color_def_new_named("srgb");
    if (linear == NULL || gamma == NULL ||
            SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
            SIXEL_FAILED(sixel_frame_init_rgbf32_borrowed(
                frame, gray, 1, 1, linear))) {
        goto end;
    }
    sixel_color_def_free(linear);
    linear = NULL;
    /* A failed replacement must retain both the borrow and its definition. */
    iface = sixel_frame_as_interface(frame);
    if (SIXEL_SUCCEEDED(sixel_frame_init_rgbf32_borrowed(
                frame, replacement, 0, 1, gamma)) ||
            SIXEL_FAILED(iface->vtbl->get_pixels(iface, &view)) ||
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
            SIXEL_FAILED(iface->vtbl->get_pixels(iface, &view)) ||
            view.pixels_float32 != replacement ||
            !sixel_cdef_same_rgb(view.rgb_definition, gamma)) {
        goto end;
    }
    result = EXIT_SUCCESS;
end:
    sixel_color_def_free(gamma);
    sixel_color_def_free(linear);
    sixel_frame_unref(frame);
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
        SIXEL_FAILED(sixel_frame_init_borrowed(frame, rows, 2, 2, format,
                                               palette, 2))) {
        goto end;
    }
    sixel_frame_set_colorspace(frame, SIXEL_COLORSPACE_LINEAR);
    if (SIXEL_SUCCEEDED(sixel_frame_set_pixelformat(frame,
            SIXEL_PIXELFORMAT_PAL8)) &&
        sixel_frame_get_pixelformat(frame) == SIXEL_PIXELFORMAT_PAL8 &&
        sixel_frame_get_colorspace(frame) == SIXEL_COLORSPACE_LINEAR &&
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
        sixel_frame_get_pixelformat(frame) == SIXEL_PIXELFORMAT_BGR888 &&
        memcmp(sixel_frame_get_pixels(frame), expected, sizeof(expected)) == 0
        && sixel_frame_get_colorspace(frame) == SIXEL_COLORSPACE_GAMMA) {
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
        sixel_frame_get_pixelformat(frame) == SIXEL_PIXELFORMAT_BGR888 &&
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
    status = clone->vtbl->get_pixels(clone, &view);
    if (SIXEL_FAILED(status) || view.pixels != NULL
            || view.pixels_float32 == NULL || view.pixels_float32 == pixels
            || view.pixelformat != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32
            || view.colorspace != SIXEL_COLORSPACE_LINEAR
            || memcmp(view.pixels_float32, original, sizeof(original)) != 0) {
        goto end;
    }
    status = clone->vtbl->set_pixelformat(clone, SIXEL_PIXELFORMAT_RGB888);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = clone->vtbl->get_pixels(clone, &view);
    if (SIXEL_FAILED(status) || view.pixels_float32 != NULL
            || view.pixels == NULL
            || view.pixelformat != SIXEL_PIXELFORMAT_RGB888
            || view.colorspace != SIXEL_COLORSPACE_GAMMA
            || view.pixels[0] != 255 || view.pixels[1] != 0
            || view.pixels[2] != 0) {
        goto end;
    }
    status = iface->vtbl->get_pixels(iface, &view);
    if (SIXEL_FAILED(status) || view.pixels != NULL
            || view.pixels_float32 != pixels
            || view.pixelformat != SIXEL_PIXELFORMAT_LINEARRGBFLOAT32
            || view.colorspace != SIXEL_COLORSPACE_LINEAR
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
