/*
 * SPDX-License-Identifier: MIT
 *
 * Policy: docs/encoder/palette-application/lookup-policy/ordered.md
 *
 * Distance-ordered candidates must preserve exhaustive results and the
 * lowest-index tie rule, including reverse strides and short tails.
 * Float selection delegates to the existing exact blockcull backend.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <limits.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>
#include <6cells.h>

#include "src/factory.h"

enum {
    ORDERED_EXACT_QUERIES = 8193
};

static unsigned int
ordered_exact_next(unsigned int *state)
{
    *state = *state * 1664525U + 1013904223U;
    return *state >> 8;
}

/* Random colors; every fifth entry repeats an earlier one so duplicate
 * coordinates must resolve to the lower index. */
static void
ordered_exact_palette(unsigned char *palette, int ncolors,
                        unsigned int seed)
{
    unsigned int state;
    int i;

    state = seed;
    for (i = 0; i < ncolors * 3; ++i) {
        palette[i] = (unsigned char)(ordered_exact_next(&state) & 255U);
    }
    /* The midpoint query must retain an equality at the radius cutoff. */
    if (ncolors == 2) {
        memset(palette, 0, 6);
        palette[3] = 2;
    }
    if (ncolors == 129) {
        memset(palette, 42, (size_t)ncolors * 3U);
    }
    for (i = 5; i < ncolors; i += 5) {
        memcpy(palette + i * 3, palette + (i - 5) * 3, 3);
    }
}

static void
ordered_exact_pixels(unsigned char *pixels, unsigned char const *palette,
                       int ncolors, unsigned int seed)
{
    unsigned int state;
    int q;
    int c;
    int a;
    int b;
    int cell;

    state = seed;
    for (q = 0; q < ORDERED_EXACT_QUERIES; ++q) {
        cell = q / 8;
        /* Every cell corner, including the closed 31/32 boundaries. */
        if (q < 4096) {
            pixels[q * 3 + 0] = (unsigned char)
                ((cell >> 6) * 32 + ((q & 1) != 0 ? 31 : 0));
            pixels[q * 3 + 1] = (unsigned char)
                (((cell >> 3) & 7) * 32 + ((q & 2) != 0 ? 31 : 0));
            pixels[q * 3 + 2] = (unsigned char)
                ((cell & 7) * 32 + ((q & 4) != 0 ? 31 : 0));
            continue;
        }
        a = (int)(ordered_exact_next(&state) % (unsigned int)ncolors);
        b = (int)(ordered_exact_next(&state) % (unsigned int)ncolors);
        for (c = 0; c < 3; ++c) {
            switch ((q / 97) % 4) {
            case 0:
                /* Smooth ramp: narrow boxes. */
                pixels[q * 3 + c] = (unsigned char)((q * (c + 1)) / 7);
                break;
            case 1:
                pixels[q * 3 + c] = (unsigned char)
                    (ordered_exact_next(&state) & 255U);
                break;
            case 2:
                pixels[q * 3 + c] = palette[a * 3 + c];
                break;
            default:
                pixels[q * 3 + c] = (unsigned char)
                    (((palette[a * 3 + c] & 0xfe)
                      + (palette[b * 3 + c] & 0xfe)) / 2);
                break;
            }
        }
    }
}

static int
ordered_exact_byte_oracle(unsigned char const *palette, int ncolors,
                            unsigned char const *pixel)
{
    int best;
    int minimum;
    int distance;
    int d;
    int i;
    int c;

    best = 0;
    minimum = INT_MAX;
    for (i = 0; i < ncolors; ++i) {
        distance = 0;
        for (c = 0; c < 3; ++c) {
            d = (int)pixel[c] - (int)palette[i * 3 + c];
            distance += d * d;
        }
        if (distance < minimum) {
            minimum = distance;
            best = i;
        }
    }
    return best;
}

static sixel_lookup_policy_interface_t *
ordered_exact_create(char const *class_name, sixel_allocator_t *allocator)
{
    sixel_factory_t *factory;
    sixel_lookup_policy_interface_t *policy;
    void *service;

    policy = NULL;
    service = NULL;
    if (SIXEL_FAILED(sixel_components_getservice("services/factory",
                                                 &service))) {
        return NULL;
    }
    factory = (sixel_factory_t *)service;
    if (SIXEL_FAILED(factory->vtbl->create(factory, class_name, allocator,
                                           (void **)&policy))) {
        policy = NULL;
    }
    factory->vtbl->unref(factory);
    return policy;
}

static int
ordered_exact_run(sixel_allocator_t *allocator, int is_float, int ncolors)
{
    static unsigned char pixels[ORDERED_EXACT_QUERIES * 3];
    static float pixels_float[ORDERED_EXACT_QUERIES * 3];
    static sixel_index_t actual[ORDERED_EXACT_QUERIES];
    unsigned char palette[SIXEL_PALETTE_MAX * 3];
    float palette_float[SIXEL_PALETTE_MAX * 3];
    sixel_lookup_policy_interface_t *policy;
    sixel_lookup_policy_interface_t *reference;
    sixel_lookup_policy_prepare_request_t request;
    unsigned char const *query;
    int expected;
    int stride;
    int direction;
    int start;
    int length;
    int q;
    int i;
    int ok;

    ok = 0;
    reference = NULL;
    policy = ordered_exact_create(is_float ? "lookup/ordered.float32"
                                             : "lookup/ordered.8bit",
                                    allocator);
    if (policy == NULL) {
        fprintf(stderr, "ordered: create failed\n");
        return 0;
    }
    if (is_float) {
        reference = ordered_exact_create("lookup/vptree.float32",
                                           allocator);
        if (reference == NULL) {
            fprintf(stderr, "ordered: reference create failed\n");
            goto end;
        }
    }
    ordered_exact_palette(palette, ncolors, (unsigned int)ncolors * 7U);
    ordered_exact_pixels(pixels, palette, ncolors,
                           (unsigned int)ncolors * 13U + 1U);
    if (ncolors == 2) {
        pixels[0] = 1;
        pixels[1] = pixels[2] = 0;
    }
    for (i = 0; i < ncolors * 3; ++i) {
        palette_float[i] = (float)palette[i] / 255.0f;
    }
    for (q = 0; q < ORDERED_EXACT_QUERIES * 3; ++q) {
        pixels_float[q] = (float)pixels[q] / 255.0f;
    }
    if (is_float) {
        /* Nonfinite and far out-of-range samples disable culling for
         * their run without changing the exhaustive answer. */
        pixels_float[600 * 3 + 0] = NAN;
        pixels_float[601 * 3 + 1] = INFINITY;
        pixels_float[602 * 3 + 2] = -1.0e30f;
        pixels_float[603 * 3 + 0] = 3.0e38f;
    }

    memset(&request, 0, sizeof(request));
    request.palette = palette;
    request.depth = 3;
    request.reqcolor = ncolors;
    request.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    request.allocator = allocator;
    if (is_float) {
        request.palette_float = palette_float;
        request.float_depth = 3 * (int)sizeof(float);
        request.pixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
    }
    if (SIXEL_FAILED(policy->vtbl->prepare(policy, &request))
            || (reference != NULL
                && SIXEL_FAILED(reference->vtbl->prepare(reference,
                                                         &request)))) {
        fprintf(stderr, "ordered: prepare failed\n");
        goto end;
    }
    stride = is_float ? 3 * (int)sizeof(float) : 3;
    query = is_float ? (unsigned char const *)(void const *)pixels_float
                     : pixels;
    for (direction = -1; direction <= 1; direction += 2) {
        for (length = 1; length <= ORDERED_EXACT_QUERIES;
                length = length < 70 ? length + 1
                                     : ORDERED_EXACT_QUERIES) {
            start = direction == 1 ? 0 : length - 1;
            memset(actual, 0xff, sizeof(actual));
            policy->vtbl->map_pixels(policy, query + start * stride,
                (size_t)length, direction * stride, actual + start,
                direction, NULL);
            for (q = 0; q < length; ++q) {
                expected = is_float
                    ? policy->vtbl->map_pixel(policy, query + q * stride)
                    : ordered_exact_byte_oracle(palette, ncolors,
                                                  pixels + q * 3);
                if ((!is_float && policy->vtbl->map_pixel_hint(policy,
                        query + q * stride, ncolors - 1) != expected)
                        || (int)actual[q] != expected
                        || policy->vtbl->map_pixel(policy,
                               query + q * stride) != expected
                        || (reference != NULL
                            && (q < 600 || q > 603)
                            && reference->vtbl->map_pixel(reference,
                                   query + q * stride) != expected)) {
                    fprintf(stderr,
                            "ordered %s colors=%d length=%d dir=%d "
                            "q=%d expected=%d actual=%d\n",
                            is_float ? "float32" : "8bit", ncolors, length,
                            direction, q, expected, (int)actual[q]);
                    goto end;
                }
            }
            if (length == ORDERED_EXACT_QUERIES) {
                break;
            }
        }
    }
    if (!is_float) {
        /* Reuse unchanged data, then invalidate at the same palette size. */
        if (SIXEL_FAILED(policy->vtbl->prepare(policy, &request))) {
            goto end;
        }
        palette[0] ^= 255U;
        if (SIXEL_FAILED(policy->vtbl->prepare(policy, &request))) {
            goto end;
        }
        for (q = 0; q < ORDERED_EXACT_QUERIES; ++q) {
            expected = ordered_exact_byte_oracle(palette, ncolors,
                                                  pixels + q * 3);
            if (policy->vtbl->map_pixel(policy, pixels + q * 3) != expected) {
                fprintf(stderr, "ordered palette invalidation failed\n");
                goto end;
            }
        }
    }
    ok = 1;
end:
    if (reference != NULL) {
        reference->vtbl->unref(reference);
    }
    policy->vtbl->unref(policy);
    return ok;
}

int
test_lookup_0039_ordered_exact(int argc, char **argv)
{
    static int const sizes[] = {1, 2, 7, 33, 64, 129, 200, 256};
    sixel_allocator_t *allocator;
    int is_float;
    size_t i;
    int ok;

    allocator = NULL;
    is_float = argc > 1 && strcmp(argv[1], "float") == 0;
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL,
                                         NULL))) {
        return EXIT_FAILURE;
    }
    ok = 1;
    for (i = 0; ok && i < sizeof(sizes) / sizeof(sizes[0]); ++i) {
        ok = ordered_exact_run(allocator, is_float, sizes[i]);
    }
    sixel_allocator_unref(allocator);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
