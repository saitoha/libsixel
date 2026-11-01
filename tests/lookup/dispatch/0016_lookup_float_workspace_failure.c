/*
 * SPDX-License-Identifier: MIT
 * Policy: docs/encoder/palette-application/lookup-policy/README.md
 * A refused band workspace preserves the scalar palette indices.
 */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include <6cells.h>
#include "src/dither.h"
#include "dither-policy-private.h"
#include "src/factory.h"

static size_t float_workspace_fail_size;
static int float_workspace_fail_count;

static void *
float_workspace_malloc(size_t size)
{
    if (float_workspace_fail_size && size == float_workspace_fail_size) {
        ++float_workspace_fail_count;
        return NULL;
    }
    return malloc(size);
}

int
test_lookup_0020_float_workspace_failure(int argc, char **argv)
{
    sixel_allocator_t *allocator;
    sixel_dither_t *dither;
    sixel_factory_t *factory;
    sixel_lookup_policy_interface_t *lookup;
    sixel_dither_policy_interface_t *policy;
    sixel_lookup_policy_prepare_request_t lp;
    sixel_dither_policy_prepare_request_t dp;
    sixel_dither_policy_apply_request_t request;
    sixel_dither_execution_t request_execution;
    unsigned char palette[12] = { 0, 0, 0, 85, 85, 85,
                                 170, 170, 170, 255, 255, 255 };
    float native[12] = { -0.01f, 0, 0, 0.333f, 0.334f, 0.335f,
                        0.666f, 0.667f, 0.668f, 1.01f, 1, 1 };
    float pixels[51 * 3];
    sixel_index_t actual[51], expected[51];
    int i, rc;

    (void)argc;
    (void)argv;
    allocator = NULL;
    dither = NULL;
    factory = NULL;
    lookup = NULL;
    policy = NULL;
    rc = EXIT_FAILURE;
    float_workspace_fail_size = 0;
    float_workspace_fail_count = 0;
    memset(&lp, 0, sizeof(lp));
    memset(&dp, 0, sizeof(dp));
    memset(&request, 0, sizeof(request));
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, float_workspace_malloc,
                                        calloc, realloc, free))
            || SIXEL_FAILED(sixel_dither_new(&dither, 4, allocator))
            || SIXEL_FAILED(sixel_components_getservice("services/factory",
                                                      (void **)&factory))) {
        goto cleanup;
    }
    sixel_dither_set_palette(dither, palette);
    dither->method_for_diffuse = SIXEL_DIFFUSE_NONE;
    lp.palette = palette;
    lp.palette_float = native;
    lp.float_depth = 3 * (int)sizeof(float);
    lp.depth = 3;
    lp.reqcolor = 4;
    lp.allocator = allocator;
    lp.pixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
    sixel_dither_policy_capture_options(dither, &dp.options);
    dp.pixelformat = lp.pixelformat;
    if (SIXEL_FAILED(factory->vtbl->create(factory, "lookup/vptree.float32",
                                         allocator, (void **)&lookup))
            || SIXEL_FAILED(factory->vtbl->create(factory,
                "dither/none.float32", allocator, (void **)&policy))
            || SIXEL_FAILED(lookup->vtbl->prepare(lookup, &lp))
            || SIXEL_FAILED(policy->vtbl->prepare(policy, &dp))) {
        goto cleanup;
    }
    for (i = 0; i < 51 * 3; ++i) {
        pixels[i] = (float)(i * 37 % 257) / 256;
    }
    for (i = 0; i < 51; ++i) {
        expected[i] = (sixel_index_t)lookup->vtbl->map_pixel(lookup,
            (unsigned char const *)(pixels + i * 3));
    }
    sixel_dither_policy_capture_execution(dither, &request_execution);
    request.execution = &request_execution;
    request.lookup_policy = lookup;
    request.data = (unsigned char *)pixels;
    request.result = actual;
    request.width = 17;
    request.height = 3;
    request.depth = 3;
    request.palette = palette;
    float_workspace_fail_size = lookup->vtbl->workspace_size(lookup);
    if (float_workspace_fail_size == 0
            || SIXEL_FAILED(policy->vtbl->apply(policy, &request))
            || float_workspace_fail_count != 1
            || memcmp(actual, expected, sizeof(actual)) != 0) {
        goto cleanup;
    }
    rc = EXIT_SUCCESS;
cleanup:
    float_workspace_fail_size = 0;
    if (policy != NULL) {
        policy->vtbl->unref(policy);
    }
    if (lookup != NULL) {
        lookup->vtbl->unref(lookup);
    }
    if (factory != NULL) {
        factory->vtbl->unref(factory);
    }
    sixel_dither_unref(dither);
    sixel_allocator_unref(allocator);
    return rc;
}
