/*
 * SPDX-License-Identifier: MIT
 *
 * Verify that every lookup policy returns from map_pixel_hint() exactly what
 * map_pixel() returns, whatever the hint.
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

#define HINT_CONTRACT_COLORS 24
#define HINT_CONTRACT_QUERIES 4000

static unsigned int
hint_contract_next(unsigned int *state)
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
hint_contract_palette(unsigned char *palette, float *palette_float)
{
    unsigned int state;
    int index;
    int channel;

    state = 7U;
    for (index = 0; index < HINT_CONTRACT_COLORS; ++index) {
        for (channel = 0; channel < 3; ++channel) {
            if (index >= 4 && index % 5 == 0) {
                palette[index * 3 + channel] =
                    palette[(index - 4) * 3 + channel];
            } else {
                palette[index * 3 + channel] = (unsigned char)
                    ((hint_contract_next(&state) % 6U) * 51U);
            }
            palette_float[index * 3 + channel] =
                (float)palette[index * 3 + channel] / 255.0f;
        }
    }
}

static int
hint_contract_check(char const *class_name,
                    unsigned char const *palette,
                    float const *palette_float)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_factory_t *factory;
    sixel_lookup_policy_interface_t *policy;
    sixel_lookup_policy_prepare_request_t request;
    unsigned char pixel[3];
    float pixel_float[3];
    unsigned char const *query;
    void *service;
    unsigned int state;
    int is_float;
    int ncolors;
    int count;
    int channel;
    int hint;
    int plain;
    int hinted;
    int result;

    status = SIXEL_FALSE;
    allocator = NULL;
    factory = NULL;
    policy = NULL;
    memset(&request, 0, sizeof(request));
    memset(pixel, 0, sizeof(pixel));
    memset(pixel_float, 0, sizeof(pixel_float));
    query = NULL;
    service = NULL;
    state = 11U;
    is_float = strstr(class_name, ".float32") != NULL;
    /* The monochrome policies serve two-color palettes only. */
    ncolors = HINT_CONTRACT_COLORS;
    if (strstr(class_name, "/mono-") != NULL) {
        ncolors = 2;
    }
    count = 0;
    channel = 0;
    hint = -1;
    plain = -1;
    hinted = -1;
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
    status = policy->vtbl->prepare(policy, &request);
    if (SIXEL_FAILED(status)) {
        fprintf(stderr, "%s: prepare failed\n", class_name);
        goto cleanup;
    }

    for (count = 0; count < HINT_CONTRACT_QUERIES; ++count) {
        for (channel = 0; channel < 3; ++channel) {
            switch (count % 3) {
            case 0:
                /* exact palette hits, duplicates included */
                pixel[channel] = palette[
                    (hint_contract_next(&state) % HINT_CONTRACT_COLORS) * 3
                    + (unsigned int)channel];
                break;
            case 1:
                /* midpoints of the coarse grid: equidistant ties */
                pixel[channel] = (unsigned char)
                    ((hint_contract_next(&state) % 11U) * 51U / 2U);
                break;
            default:
                pixel[channel] =
                    (unsigned char)(hint_contract_next(&state) & 0xffU);
                break;
            }
            pixel_float[channel] = (float)pixel[channel] / 255.0f;
        }
        query = pixel;
        if (is_float != 0) {
            query = (unsigned char const *)(void const *)pixel_float;
        }
        switch (count % 4) {
        case 0:
            hint = -1;
            break;
        case 1:
            hint = ncolors;
            break;
        case 2:
            hint = plain;
            break;
        default:
            hint = (int)(hint_contract_next(&state) % (unsigned int)ncolors);
            break;
        }
        plain = policy->vtbl->map_pixel(policy, query);
        hinted = policy->vtbl->map_pixel_hint(policy, query, hint);
        if (plain != hinted) {
            fprintf(stderr,
                    "%s: query %d (%u,%u,%u) hint %d mapped %d, not %d\n",
                    class_name,
                    count,
                    (unsigned int)pixel[0],
                    (unsigned int)pixel[1],
                    (unsigned int)pixel[2],
                    hint,
                    hinted,
                    plain);
            goto cleanup;
        }
    }
    result = EXIT_SUCCESS;

cleanup:
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
test_lookup_0014_hint_contract(int argc, char **argv)
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
        "lookup/rbc.8bit",
        "lookup/rbc.float32",
        "lookup/mahalanobis.8bit",
        "lookup/mahalanobis.float32"
    };
    unsigned char palette[HINT_CONTRACT_COLORS * 3];
    float palette_float[HINT_CONTRACT_COLORS * 3];
    size_t index;

    (void)argc;
    (void)argv;

    hint_contract_palette(palette, palette_float);
    for (index = 0U;
            index < sizeof(class_names) / sizeof(class_names[0]);
            ++index) {
        if (hint_contract_check(class_names[index],
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
