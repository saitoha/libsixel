/* SPDX-License-Identifier: MIT */
/* A byte derivative belongs to a clone, never to the canonical float frame. */
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include <6cells.h>
#include "src/cms.h"
#include "src/loader-common.h"
#include "tests/data/color-numeric/icc.h"
#include "tests/data/color-numeric/paired-lut.h"
#include "tests/data/color-numeric/opponents.h"
#include "tests/data/color-numeric/vectors.h"

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

static int
test_profile_colorspace_lut(void)
{
    unsigned char bytes[sizeof(color_paired_lut)];
    float pixel[3] = {.1f, .2f, .3f};
    /* Every A2B0 CLUT vertex encodes D50 XYZ (.25, .5, .75).
     * Multiply by the inverse D50-adapted sRGB primary matrix. */
    double const expected[] = {-.3930784364, .7384529717, .9578093449};
    sixel_frame_t *frame = NULL;
    sixel_frame_interface_t *iface;
    sixel_frame_pixels_view_t view;
    sixel_color_def_t *profile;
    int c;
    int result = EXIT_FAILURE;

    memcpy(bytes, color_paired_lut, sizeof(bytes));
    memcpy(bytes + 12, "spac", 4);
    profile = sixel_cdef_icc(bytes, sizeof(bytes));
    if (profile == NULL || SIXEL_FAILED(sixel_frame_new(&frame, NULL)) ||
        SIXEL_FAILED(sixel_frame_init_borrowed(frame, pixel, 1, 1,
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
    for (c = 0; c < 3; ++c) {
        if (!(fabs(view.pixels_float32[c] - expected[c]) < 1e-6)) {
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
    float pixels[30];
    float original[30];
    sixel_frame_t *frame;
    sixel_frame_interface_t *iface;
    sixel_frame_interface_t *clone;
    sixel_frame_pixels_view_t view;
    SIXELSTATUS status;
    size_t i;
    int c;
    int result;

    if (argc == 2 && strcmp(argv[1], "raw-palette") == 0) {
        return test_raw_palette();
    }
    if (argc == 2 && strcmp(argv[1], "profile-lut") == 0) {
        return test_profile_colorspace_lut();
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
