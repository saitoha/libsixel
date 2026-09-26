/*
 * SPDX-License-Identifier: MIT
 * Policy: docs/concepts/pixelformat-precision/README.md
 */
/* Geometry must move or replace pixels and their mask as one frame state. */
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include <6cells.h>
#include "src/filter-clip.h"

/* Allocator callbacks have no userdata; each runner process owns this state. */
static int fg_allocation;
static int fg_fail_at;
static int fg_failed;

static void *
fg_malloc(size_t size)
{
    if (fg_fail_at != 0 && ++fg_allocation == fg_fail_at) {
        fg_failed = 1;
        return NULL;
    }
    return malloc(size);
}

static void *
fg_calloc(size_t count, size_t size)
{
    if (fg_fail_at != 0 && ++fg_allocation == fg_fail_at) {
        fg_failed = 1;
        return NULL;
    }
    return calloc(count, size);
}

static void *
fg_realloc(void *ptr, size_t size)
{
    if (fg_fail_at != 0 && ++fg_allocation == fg_fail_at) {
        fg_failed = 1;
        return NULL;
    }
    return realloc(ptr, size);
}

int
test_frame_0054_geometry(int argc, char **argv)
{
    static unsigned char const mask_bytes[9] = {1, 0, 1, 0, 1, 0, 1, 0, 1};
    static double const centers[4] = {0.0, 0.625, 1.375, 2.0};
    static int const formats[5] = {
        SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        SIXEL_PIXELFORMAT_OKLABFLOAT32, SIXEL_PIXELFORMAT_CIELABFLOAT32,
        SIXEL_PIXELFORMAT_DIN99DFLOAT32
    };
    float storage[96];
    float original[96];
    float *pixels;
    unsigned char *mask;
    unsigned char const *mask_before;
    sixel_allocator_t *allocator;
    sixel_frame_t *frame;
    sixel_frame_interface_t *iface;
    sixel_frame_pixels_view_t before;
    sixel_frame_pixels_view_t after;
    sixel_frame_transparency_t transparency;
    sixel_filter_clip_config_t config;
    sixel_filter_t filter;
    SIXELSTATUS status;
    SIXELSTATUS expected_status;
    int format;
    int mode;
    int is_resize;
    int owned;
    int transferred;
    int i;
    int x;
    int y;
    int w;
    int h;
    int result;
    double value;
    double coverage;

    if (argc != 3 && argc != 4) {
        return EXIT_FAILURE;
    }
    owned = argc == 4;
    transferred = 0;
    memset(&filter, 0, sizeof(filter));
    mode = atoi(argv[2]);
    is_resize = strcmp(argv[1], "resize") == 0;
    format = SIXEL_PIXELFORMAT_LINEARRGBFLOAT32;
    if (strcmp(argv[1], "clip") == 0) {
        if (mode < 0 || mode >= 5) {
            return EXIT_FAILURE;
        }
        format = formats[mode];
    }
    fg_allocation = 0;
    fg_fail_at = 0;
    fg_failed = 0;
    allocator = NULL;
    frame = NULL;
    mask = NULL;
    result = EXIT_FAILURE;
    /* Padding makes even the old missing-bounds check safe to probe. */
    for (i = 0; i < 96; ++i) {
        storage[i] = (float)(i - 30) / 4.0f;
    }
    pixels = storage + 12;
    if (!is_resize) {
        /* Crop is a byte move: NaN, infinity and signed zero also survive. */
        pixels[12] = (float)NAN;
        pixels[13] = (float)INFINITY;
        pixels[14] = -0.0f;
    } else if (mode == 7) {
        pixels[26] = (float)NAN;
    } else if (mode == 8) {
        pixels[0] = FLT_MAX;
        pixels[3] = -FLT_MAX;
    }
    memcpy(original, storage, sizeof(storage));
    status = sixel_allocator_new(&allocator,
        fg_malloc, fg_calloc, fg_realloc, free);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_frame_new(&frame, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (owned) {
        pixels = (float *)sixel_allocator_malloc(allocator, 27 * sizeof(float));
        if (pixels == NULL) {
            goto end;
        }
        memcpy(pixels, storage + 12, 27 * sizeof(float));
        status = sixel_frame_init(frame, (unsigned char *)pixels, 3, 3,
            format, NULL, -1);
    } else {
        status = sixel_frame_init_borrowed(frame, pixels, 3, 3,
            format, NULL, -1);
    }
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    transferred = 1;
    mask = (unsigned char *)sixel_allocator_malloc(allocator, 9);
    if (mask == NULL) {
        goto end;
    }
    memcpy(mask, mask_bytes, 9);
    memset(&transparency, 0, sizeof(transparency));
    transparency.transparent = -1;
    transparency.alpha_zero_is_transparent = 1;
    transparency.transparent_mask = mask;
    transparency.transparent_mask_size = 9;
    iface = sixel_frame_as_interface(frame);
    status = iface->vtbl->set_transparency(iface, &transparency);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    mask_before = mask;
    mask = NULL;
    status = iface->vtbl->get_pixels(iface, &before);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    expected_status = SIXEL_OK;
    x = 1;
    y = 1;
    w = 2;
    h = 1;
    if (is_resize) {
        fg_fail_at = mode <= 6 ? mode : 0;
        expected_status = mode <= 6 ? SIXEL_BAD_ALLOCATION : SIXEL_BAD_INPUT;
        if (mode == 9) {
            expected_status = SIXEL_OK;
        }
        /* Six allocations: pixels, scratch, mask RGB pair, bytes, scratch. */
        status = sixel_frame_resize_float32(frame, 4, 4,
            mode == 8 ? SIXEL_RES_BICUBIC : SIXEL_RES_BILINEAR);
        fg_fail_at = 0;
    } else if (strcmp(argv[1], "bounds") == 0) {
        expected_status = SIXEL_BAD_INPUT;
        if (mode == 0) {
            x = -1;
        } else if (mode == 1) {
            y = -1;
        } else if (mode == 2) {
            w = 3;
        } else if (mode == 3) {
            h = 3;
        } else if (mode == 4) {
            x = INT_MAX;
        } else {
            y = INT_MAX;
        }
        status = sixel_frame_clip(frame, x, y, w, h);
    } else if (strcmp(argv[1], "cli") == 0) {
        /* The filter clamps a valid origin's oversized rectangle first. */
        memset(&config, 0, sizeof(config));
        config.clip_x = mode == 1 ? 3 : 1;
        config.clip_y = 1;
        config.clip_width = 20;
        config.clip_height = 1;
        status = sixel_filter_clip_init(&filter, &config);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        filter.input.slot = &frame;
        status = filter.vtbl->apply(&filter, allocator, NULL);
    } else {
        status = sixel_frame_clip(frame, x, y, w, h);
    }
    if (status != expected_status
            || (is_resize && mode <= 6 && fg_failed != 1)) {
        fprintf(stderr, "geometry status %x expected %x, allocation %d\n",
            status, expected_status, fg_allocation);
        goto end;
    }
    status = iface->vtbl->get_pixels(iface, &after);
    if (SIXEL_FAILED(status) || after.pixels != NULL
            || (mode == 9 && is_resize
                ? after.pixels_float32 == before.pixels_float32
                : after.pixels_float32 != before.pixels_float32)
            || after.pixelformat != before.pixelformat
            || after.colorspace != before.colorspace) {
        goto end;
    }
    status = iface->vtbl->get_transparency(iface, &transparency);
    if (SIXEL_FAILED(status) || !transparency.alpha_zero_is_transparent
            || (mode == 9 && is_resize
                ? transparency.transparent_mask == mask_before
                : transparency.transparent_mask != mask_before)) {
        goto end;
    }
    if (is_resize && mode == 9) {
        if (after.width != 4 || after.height != 4
                || transparency.transparent_mask_size != 16) {
            goto end;
        }
        /* Independent bilinear interpolation of a ramp and a checker mask. */
        for (i = 0; i < 48; ++i) {
            x = (i / 3) % 4;
            y = i / 12;
            value = -4.5 + 2.25 * centers[y] + 0.75 * centers[x]
                + 0.25 * (i % 3);
            if (!isfinite(after.pixels_float32[i])
                    || fabs(after.pixels_float32[i] - value) > 2e-6) {
                goto end;
            }
            coverage = fabs(centers[x] - 1.0) * fabs(centers[y] - 1.0)
                + (1.0 - fabs(centers[x] - 1.0))
                * (1.0 - fabs(centers[y] - 1.0));
            if (transparency.transparent_mask[i / 3]
                    != (coverage >= 0.5 ? 1 : 0)) {
                goto end;
            }
        }
    } else if (SIXEL_FAILED(expected_status)
            || (strcmp(argv[1], "cli") == 0 && mode == 1)) {
        if (after.width != 3 || after.height != 3
                || memcmp(storage, original, sizeof(storage)) != 0
                || memcmp(pixels, original + 12, 27 * sizeof(float)) != 0
                || transparency.transparent_mask_size != 9
                || memcmp(transparency.transparent_mask, mask_bytes, 9) != 0) {
            goto end;
        }
    } else {
        if (after.width != 2 || after.height != 1
                || memcmp(pixels, original + 24, 6 * sizeof(float)) != 0
                || transparency.transparent_mask_size != 2
                || transparency.transparent_mask[0] != 1
                || transparency.transparent_mask[1] != 0) {
            goto end;
        }
    }
    result = EXIT_SUCCESS;
end:
    fg_fail_at = 0;
    if (filter.vtbl != NULL) {
        filter.vtbl->dispose(&filter);
    }
    if (owned && !transferred && pixels != storage + 12) {
        sixel_allocator_free(allocator, pixels);
    }
    sixel_allocator_free(allocator, mask);
    sixel_frame_unref(frame);
    sixel_allocator_unref(allocator);
    return result;
}
