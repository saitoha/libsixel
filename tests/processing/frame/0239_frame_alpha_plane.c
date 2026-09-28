/* SPDX-License-Identifier: MIT */
/* Independent coverage, ownership and delivery checks for straight alpha. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include <6cells.h>
#include "src/frame-private.h"

static int fa_live;
static int fa_fail;

static void *
fa_malloc(size_t size)
{
    void *p;

    if (fa_fail) {
        return NULL;
    }
    p = malloc(size);
    fa_live += p != NULL;
    return p;
}

static void *
fa_calloc(size_t n, size_t size)
{
    void *p;

    p = fa_malloc(n * size);
    if (p != NULL) {
        memset(p, 0, n * size);
    }
    return p;
}

static void *
fa_realloc(void *p, size_t size)
{
    void *q;
    int empty;

    if (fa_fail) {
        return NULL;
    }
    empty = p == NULL;
    q = realloc(p, size);
    fa_live += empty && q != NULL;
    return q;
}

static void
fa_free(void *p)
{
    fa_live -= p != NULL;
    free(p);
}

int
test_frame_0239_alpha(int argc, char **argv)
{
    static float const source[12] = {
        -0.5f, 1.5f, 0.25f, 0.25f, 0.5f, 0.75f,
        1.25f, -0.25f, 0.5f, 0.125f, 0.25f, 0.5f
    };
    static float const coverage[4] = {0.5f, 0.0f, 1.0f, 0.25f};
    static float const bg[3] = {0.25f, 0.5f, 0.75f};
    sixel_allocator_t *allocator;
    sixel_frame_t *frame;
    sixel_frame_interface_t *iface;
    sixel_frame_interface_t *clone;
    sixel_frame_pixels_view_t pixels;
    sixel_frame_transparency_t mask;
    sixel_frame_alpha_t alpha;
    SIXELSTATUS status;
    float storage[12];
    float *plane;
    float *adopted;
    size_t bytes;
    size_t i;
    int c;
    int mode;
    int result;
    double expected;

    if (argc != 2) {
        return EXIT_FAILURE;
    }
    mode = atoi(argv[1]);
    allocator = NULL;
    frame = NULL;
    clone = NULL;
    plane = NULL;
    result = EXIT_FAILURE;
    fa_live = fa_fail = 0;
    memcpy(storage, source, sizeof(storage));
    status = sixel_allocator_new(&allocator, fa_malloc, fa_calloc,
                                  fa_realloc, fa_free);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_frame_new(&frame, allocator);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    status = sixel_frame_init_borrowed(frame, storage, 2, 2,
        SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, NULL, -1);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    iface = sixel_frame_as_interface(frame);
    plane = (float *)fa_malloc(sizeof(coverage));
    if (plane == NULL) {
        goto end;
    }
    memcpy(plane, coverage, sizeof(coverage));
    alpha.values = plane;
    alpha.count = mode == 1 ? 3u : 4u;
    if (mode == 2) {
        plane[3] = NAN;
    } else if (mode == 3) {
        plane[3] = 1.01f;
    }
    adopted = plane;
    status = iface->vtbl->set_alpha(iface, &alpha);
    if (mode >= 1 && mode <= 3) {
        if (status != (mode == 1 ? SIXEL_BAD_ARGUMENT : SIXEL_BAD_INPUT)) {
            goto end;
        }
        if (mode == 3) {
            plane[3] = -0.01f;
            if (iface->vtbl->set_alpha(iface, &alpha) != SIXEL_BAD_INPUT) {
                goto end;
            }
        }
        iface->vtbl->get_alpha(iface, &alpha);
        result = alpha.values == NULL && alpha.count == 0u
            ? EXIT_SUCCESS : EXIT_FAILURE;
        goto end;
    }
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    plane = NULL;
    if (mode == 4) {
        status = iface->vtbl->clone(iface, allocator, &clone);
        if (SIXEL_FAILED(status)) {
            goto end;
        }
        clone->vtbl->get_alpha(clone, &alpha);
        if (alpha.values == adopted || alpha.count != 4u ||
            memcmp(alpha.values, coverage, sizeof(coverage)) != 0) {
            goto end;
        }
        alpha.values[0] = 0.0f;
    } else if (mode == 5) {
        status = iface->vtbl->measure_storage(iface, &bytes);
        if (SIXEL_FAILED(status) || bytes != 16u * sizeof(float)) {
            goto end;
        }
    } else if (mode == 6) {
        status = iface->vtbl->clip(iface, 1, 0, 1, 2);
        iface->vtbl->get_alpha(iface, &alpha);
        iface->vtbl->get_pixels(iface, &pixels);
        if (SIXEL_FAILED(status) || alpha.count != 2u ||
            alpha.values[0] != coverage[1] ||
            alpha.values[1] != coverage[3] ||
            memcmp(pixels.pixels_float32, source + 3, 3u * sizeof(float)) ||
            memcmp(pixels.pixels_float32 + 3, source + 9, 3u * sizeof(float))) {
            goto end;
        }
        result = EXIT_SUCCESS;
        goto end;
    } else if (mode == 7 || mode == 8) {
        status = iface->vtbl->set_pixelformat(iface, mode == 7
            ? SIXEL_PIXELFORMAT_RGBFLOAT32 : SIXEL_PIXELFORMAT_RGB888);
        if (mode == 7 ? SIXEL_FAILED(status) : status != SIXEL_BAD_ARGUMENT) {
            goto end;
        }
    } else if (mode >= 9 && mode <= 13) {
        if (mode == 12) {
            fa_fail = 1;
        } else if (mode == 13) {
            storage[11] = NAN;
        }
        status = sixel_frame_resolve_alpha(frame, mode == 9 ? NULL : bg,
                                           mode != 11);
        fa_fail = 0;
        iface->vtbl->get_pixels(iface, &pixels);
        if (mode >= 12) {
            if (status != (mode == 12 ? SIXEL_BAD_ALLOCATION : SIXEL_BAD_INPUT)
                || pixels.pixels_float32 != storage ||
                memcmp(storage, source, mode == 13 ? 11u * sizeof(float)
                                                  : sizeof(source)) != 0) {
                goto end;
            }
        } else {
            if (SIXEL_FAILED(status)) {
                goto end;
            }
            iface->vtbl->get_alpha(iface, &alpha);
            iface->vtbl->get_transparency(iface, &mask);
            if (alpha.values != NULL || alpha.count != 0u ||
                (mode == 11 ? mask.transparent_mask != NULL
                 : mask.transparent_mask == NULL ||
                   mask.transparent_mask_size != 4u)) {
                goto end;
            }
            for (i = 0u; i < 4u; ++i) {
                if (mode != 11 && mask.transparent_mask[i] != (i == 1u)) {
                    goto end;
                }
                for (c = 0; c < 3; ++c) {
                    expected = source[i * 3u + c];
                    if (mode != 9 && (mode == 11 || coverage[i] != 0.0f)) {
                        expected = expected * coverage[i] +
                            bg[c] * (1.0 - coverage[i]);
                    }
                    /* Products and sums here are exact binary fractions. */
                    if (pixels.pixels_float32[i * 3u + c] != expected) {
                        goto end;
                    }
                }
            }
            result = memcmp(storage, source, sizeof(storage)) == 0
                ? EXIT_SUCCESS : EXIT_FAILURE;
            goto end;
        }
    } else if (mode == 14) {
        status = iface->vtbl->resize_float32(iface, 4, 4, SIXEL_RES_BILINEAR);
        if (status != SIXEL_BAD_ARGUMENT) {
            goto end;
        }
    } else if (mode == 16) {
        memset(&mask, 0, sizeof(mask));
        mask.transparent = 0;
        status = iface->vtbl->set_transparency(iface, &mask);
        if (status != SIXEL_BAD_ARGUMENT) {
            goto end;
        }
    } else if (mode == 15) {
        status = sixel_frame_init_borrowed(frame, storage, 2, 2,
            SIXEL_PIXELFORMAT_LINEARRGBFLOAT32, NULL, -1);
        iface->vtbl->get_alpha(iface, &alpha);
        result = SIXEL_SUCCEEDED(status) && alpha.values == NULL
            && alpha.count == 0u ? EXIT_SUCCESS : EXIT_FAILURE;
        goto end;
    }
    iface->vtbl->get_alpha(iface, &alpha);
    result = alpha.values == adopted && alpha.count == 4u &&
        memcmp(alpha.values, coverage, sizeof(coverage)) == 0
        ? EXIT_SUCCESS : EXIT_FAILURE;
end:
    fa_fail = 0;
    fa_free(plane);
    if (clone != NULL) {
        clone->vtbl->unref(clone);
    }
    sixel_frame_unref(frame);
    sixel_allocator_unref(allocator);
    if (fa_live != 0) {
        fprintf(stderr, "alpha test leaked %d allocations\n", fa_live);
        result = EXIT_FAILURE;
    }
    return result;
}
