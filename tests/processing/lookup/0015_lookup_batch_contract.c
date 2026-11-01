/*
 * SPDX-License-Identifier: MIT
 * Policy: docs/encoder/palette-application/lookup-policy/ordered.md
 *
 * Policy: docs/encoder/palette-application/lookup-policy/delaunay.md
 * Policy: docs/encoder/palette-application/lookup-policy/blockcull.md
 * Policy: docs/encoder/palette-application/lookup-policy/README.md
 * Compare ordered bulk calls against cold independent point-query state.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>
#include <6cells.h>

#include "src/factory.h"

#define BATCH_CONTRACT_COLORS 24
#define BATCH_CONTRACT_QUERIES 4000

static unsigned int
batch_contract_next(unsigned int *state)
{
    *state = *state * 1103515245U + 12345U;
    return (*state >> 8) & 0xffffffU;
}

/*
 * The palette repeats some colors and keeps others on a coarse grid, so the
 * queries below include exact hits, duplicate entries and equidistant ties,
 * the cases where a hinted search could otherwise pick a different index.
 */
static void
batch_contract_palette(unsigned char *palette, float *palette_float)
{
    unsigned int state;
    int index;
    int channel;

    state = 7U;
    for (index = 0; index < BATCH_CONTRACT_COLORS; ++index) {
        for (channel = 0; channel < 3; ++channel) {
            if (index >= 4 && index % 5 == 0) {
                palette[index * 3 + channel] =
                    palette[(index - 4) * 3 + channel];
            } else {
                palette[index * 3 + channel] = (unsigned char)
                    ((batch_contract_next(&state) % 6U) * 51U);
            }
            palette_float[index * 3 + channel] =
                (float)palette[index * 3 + channel] / 255.0f;
        }
    }
}

static int
batch_contract_check(char const *class_name,
                    unsigned char const *palette,
                    float const *palette_float)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_factory_t *factory;
    sixel_lookup_policy_interface_t *policy;
    sixel_lookup_policy_interface_t *reference;
    sixel_lookup_policy_interface_t *seeds[2];
    sixel_lookup_policy_prepare_request_t request;
    unsigned char pixel[BATCH_CONTRACT_QUERIES * 3];
    float pixel_float[BATCH_CONTRACT_QUERIES * 3];
    sixel_index_t expected[BATCH_CONTRACT_QUERIES];
    sixel_index_t actual[BATCH_CONTRACT_QUERIES];
    unsigned char const *query;
    void *service;
    unsigned int state;
    int is_float;
    int is_fhedt;
    int seed;
    int ncolors;
    int count;
    int channel;
    int direction;
    int stride;
    int start;
    int length;
    int result;

    status = SIXEL_FALSE;
    allocator = NULL;
    factory = NULL;
    policy = NULL;
    reference = NULL;
    seeds[0] = seeds[1] = NULL;
    seed = 0;
    memset(&request, 0, sizeof(request));
    memset(pixel, 0, sizeof(pixel));
    memset(pixel_float, 0, sizeof(pixel_float));
    query = NULL;
    service = NULL;
    state = 11U;
    is_float = strstr(class_name, ".float32") != NULL;
    is_fhedt = strstr(class_name, "lookup/fhedt.") != NULL;
    /* The monochrome policies serve two-color palettes only. */
    ncolors = BATCH_CONTRACT_COLORS;
    if (strstr(class_name, "/mono-") != NULL) {
        ncolors = 2;
    }
    count = 0;
    channel = 0;
    direction = 1;
    stride = is_float ? 3 * (int)sizeof(float) : 3;
    result = EXIT_FAILURE;

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    status = sixel_components_getservice("services/factory", &service);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    factory = (sixel_factory_t *)service;
    status = factory->vtbl->create(factory,
                                   class_name,
                                   allocator,
                                   (void **)&policy);
    if (SIXEL_SUCCEEDED(status)) {
        status = factory->vtbl->create(factory, class_name, allocator,
                                      (void **)&reference);
    }
    for (seed = 0; is_fhedt && seed < 2 && SIXEL_SUCCEEDED(status); ++seed) {
        status = factory->vtbl->create(factory, class_name, allocator,
                                      (void **)&seeds[seed]);
    }
    factory->vtbl->unref(factory);
    factory = NULL;
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: create failed\n", class_name);
        goto cleanup;
    }

    request.palette = palette;
    request.depth = 3;
    request.reqcolor = ncolors;
    request.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    request.allocator = allocator;
    if (is_float != 0) {
        request.palette_float = palette_float;
        request.float_depth = 3 * (int)sizeof(float);
        request.pixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
    }
    /* Build two independent immutable FHEDT grids once, inside this test.
     * Reprepare below still resets each query handle. Separate seed owners
     * keep the scalar and bulk TLS cache identities distinct, including when
     * the optional cache is enabled. Never share mutable lookup state. */
    if (is_fhedt) {
        request.fhedt_shared_override = 1;
        request.fhedt_shared = 1;
        for (seed = 0; seed < 2; ++seed) {
            if (SIXEL_FAILED(seeds[seed]->vtbl->prepare(seeds[seed],
                                                       &request))) {
                goto cleanup;
            }
        }
    }
    /* Runs, nonadjacent repeats and distinct colors share one cold stream.
     * The last byte of every float remains part of the query's identity. */
    for (count = 0; count < BATCH_CONTRACT_QUERIES; ++count) {
        for (channel = 0; channel < 3; ++channel) {
            pixel[count * 3 + channel] = (unsigned char)
                (batch_contract_next(&state) & 255U);
            if (count < 40) {
                pixel[count * 3 + channel] = 25;
            } else if (count % 7 == 0) {
                pixel[count * 3 + channel] = pixel[channel];
            }
            pixel_float[count * 3 + channel] =
                (float)pixel[count * 3 + channel] / 255.0f;
        }
    }
    query = is_float ? (unsigned char const *)(void const *)pixel_float
                     : pixel;
    if (policy->vtbl->map_pixels == NULL) {
        goto cleanup;
    }
    policy->vtbl->map_pixels(policy, NULL, 0, stride, NULL, 1, NULL);
    for (direction = -1; direction <= 1; direction += 2) {
        /* Both cold and warm calls, SIMD tails, and reverse traversal. */
        for (length = 1; length <= BATCH_CONTRACT_QUERIES;
                length = length < 17 ? length + 1 : BATCH_CONTRACT_QUERIES) {
            request.reuse_policy = seeds[0];
            if (SIXEL_FAILED(policy->vtbl->prepare(policy, &request))) {
                goto cleanup;
            }
            request.reuse_policy = seeds[1];
            if (SIXEL_FAILED(reference->vtbl->prepare(reference, &request))) {
                goto cleanup;
            }
            start = direction == 1 ? 0 : length - 1;
            for (count = 0; count < length; ++count) {
                expected[start + count * direction] = (sixel_index_t)
                    reference->vtbl->map_pixel(reference,
                        query + (start + count * direction) * stride);
            }
            memset(actual, 0xff, sizeof(actual));
            policy->vtbl->map_pixels(policy, query + start * stride,
                (size_t)length, direction * stride,
                actual + start, direction, NULL);
            if (memcmp(expected, actual, (size_t)length) != 0) {
                fprintf(stderr, "%s: cold batch length %d direction %d\n",
                        class_name, length, direction);
                goto cleanup;
            }
            policy->vtbl->map_pixels(policy, query + start * stride,
                (size_t)length, direction * stride,
                actual + start, direction, NULL);
            if (memcmp(expected, actual, (size_t)length) != 0) {
                goto cleanup;
            }
            if (length == BATCH_CONTRACT_QUERIES) {
                break;
            }
        }
    }
    result = EXIT_SUCCESS;

cleanup:
    for (seed = 0; seed < 2; ++seed) {
        if (seeds[seed] != NULL) {
            seeds[seed]->vtbl->unref(seeds[seed]);
        }
    }
    if (reference != NULL) {
        reference->vtbl->unref(reference);
    }
    if (policy != NULL) {
        policy->vtbl->unref(policy);
    }
    if (factory != NULL) {
        factory->vtbl->unref(factory);
    }
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }

    return result;
}

int
test_lookup_0015_batch_contract(int argc, char **argv)
{
    static char const *const class_names[] = {
        "lookup/none.8bit",
        "lookup/none.float32",
        "lookup/mono-darkbg.8bit",
        "lookup/mono-darkbg.float32",
        "lookup/mono-lightbg.8bit",
        "lookup/mono-lightbg.float32",
        "lookup/certlut.8bit",
        "lookup/certlut.float32",
        "lookup/5bit.8bit",
        "lookup/5bit.float32",
        "lookup/6bit.8bit",
        "lookup/6bit.float32",
        "lookup/eytzinger.8bit",
        "lookup/eytzinger.float32",
        "lookup/fhedt.8bit",
        "lookup/fhedt.float32",
        "lookup/vptree.8bit",
        "lookup/vptree.float32",
        "lookup/delaunay.8bit",
        "lookup/delaunay.float32",
        "lookup/blockcull.8bit",
        "lookup/blockcull.float32",
        "lookup/ordered.8bit",
        "lookup/ordered.float32",
        "lookup/rbc.8bit",
        "lookup/rbc.float32",
        "lookup/mahalanobis.8bit",
        "lookup/mahalanobis.float32"
    };
    unsigned char palette[BATCH_CONTRACT_COLORS * 3];
    float palette_float[BATCH_CONTRACT_COLORS * 3];
    size_t index;

    (void)argc;
    (void)argv;

    batch_contract_palette(palette, palette_float);
    for (index = 0U;
            index < sizeof(class_names) / sizeof(class_names[0]);
            ++index) {
        if (batch_contract_check(class_names[index],
                                palette,
                                palette_float) != EXIT_SUCCESS) {
            return EXIT_FAILURE;
        }
    }

    return EXIT_SUCCESS;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
