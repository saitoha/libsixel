/* SPDX-License-Identifier: MIT */
/* Directly protect canonical palette values, unsnapped lookup and failure
 * isolation. The independent primary/TRC values are generated from rationals.
 */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <sixel.h>
#include <6cells.h>
#include "src/palette.h"
#include "src/lookup-policy-none.h"
#include "tests/data/color-numeric/p3-palette.h"

static int p3_fail_once;
static int p3_failure_hits;

static void *
p3_malloc(size_t size)
{
    if (p3_fail_once != 0) {
        p3_fail_once = 0;
        ++p3_failure_hits;
        return NULL;
    }
    return malloc(size);
}

static void *
p3_calloc(size_t count, size_t size)
{
    if (p3_fail_once != 0) {
        p3_fail_once = 0;
        ++p3_failure_hits;
        return NULL;
    }
    return calloc(count, size);
}

static void *
p3_realloc(void *pointer, size_t size)
{
    if (p3_fail_once != 0) {
        p3_fail_once = 0;
        ++p3_failure_hits;
        return NULL;
    }
    return realloc(pointer, size);
}

static SIXELSTATUS
p3_build(sixel_palette_t *palette,
          sixel_palette_generate_request_t const *request, int extended)
{
    sixel_palette_build_attempt_t attempt;
    SIXELSTATUS status;
    SIXELSTATUS finish;

    memset(&attempt, 0, sizeof(attempt));
    sixel_palette_binning_state_init(&attempt.binning,
        SIXEL_PALETTE_BINNING_NONE, SIXEL_PALETTE_POLICY_ORIGIN_EXPLICIT);
    status = sixel_palette_build_attempt_begin(palette, &attempt);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    status = extended != 0
        ? sixel_palette_generate_extended(palette, request)
        : palette->vtbl->generate(palette, request);
    finish = sixel_palette_build_attempt_finish(palette, &attempt);
    return SIXEL_FAILED(finish) ? finish : status;
}

int
test_palette_0051_p3_contract(int argc, char **argv)
{
    sixel_allocator_t *allocator;
    sixel_palette_t *palette;
    sixel_lookup_policy_interface_t *lookup;
    sixel_palette_generate_request_t request;
    sixel_palette_entries_request_t byte_request;
    sixel_palette_float32_entries_request_t float_request;
    sixel_palette_entries_view_t bytes;
    sixel_palette_float32_entries_view_t floats;
    sixel_lookup_policy_prepare_request_t lookup_request;
    unsigned char const *wire;
    unsigned int count;
    void *object;
    float source[3];
    float gray[6] = {0.1f, 0.1f, 0.1f, 0.11f, 0.11f, 0.11f};
    float query[3] = {0.106f, 0.106f, 0.106f};
    unsigned char gray_bytes[6] = {26, 26, 26, 28, 28, 28};
    unsigned char red_wire[3] = {100, 0, 0};
    unsigned char gray_wire[6] = {35, 35, 35, 37, 37, 37};
    SIXELSTATUS status;
    int result;
    int channel;
    int index;

    allocator = NULL;
    palette = NULL;
    lookup = NULL;
    object = NULL;
    result = EXIT_FAILURE;
    wire = NULL;
    count = 0U;
    memset(&request, 0, sizeof(request));
    memset(&lookup_request, 0, sizeof(lookup_request));
    if (argc != 2) {
        return EXIT_FAILURE;
    }
    status = sixel_allocator_new(&allocator, p3_malloc, p3_calloc,
                                   p3_realloc, free);
    if (SIXEL_FAILED(status) ||
            SIXEL_FAILED(sixel_palette_factory_new(allocator, &object))) {
        goto end;
    }
    palette = object;
    for (channel = 0; channel < 3; ++channel) {
        source[channel] = (float)color_p3_red_linear[channel];
    }
    request.data = source;
    request.length = sizeof(source);
    request.pixelformat = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
    request.requested_colors = 1;
    request.prefer_float32 = 1;
    request.quantize_model = SIXEL_QUANTIZE_MODEL_KMEANS;
    request.final_merge_mode = SIXEL_FINAL_MERGE_NONE;
    request.lut_policy = SIXEL_LUT_POLICY_NONE;

    if (strcmp(argv[1], "lookup") == 0) {
        byte_request.entries = gray_bytes;
        byte_request.colors = 2;
        byte_request.depth = 3;
        float_request.entries = gray;
        float_request.colors = 2;
        float_request.depth = 3 * sizeof(float);
        if (SIXEL_FAILED(palette->vtbl->init_entries(palette, &byte_request)) ||
                SIXEL_FAILED(palette->vtbl->init_entries_float32(palette,
                    &float_request)) ||
                SIXEL_FAILED(sixel_lookup_policy_none_float32_new(allocator,
                    &object))) {
            goto end;
        }
        lookup = object;
        if (SIXEL_FAILED(palette->vtbl->get_entries(palette, &bytes)) ||
                SIXEL_FAILED(palette->vtbl->get_entries_float32(palette,
                    &floats))) {
            goto end;
        }
        lookup_request.palette = bytes.entries;
        lookup_request.palette_float = floats.entries;
        lookup_request.depth = 3;
        lookup_request.float_depth = 3 * sizeof(float);
        lookup_request.reqcolor = 2;
        if (SIXEL_FAILED(lookup->vtbl->prepare(lookup, &lookup_request)) ||
                lookup->vtbl->map_pixel(lookup,
                    (unsigned char const *)query) != 1) {
            goto end;
        }
        if (SIXEL_FAILED(sixel_palette_prepare_rgb101(palette,
                SIXEL_COLORSPACE_LINEAR, SIXEL_PALETTE_OUTPUT_DISPLAY_P3)) ||
                SIXEL_FAILED(sixel_palette_get_rgb101(palette, &wire,
                    &count)) || count != 2 ||
                memcmp(wire, gray_wire, sizeof(gray_wire)) != 0) {
            goto end;
        }
        for (index = 0; index < 6; ++index) {
            if (floats.entries[index] != gray[index] ||
                    bytes.entries[index] != gray_bytes[index]) {
                goto end;
            }
        }
        /* The original midpoint stays .105 with snap disabled. Feeding
         * back the wire codes would move it above .106 and select entry 0. */
        if (SIXEL_FAILED(lookup->vtbl->prepare(lookup, &lookup_request)) ||
                lookup->vtbl->map_pixel(lookup,
                    (unsigned char const *)query) != 1) {
            goto end;
        }
    } else if (strcmp(argv[1], "failure-reset") == 0) {
        p3_failure_hits = 0;
        p3_fail_once = 1;
        status = p3_build(palette, &request, 1);
        if (status != SIXEL_BAD_ALLOCATION || p3_failure_hits != 1 ||
                SIXEL_SUCCEEDED(sixel_palette_get_rgb101(palette, &wire,
                    &count))) {
            goto end;
        }
        /* Both entry points preserve typed float coordinates. Recovery
         * must discard failed state without selecting a byte or clamped
         * palette. A single centroid is the literal source triple; the
         * 2e-6 budget permits double/float arithmetic, not a gamut clip. */
        if (SIXEL_FAILED(p3_build(palette, &request, 0)) ||
                SIXEL_FAILED(palette->vtbl->get_entries_float32(palette,
                    &floats)) || floats.entries == NULL) {
            goto end;
        }
        for (index = 0; index < 3; ++index) {
            if (!isfinite(floats.entries[index]) ||
                    fabs(floats.entries[index] - source[index]) > 2e-6) {
                goto end;
            }
        }
        if (SIXEL_FAILED(p3_build(palette, &request, 1)) ||
                SIXEL_FAILED(sixel_palette_prepare_rgb101(palette,
                    SIXEL_COLORSPACE_LINEAR,
                    SIXEL_PALETTE_OUTPUT_DISPLAY_P3))) {
            goto end;
        }
        memset(&float_request, 0, sizeof(float_request));
        if (SIXEL_FAILED(palette->vtbl->init_entries_float32(palette,
                &float_request)) ||
                SIXEL_SUCCEEDED(sixel_palette_get_rgb101(palette, &wire,
                    &count))) {
            goto end;
        }
    } else if (strcmp(argv[1], "values") == 0) {
        if (SIXEL_FAILED(p3_build(palette, &request, 1)) ||
                SIXEL_FAILED(palette->vtbl->get_entries_float32(palette,
                    &floats)) || floats.entries == NULL ||
                floats.entry_count != 1 ||
                memcmp(floats.entries, source, sizeof(source)) != 0) {
            goto end;
        }
        if (SIXEL_FAILED(sixel_palette_prepare_rgb101(palette,
                SIXEL_COLORSPACE_LINEAR, SIXEL_PALETTE_OUTPUT_DISPLAY_P3)) ||
                SIXEL_FAILED(sixel_palette_get_rgb101(palette, &wire,
                    &count)) || count != 1 ||
                memcmp(wire, red_wire, sizeof(red_wire)) != 0) {
            goto end;
        }
        for (channel = 0; channel < 3; ++channel) {
            if (fabs(floats.entries[channel] -
                    color_p3_red_linear[channel]) > 0.0000001) {
                goto end;
            }
        }
    } else {
        goto end;
    }
    result = EXIT_SUCCESS;
end:
    p3_fail_once = 0;
    if (lookup != NULL) {
        lookup->vtbl->unref(lookup);
    }
    if (palette != NULL) {
        palette->vtbl->unref(palette);
    }
    sixel_allocator_unref(allocator);
    if (result != EXIT_SUCCESS) {
        fprintf(stderr, "P3 palette contract failed: %s\n", argv[1]);
    }
    return result;
}
