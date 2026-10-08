/* SPDX-License-Identifier: MIT */
/* Allocation pressure must not select byte arithmetic for float work. */
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/dither.h"
#include "src/palette.h"

#define PROMOTION_PIXELS 7

static int promotion_armed;
static int promotion_hits;
static int promotion_index_frees;
static void *promotion_owned_index;
static void *promotion_borrowed_index;
static int promotion_borrowed_frees;

static void *
promotion_malloc(size_t bytes)
{
    void *result;

    /* Fail exactly the image-sized RGB float buffer once. Later allocations
     * remain available, so an incorrect byte retry can actually complete. */
    if (promotion_armed && bytes == PROMOTION_PIXELS * 3 * sizeof(float)) {
        promotion_armed = 0;
        ++promotion_hits;
        return NULL;
    }
    result = malloc(bytes);
    if (promotion_armed && bytes == PROMOTION_PIXELS * sizeof(sixel_index_t)) {
        promotion_owned_index = result;
    }
    return result;
}

static void
promotion_free(void *pointer)
{
    if (pointer != NULL && pointer == promotion_borrowed_index) {
        ++promotion_borrowed_frees;
        return;
    }
    if (pointer != NULL && pointer == promotion_owned_index) {
        ++promotion_index_frees;
        promotion_owned_index = NULL;
    }
    free(pointer);
}

static int
promotion_setup(sixel_dither_t **dither, sixel_allocator_t *allocator,
                int float_palette, int preference, int method, int clamp)
{
    unsigned char bytes[6] = {51, 51, 51, 103, 103, 103};
    float values[6] = {.199f, .199f, .199f, .402f, .402f, .402f};
    sixel_palette_float32_entries_request_t request;

    if (SIXEL_FAILED(sixel_dither_new(dither, 2, allocator))) {
        return 0;
    }
    sixel_dither_set_palette(*dither, bytes);
    sixel_dither_set_pixelformat(*dither, SIXEL_PIXELFORMAT_RGB888);
    sixel_dither_set_diffusion_type(*dither, method);
    sixel_dither_set_lut_policy(*dither, SIXEL_LUT_POLICY_NONE);
    (*dither)->force_palette = 1;
    (*dither)->prefer_float32 = preference;
    if (SIXEL_FAILED(sixel_dither_set_clamp_policy(*dither, clamp, 1.01))) {
        return 0;
    }
    if (float_palette) {
        memset(&request, 0, sizeof(request));
        request.entries = values;
        request.colors = 2;
        request.depth = 3 * sizeof(float);
        if (SIXEL_FAILED((*dither)->palette->vtbl->init_entries_float32(
                (*dither)->palette, &request))) {
            return 0;
        }
    }
    return 1;
}

static int
promotion_palette_unchanged(sixel_dither_t *dither, int float_palette)
{
    float const values[6] = {.199f, .199f, .199f, .402f, .402f, .402f};
    sixel_palette_float32_entries_view_t view;

    memset(&view, 0, sizeof(view));
    if (SIXEL_FAILED(dither->palette->vtbl->get_entries_float32(
            dither->palette, &view))) {
        return 0;
    }
    /* No arithmetic is permitted on an authoritative stored palette when
     * setup fails before lookup. Its stored binary32 values are identical. */
    return float_palette ? view.entries != NULL && view.entry_count == 2 &&
        view.depth == 3 * sizeof(float) &&
        memcmp(view.entries, values, sizeof(values)) == 0 :
        view.entries == NULL;
}

static int
promotion_case(int scenario, int method, int clamp)
{
    unsigned char const source[PROMOTION_PIXELS * 3] = {
        77, 77, 77, 51, 51, 51, 103, 103, 103, 77, 77, 77,
        51, 51, 51, 103, 103, 103, 77, 77, 77
    };
    /* The float palette midpoint is .3005, below 77/255. Its compatibility
     * bytes have midpoint 77: the byte nearest search chooses the first
     * slot at the tie. These literal indices distinguish both algorithms. */
    sixel_index_t const expected[PROMOTION_PIXELS] = {1, 0, 1, 1, 0, 1, 1};
    sixel_index_t const expected_byte[PROMOTION_PIXELS] = {0, 0, 1, 0, 0, 1, 0};
    unsigned char bytes[6] = {51, 51, 51, 103, 103, 103};
    unsigned char pixels[sizeof(source)];
    sixel_index_t borrowed[PROMOTION_PIXELS];
    sixel_index_t saved[PROMOTION_PIXELS];
    sixel_allocator_t *allocator;
    sixel_dither_t *dither;
    sixel_dither_t *other;
    sixel_index_t *indices;
    int float_palette;
    int ok;
    int i;

    allocator = NULL;
    dither = NULL;
    other = NULL;
    indices = NULL;
    ok = 0;
    float_palette = scenario < 2 || scenario == 4 ||
        scenario == 5 || scenario == 6;
    promotion_armed = 0;
    promotion_hits = 0;
    promotion_index_frees = 0;
    promotion_borrowed_frees = 0;
    promotion_owned_index = NULL;
    promotion_borrowed_index = NULL;
    memcpy(pixels, source, sizeof(source));
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, promotion_malloc,
            NULL, NULL, promotion_free)) ||
            !promotion_setup(&dither, allocator, float_palette,
                scenario == 2 || scenario == 3, method, clamp)) {
        goto end;
    }
    if (scenario == 4) {
        /* An explicit byte assignment intentionally discards the float
         * view, even when the assigned compatibility bytes are unchanged. */
        sixel_dither_set_palette(dither, bytes);
        float_palette = 0;
    }
    if (scenario == 6) {
        for (i = 0; i < PROMOTION_PIXELS; ++i) {
            borrowed[i] = (sixel_index_t)19;
        }
        memcpy(saved, borrowed, sizeof(saved));
        dither->pipeline_index_buffer = borrowed;
        dither->pipeline_index_size = sizeof(borrowed);
        promotion_borrowed_index = borrowed;
    }
    promotion_armed = 1;
    indices = sixel_dither_apply_palette(dither, pixels,
        PROMOTION_PIXELS, 1);
    if (scenario == 4) {
        /* This byte-eligible application must never request a float image.
         * Hard FS may modify source error state; NONE has literal picks. */
        ok = promotion_hits == 0 && promotion_armed == 1 &&
            indices != NULL && promotion_palette_unchanged(dither, 0) &&
            dither->interframe_state.last_apply_status == SIXEL_OK &&
            dither->interframe_state.depth == 3 &&
            (method != SIXEL_DIFFUSE_NONE ||
             memcmp(indices, expected_byte, sizeof(expected_byte)) == 0);
        goto end;
    }
    ok = indices == NULL && promotion_hits == 1 &&
        dither->interframe_state.last_apply_status == SIXEL_BAD_ALLOCATION &&
        dither->interframe_state.consume_count == 0 &&
        promotion_index_frees == (scenario == 6 ? 0 : 1) &&
        dither->pipeline_index_buffer == NULL &&
        dither->pipeline_index_owned == 0 &&
        dither->dither_policy == NULL && dither->lookup_policy == NULL &&
        dither->use_rgb101_palette == 0 &&
        memcmp(pixels, source, sizeof(source)) == 0 &&
        promotion_palette_unchanged(dither, float_palette);
    if (scenario == 6) {
        ok = ok && promotion_borrowed_frees == 0 &&
            memcmp(borrowed, saved, sizeof(saved)) == 0;
    }
    if (!ok || (scenario != 5 && scenario != 6)) {
        goto end;
    }
    if (scenario == 5) {
        /* A separate byte object remains usable while the failed object
         * retains its authoritative palette for a later float retry. */
        if (!promotion_setup(&other, allocator, 0, 0,
                SIXEL_DIFFUSE_NONE, SIXEL_DITHER_CLAMP_HARD)) {
            ok = 0;
            goto end;
        }
        indices = sixel_dither_apply_palette(other, pixels,
            PROMOTION_PIXELS, 1);
        ok = indices != NULL &&
            memcmp(indices, expected_byte, sizeof(expected_byte)) == 0 &&
            other->interframe_state.depth == 3;
        sixel_allocator_free(allocator, indices);
        indices = NULL;
    } else {
        dither->pipeline_index_buffer = borrowed;
        dither->pipeline_index_size = sizeof(borrowed);
    }
    indices = sixel_dither_apply_palette(dither, pixels,
        PROMOTION_PIXELS, 1);
    ok = ok && indices != NULL &&
        (scenario != 6 || indices == borrowed) &&
        memcmp(indices, expected, sizeof(expected)) == 0 &&
        memcmp(pixels, source, sizeof(source)) == 0 &&
        promotion_palette_unchanged(dither, 1) &&
        dither->interframe_state.last_apply_status == SIXEL_OK &&
        dither->interframe_state.consume_count == 1 &&
        dither->interframe_state.depth == 3 * sizeof(float) &&
        promotion_hits == 1 && promotion_borrowed_frees == 0;
end:
    promotion_armed = 0;
    if (indices != NULL && indices != borrowed) {
        sixel_allocator_free(allocator, indices);
    }
    sixel_dither_unref(other);
    sixel_dither_unref(dither);
    sixel_allocator_unref(allocator);
    if (!ok) {
        fprintf(stderr, "promotion case=%d method=%d hits=%d frees=%d\n",
            scenario, method, promotion_hits, promotion_index_frees);
    }
    return ok;
}

/* Public initialization takes its precision from the supplied pixel format,
 * replacing the preceding request rather than retaining an old float hint. */
static int
promotion_initialize(void)
{
    unsigned char bytes[PROMOTION_PIXELS * 3] = {
        51, 51, 51, 103, 103, 103, 51, 51, 51, 103, 103, 103,
        51, 51, 51, 103, 103, 103, 51, 51, 51
    };
    float values[PROMOTION_PIXELS * 3] = {
        .199f, .199f, .199f, .402f, .402f, .402f,
        .199f, .199f, .199f, .402f, .402f, .402f,
        .199f, .199f, .199f, .402f, .402f, .402f, .199f, .199f, .199f
    };
    double const expected[2] = {.199, .402};
    sixel_allocator_t *allocator;
    sixel_dither_t *dither;
    sixel_palette_float32_entries_view_t view;
    SIXELSTATUS status;
    int first;
    int row;
    int axis;
    int ok;

    allocator = NULL;
    dither = NULL;
    ok = 0;
    promotion_armed = 0;
    promotion_hits = 0;
    promotion_owned_index = NULL;
    promotion_borrowed_index = NULL;
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, promotion_malloc,
            NULL, NULL, promotion_free)) ||
            !promotion_setup(&dither, allocator, 0, 0,
                SIXEL_DIFFUSE_NONE, SIXEL_DITHER_CLAMP_HARD)) {
        goto end;
    }
    sixel_dither_set_final_merge(dither, SIXEL_FINAL_MERGE_NONE);
    status = sixel_dither_initialize(dither, (unsigned char *)values,
        PROMOTION_PIXELS, 1, SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_LARGE_NORM,
        SIXEL_REP_AVERAGE_PIXELS, SIXEL_QUALITY_FULL);
    memset(&view, 0, sizeof(view));
    ok = status == SIXEL_OK && dither->prefer_float32 == 1 &&
        dither->ncolors == 2 &&
        SIXEL_SUCCEEDED(dither->palette->vtbl->get_entries_float32(
            dither->palette, &view)) && view.entries != NULL &&
        view.entry_count == 2 && view.depth == 3 * sizeof(float);
    if (!ok) {
        goto end;
    }
    first = view.entries[0] < view.entries[3] ? 0 : 1;
    for (row = 0; row < 2; ++row) {
        for (axis = 0; axis < 3; ++axis) {
            /* Two source levels and two slots retain those exact levels.
             * Allow solver scale/unscale and binary32 storage error only. */
            if (!isfinite(view.entries[((first + row) % 2) * 3 + axis]) ||
                    fabs(view.entries[((first + row) % 2) * 3 + axis] -
                        expected[row]) > 2e-6) {
                ok = 0;
            }
        }
    }
    promotion_armed = 1;
    status = sixel_dither_initialize(dither, bytes, PROMOTION_PIXELS, 1,
        SIXEL_PIXELFORMAT_RGB888, SIXEL_LARGE_NORM, SIXEL_REP_AVERAGE_PIXELS,
        SIXEL_QUALITY_FULL);
    ok = ok && status == SIXEL_OK && dither->prefer_float32 == 0 &&
        dither->pixelformat == SIXEL_PIXELFORMAT_RGB888 &&
        dither->ncolors == 2 && dither->use_rgb101_palette == 0 &&
        promotion_hits == 0 && promotion_armed == 1 &&
        promotion_palette_unchanged(dither, 0);
end:
    promotion_armed = 0;
    sixel_dither_unref(dither);
    sixel_allocator_unref(allocator);
    if (!ok) {
        fprintf(stderr, "promotion initialize hits=%d\n", promotion_hits);
    }
    return ok;
}

int
test_precision_0009(int argc, char **argv)
{
    int scenario;
    int method;

    scenario = argc == 2 ? atoi(argv[1]) : -1;
    if (scenario < 0 || scenario > 8) {
        return 1;
    }
    if (scenario == 8) {
        return promotion_initialize() ? 0 : 1;
    }
    method = scenario == 1 || scenario == 3 || scenario == 7 ?
        SIXEL_DIFFUSE_FS : SIXEL_DIFFUSE_NONE;
    if (scenario == 7) {
        return promotion_case(scenario, method, SIXEL_DITHER_CLAMP_SOFT) &&
            promotion_case(scenario, method, SIXEL_DITHER_CLAMP_NONE) ? 0 : 1;
    }
    if (scenario == 4 && !promotion_case(scenario, SIXEL_DIFFUSE_FS,
            SIXEL_DITHER_CLAMP_HARD)) {
        return 1;
    }
    return promotion_case(scenario, method, SIXEL_DITHER_CLAMP_HARD) ? 0 : 1;
}
