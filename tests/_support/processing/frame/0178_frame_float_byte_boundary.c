/* SPDX-License-Identifier: MIT */
/* Exact byte output and atomic ownership at the explicit float boundary. */
#include <float.h>
#include <limits.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include <6cells.h>
#include "tests/data/color-numeric/frame-byte.h"

/* Allocator callbacks have no userdata; one test process owns this state. */
static int fb_live;
static int fb_fail;
static int fb_failed;
static int fb_source_owned;
static int fb_source_freed;
static void *fb_source;

static void *
fb_malloc(size_t size)
{
    void *result;

    if (fb_fail) {
        fb_failed = 1;
        return NULL;
    }
    result = malloc(size);
    fb_live += result != NULL;
    return result;
}

static void *
fb_calloc(size_t count, size_t size)
{
    void *result;

    result = fb_malloc(count * size);
    if (result != NULL) {
        memset(result, 0, count * size);
    }
    return result;
}

static void *
fb_realloc(void *pointer, size_t size)
{
    void *result;
    int was_null;

    if (fb_fail) {
        fb_failed = 1;
        return NULL;
    }
    was_null = pointer == NULL;
    result = realloc(pointer, size);
    fb_live += was_null && result != NULL;
    return result;
}

static void
fb_free(void *pointer)
{
    if (pointer != NULL) {
        if (pointer == fb_source) {
            ++fb_source_freed;
            if (!fb_source_owned || fb_source_freed > 1) {
                /* Record the ownership bug without freeing stack storage. */
                return;
            }
        }
        --fb_live;
        free(pointer);
    }
}

int
test_frame_0178_byte(int argc, char **argv)
{
    static int const formats[5] = {
        SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
        SIXEL_PIXELFORMAT_OKLABFLOAT32, SIXEL_PIXELFORMAT_CIELABFLOAT32,
        SIXEL_PIXELFORMAT_DIN99DFLOAT32
    };
    float pixels[800 * 3];
    float original[800 * 3];
    float *owned_pixels;
    unsigned char mask_bytes[800];
    unsigned char *mask;
    sixel_allocator_t *allocator;
    sixel_frame_t *frame;
    sixel_frame_interface_t *iface;
    sixel_frame_interface_t *clone;
    sixel_frame_pixels_view_t before;
    sixel_frame_pixels_view_t after;
    sixel_frame_transparency_t transparency;
    unsigned char const *mask_before;
    SIXELSTATUS status;
    SIXELSTATUS expected_status;
    uint32_t bits;
    unsigned int begin;
    unsigned int count;
    unsigned int i;
    int c;
    int format;
    int route;
    int mode;
    int result;

    if (argc != 4) {
        return EXIT_FAILURE;
    }
    format = atoi(argv[1]);
    route = atoi(argv[2]);
    mode = atoi(argv[3]);
    if (format < 0 || format > 4 || route < 0 || route > 4 ||
        mode < 0 || mode > 6) {
        return EXIT_FAILURE;
    }
    begin = color_frame_byte_offsets[format];
    count = color_frame_byte_offsets[format + 1] - begin;
    if (count > 800 || sizeof(float) != sizeof(bits)) {
        return EXIT_FAILURE;
    }
    for (i = 0u; i < count; ++i) {
        mask_bytes[i] = (unsigned char)(i % 3u == 0u);
        for (c = 0; c < 3; ++c) {
            bits = color_frame_byte[begin + i].bits[c];
            memcpy(pixels + i * 3u + c, &bits, sizeof(bits));
        }
    }
    /* A bad final triplet follows many valid conversions. Its failure must
     * not publish a prefix, edit borrowed samples, or alter any frame tags. */
    if (mode == 2) {
        pixels[count * 3u - 1u] = (float)NAN;
    } else if (mode == 3) {
        pixels[count * 3u - 1u] = (float)INFINITY;
    } else if (mode == 4) {
        pixels[count * 3u - 3u] = FLT_MAX;
        pixels[count * 3u - 2u] = 0.0f;
        pixels[count * 3u - 1u] = 0.0f;
    }
    memcpy(original, pixels, count * 3u * sizeof(float));
    fb_live = fb_fail = fb_failed = fb_source_freed = 0;
    fb_source = NULL;
    allocator = NULL;
    frame = NULL;
    clone = NULL;
    mask = NULL;
    owned_pixels = NULL;
    result = EXIT_FAILURE;
    status = sixel_allocator_new(&allocator,
        fb_malloc, fb_calloc, fb_realloc, fb_free);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_frame_new(&frame, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (route == 1 || route == 3) {
        owned_pixels = fb_malloc(count * 3u * sizeof(float));
        if (owned_pixels == NULL) {
            goto end;
        }
        memcpy(owned_pixels, pixels, count * 3u * sizeof(float));
        status = sixel_frame_init(frame, (unsigned char *)owned_pixels,
            (int)count, 1, formats[format], NULL, -1);
        if (SIXEL_SUCCEEDED(status)) {
            owned_pixels = NULL;
        }
    } else {
        status = sixel_frame_init_borrowed(frame, pixels,
            (int)count, 1, formats[format], NULL, -1);
    }
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    mask = fb_malloc(count);
    if (mask == NULL) {
        goto end;
    }
    memcpy(mask, mask_bytes, count);
    memset(&transparency, 0, sizeof(transparency));
    transparency.transparent = -1;
    transparency.alpha_zero_is_transparent = 1;
    transparency.transparent_mask = mask;
    transparency.transparent_mask_size = count;
    iface = sixel_frame_as_interface(frame);
    status = iface->vtbl->set_transparency(iface, &transparency);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    mask = NULL;
    if (route == 4) {
        status = iface->vtbl->clone(iface, allocator, &clone);
        if (SIXEL_FAILED(status) || clone == NULL) {
            goto end;
        }
        iface = clone;
    }
    if (mode == 5) {
        sixel_frame_set_width(frame, 0);
    } else if (mode == 6) {
        sixel_frame_set_width(frame, INT_MAX);
        sixel_frame_set_height(frame, INT_MAX);
    }
    status = iface->vtbl->get_pixels(iface, &before);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = iface->vtbl->get_transparency(iface, &transparency);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    mask_before = transparency.transparent_mask;
    fb_source = (void *)before.pixels_float32;
    fb_source_owned = route == 1 || route == 3 || route == 4;
    fb_fail = mode == 1;
    expected_status = mode == 0 ? SIXEL_OK :
        mode == 1 ? SIXEL_BAD_ALLOCATION : SIXEL_BAD_INPUT;
    status = route < 2
        ? sixel_frame_set_pixelformat(frame, SIXEL_PIXELFORMAT_RGB888)
        : iface->vtbl->set_pixelformat(iface, SIXEL_PIXELFORMAT_RGB888);
    fb_fail = 0;
    if (status != expected_status || fb_failed != (mode == 1) ||
        fb_source_freed != (mode == 0 && fb_source_owned)) {
        fprintf(stderr, "byte status %x expected %x, freed=%d failed=%d\n",
            status, expected_status, fb_source_freed, fb_failed);
        goto end;
    }
    status = iface->vtbl->get_pixels(iface, &after);
    if (SIXEL_FAILED(status) || after.width != before.width ||
        after.height != before.height) {
        goto end;
    }
    if (mode != 0) {
        if (after.pixels != NULL ||
            after.pixels_float32 != before.pixels_float32 ||
            after.pixelformat != before.pixelformat ||
            after.colorspace != before.colorspace ||
            memcmp(after.pixels_float32, original,
                   count * 3u * sizeof(float)) != 0) {
            fprintf(stderr, "failed byte conversion changed source state\n");
            goto end;
        }
    } else {
        if (after.pixels == NULL || after.pixels_float32 != NULL ||
            after.pixelformat != SIXEL_PIXELFORMAT_RGB888 ||
            after.colorspace != SIXEL_COLORSPACE_GAMMA) {
            goto end;
        }
        for (i = 0u; i < count; ++i) {
            for (c = 0; c < 3; ++c) {
                if (after.pixels[i * 3u + c] !=
                    color_frame_byte[begin + i].bytes[c]) {
                    fprintf(stderr, "byte sample %u channel %d: %u != %u\n",
                        i, c, after.pixels[i * 3u + c],
                        color_frame_byte[begin + i].bytes[c]);
                    goto end;
                }
            }
        }
    }
    status = iface->vtbl->get_transparency(iface, &transparency);
    if (SIXEL_FAILED(status) || transparency.transparent != -1 ||
        !transparency.alpha_zero_is_transparent ||
        transparency.transparent_mask != mask_before ||
        transparency.transparent_mask_size != count ||
        memcmp(transparency.transparent_mask, mask_bytes, count) != 0 ||
        memcmp(pixels, original, count * 3u * sizeof(float)) != 0) {
        goto end;
    }
    if (clone != NULL) {
        status = sixel_frame_as_interface(frame)->vtbl->get_pixels(
            sixel_frame_as_interface(frame), &after);
        if (SIXEL_FAILED(status) || after.pixels_float32 != pixels ||
            after.pixelformat != before.pixelformat ||
            after.colorspace != before.colorspace) {
            goto end;
        }
    }
    result = EXIT_SUCCESS;
end:
    fb_fail = 0;
    if (clone != NULL) {
        clone->vtbl->unref(clone);
    }
    sixel_frame_unref(frame);
    fb_free(mask);
    fb_free(owned_pixels);
    sixel_allocator_unref(allocator);
    if (fb_live != 0 ||
        (fb_source != NULL && fb_source_freed != fb_source_owned)) {
        fprintf(stderr, "byte ownership balance live=%d source freed=%d\n",
            fb_live, fb_source_freed);
        result = EXIT_FAILURE;
    }
    return result;
}
