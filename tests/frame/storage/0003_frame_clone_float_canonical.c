/* SPDX-License-Identifier: MIT */
/* A byte derivative belongs to a clone, never to the canonical float frame. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
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
test_converted_paint(int with_mask, int source, int target)
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
    if (SIXEL_FAILED(iface->vtbl->set_pixelformat(iface, target)) ||
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
        width != 4 || height != 1 || paint == NULL ||
        memcmp(paint, expected, sizeof(expected)) != 0) {
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
test_shared_primary(void)
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
    sixel_color_def_t *p3;
    sixel_frame_t *frame = NULL;
    sixel_frame_interface_t *iface;
    sixel_frame_pixels_view_t view;
    int result = EXIT_FAILURE;

    p3 = sixel_cdef_name("display-p3");
    if (p3 == NULL || SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
        SIXEL_FAILED(sixel_frame_init_borrowed(frame, samples, 5, 1,
            SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1))) {
        goto end;
    }
    iface = sixel_frame_as_interface(frame);
    if (SIXEL_SUCCEEDED(iface->vtbl->set_rgb_definition(iface, p3)) &&
        SIXEL_SUCCEEDED(iface->vtbl->set_pixelformat(iface,
            SIXEL_PIXELFORMAT_RGB888)) &&
        SIXEL_SUCCEEDED(iface->vtbl->get_pixels(iface, &view)) &&
        view.pixels != NULL &&
        memcmp(view.pixels, expected, sizeof(expected)) == 0) {
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
    definition = icc ? sixel_cdef_icc(color_icc, sizeof(color_icc)) :
        sixel_cdef_name(srgb ? "srgb" : "display-p3");
    if (definition == NULL || SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
        SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixels, 2, 1,
            SIXEL_PIXELFORMAT_RGBFLOAT32, NULL, -1))) {
        goto end;
    }
    iface = sixel_frame_as_interface(frame);
    if (SIXEL_FAILED(iface->vtbl->set_rgb_definition(iface, definition))) {
        goto end;
    }
    if (icc && (SIXEL_FAILED(iface->vtbl->measure_storage(iface,
            &footprint)) || footprint < sizeof(color_icc) + sizeof(pixels))) {
        goto end;
    }
    sixel_cdef_free(definition);
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
    sixel_cdef_free(definition);
    sixel_frame_unref(frame);
    if (clone != NULL) {
        clone->vtbl->unref(clone);
    }
    return result;
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

    if (argc == 2 && strcmp(argv[1], "shared-blue") == 0) {
        return test_shared_primary();
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
                                    SIXEL_PIXELFORMAT_RGBFLOAT32);
    }
    if (argc == 2 && strcmp(argv[1], "indexed-byte-mask") == 0) {
        return test_converted_paint(1, SIXEL_PIXELFORMAT_PAL8,
                                    SIXEL_PIXELFORMAT_RGB888);
    }
    if (argc == 2 && strcmp(argv[1], "rgba-byte-mask") == 0) {
        for (i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i) {
            if (test_converted_paint(0, formats[i],
                    SIXEL_PIXELFORMAT_RGB888) != EXIT_SUCCESS ||
                test_converted_paint(1, formats[i],
                    SIXEL_PIXELFORMAT_RGB888) != EXIT_SUCCESS) {
                return EXIT_FAILURE;
            }
        }
        return EXIT_SUCCESS;
    }
    if (argc == 2 && strcmp(argv[1], "rgba-float-mask") == 0) {
        for (i = 0; i < sizeof(formats) / sizeof(formats[0]); ++i) {
            if (test_converted_paint(0, formats[i],
                    SIXEL_PIXELFORMAT_RGBFLOAT32) != EXIT_SUCCESS ||
                test_converted_paint(1, formats[i],
                    SIXEL_PIXELFORMAT_LINEARRGBFLOAT32) != EXIT_SUCCESS) {
                return EXIT_FAILURE;
            }
        }
        return EXIT_SUCCESS;
    }
    if (argc == 2 && strcmp(argv[1], "packed-byte-mask") == 0) {
        return test_converted_paint(1, SIXEL_PIXELFORMAT_PAL1,
                                    SIXEL_PIXELFORMAT_RGB888);
    }
    if (argc == 2 && strcmp(argv[1], "implicit-palette") == 0) {
        return test_implicit_palette();
    }
    if (argc == 2 && strcmp(argv[1], "indexed-mask") == 0) {
        return test_converted_paint(1, SIXEL_PIXELFORMAT_PAL8,
                                    SIXEL_PIXELFORMAT_RGBFLOAT32);
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
    if (argc == 3) {
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
    if (argc == 2 && strcmp(argv[1], "packed-retry") == 0) {
        return test_packed_retry();
    }
    if (argc == 2) {
        return test_profile_frame(argv[1]);
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
