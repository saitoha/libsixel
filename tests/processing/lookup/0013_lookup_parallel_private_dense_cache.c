/*
 * SPDX-License-Identifier: MIT
 *
 * Verify private dense lookup policies cache during parallel dithering.
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

static int
private_dense_cache_populates(char const *class_name)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_factory_t *factory;
    sixel_lookup_policy_interface_t *policy;
    sixel_lookup_policy_prepare_request_t request;
    unsigned char palette[6];
    unsigned char first_pixel[3];
    unsigned char second_pixel[3];
    void *service;
    int first_result;
    int second_result;
    int result;

    status = SIXEL_FALSE;
    allocator = NULL;
    factory = NULL;
    policy = NULL;
    memset(&request, 0, sizeof(request));
    memset(palette, 0, sizeof(palette));
    memset(first_pixel, 0, sizeof(first_pixel));
    memset(second_pixel, 0, sizeof(second_pixel));
    service = NULL;
    first_result = -1;
    second_result = -1;
    result = EXIT_FAILURE;

    /*
     * Values 100 and 101 occupy the same rounded RGB555 and RGB666 bucket,
     * while each is nearest to a different palette entry.  Reusing index 0
     * for the second query therefore proves that the first query populated
     * the private dense cache instead of performing two exhaustive scans.
     */
    palette[0] = 100;
    palette[3] = 101;
    first_pixel[0] = 100;
    second_pixel[0] = 101;

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
        goto cleanup;
    }

    request.palette = palette;
    request.depth = 3;
    request.reqcolor = 2;
    request.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    request.parallel_dither_active = 1;
    request.shared_instance_enabled = 0;
    request.allocator = allocator;
    status = policy->vtbl->prepare(policy, &request);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    first_result = policy->vtbl->map_pixel(policy, first_pixel);
    second_result = policy->vtbl->map_pixel(policy, second_pixel);
    if (first_result != 0 || second_result != 0) {
        fprintf(stderr,
                "%s did not populate its parallel private dense cache "
                "(%d, %d)\n",
                class_name,
                first_result,
                second_result);
        goto cleanup;
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
test_lookup_0013_parallel_private_dense_cache(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (private_dense_cache_populates("lookup/5bit.8bit") != EXIT_SUCCESS) {
        return EXIT_FAILURE;
    }
    if (private_dense_cache_populates("lookup/6bit.8bit") != EXIT_SUCCESS) {
        return EXIT_FAILURE;
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
