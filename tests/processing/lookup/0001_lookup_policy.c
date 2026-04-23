/*
 * SPDX-License-Identifier: MIT
 *
 * Unit tests for lookup-policy dispatch. These checks validate that normal,
 * fast LUT, and monochrome lookup modes share the same create/prepare/map
 * contract via the component factory service.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sixel.h>

#include "src/components.h"
#include "src/factory.h"
#include "src/lookup-policy.h"
#include "src/filter.h"
#include "tests/processing/filter/filter_test_common.h"

/*
 * IDL usage in this unit
 *
 * IComponents.getservice("services/factory", &factory)
 * IFactory.create("lookup/...", &policy)
 * ILookupPolicy.prepare(request)
 */

static void
init_lookup_policy_request(sixel_lookup_policy_prepare_request_t *request,
                           unsigned char const *palette,
                           int reqcolor,
                           int optimize_lookup,
                           int lut_policy,
                           int pixelformat,
                           sixel_lut_t *reuse_lut,
                           sixel_lut_t **reuse_lut_slot,
                           sixel_allocator_t *allocator)
{
    memset(request, 0, sizeof(*request));
    request->palette = palette;
    request->palette_float = NULL;
    request->depth = 3;
    request->float_depth = 0;
    request->reqcolor = reqcolor;
    request->complexion = 1;
    request->optimize_lookup = optimize_lookup;
    request->lut_policy = lut_policy;
    request->method_for_largest = SIXEL_LARGE_AUTO;
    request->pixelformat = pixelformat;
    request->parallel_active = 0;
    request->reuse_lut_is_shared = 0;
    request->reuse_lut = reuse_lut;
    request->reuse_lut_slot = reuse_lut_slot;
    request->allocator = allocator;
}

static SIXELSTATUS
create_lookup_policy(char const *name,
                     sixel_lookup_policy_prepare_request_t const *request,
                     sixel_lookup_policy_interface_t **lookup_policy)
{
    SIXELSTATUS status;
    sixel_factory_t *factory;
    sixel_lookup_policy_interface_t *created;
    void *service;

    status = SIXEL_FALSE;
    factory = NULL;
    created = NULL;
    service = NULL;

    if (name == NULL || request == NULL || lookup_policy == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    *lookup_policy = NULL;

    status = sixel_components_getservice("services/factory", &service);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    factory = (sixel_factory_t *)service;

    status = factory->vtbl->create(factory, name, (void **)&created);
    factory->vtbl->unref(factory);
    factory = NULL;
    if (SIXEL_FAILED(status)) {
        return status;
    }

    status = created->vtbl->prepare(created, request);
    if (SIXEL_FAILED(status)) {
        created->vtbl->unref(created);
        return status;
    }

    *lookup_policy = created;
    return SIXEL_OK;
}

static SIXELSTATUS
create_lookup_policy_by_selected_name(
    sixel_lookup_policy_prepare_request_t const *request,
    sixel_lookup_policy_interface_t **lookup_policy,
    char const **selected_name)
{
    char const *name;

    name = NULL;
    if (selected_name != NULL) {
        *selected_name = NULL;
    }

    if (request == NULL || lookup_policy == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    name = sixel_lookup_policy_select_name(request);
    if (name == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }

    if (selected_name != NULL) {
        *selected_name = name;
    }

    return create_lookup_policy(name, request, lookup_policy);
}

static int
test_lookup_policy_normal_mode_maps_expected_color(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_lookup_policy_interface_t *lookup_policy;
    sixel_lookup_policy_prepare_request_t request;
    unsigned char palette[6];
    unsigned char pixel[3];
    int mapped;
    char const *selected_name;

    status = SIXEL_FALSE;
    allocator = NULL;
    lookup_policy = NULL;
    memset(&request, 0, sizeof(request));
    memset(palette, 0, sizeof(palette));
    memset(pixel, 0, sizeof(pixel));
    mapped = -1;
    selected_name = NULL;

    status = make_allocator(&allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    palette[0] = 0;
    palette[1] = 0;
    palette[2] = 0;
    palette[3] = 255;
    palette[4] = 0;
    palette[5] = 0;
    pixel[0] = 250;
    pixel[1] = 10;
    pixel[2] = 10;

    init_lookup_policy_request(&request,
                               palette,
                               2,
                               0,
                               SIXEL_LUT_POLICY_NONE,
                               SIXEL_PIXELFORMAT_RGB888,
                               NULL,
                               NULL,
                               allocator);

    status = create_lookup_policy_by_selected_name(&request,
                                                   &lookup_policy,
                                                   &selected_name);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    if (selected_name == NULL || strcmp(selected_name, "lookup/normal") != 0) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }
    mapped = lookup_policy->vtbl->map_pixel(lookup_policy, pixel);
    if (mapped != 1) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    status = SIXEL_OK;

cleanup:
    if (lookup_policy != NULL) {
        lookup_policy->vtbl->unref(lookup_policy);
    }
    sixel_allocator_unref(allocator);

    return SIXEL_SUCCEEDED(status);
}

static int
test_lookup_policy_fast_mode_maps_float_input(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_lookup_policy_interface_t *lookup_policy;
    sixel_lookup_policy_prepare_request_t request;
    unsigned char palette[6];
    float pixel_float[3];
    int mapped;
    sixel_lut_t *cached_lut;
    char const *selected_name;

    status = SIXEL_FALSE;
    allocator = NULL;
    lookup_policy = NULL;
    memset(&request, 0, sizeof(request));
    memset(palette, 0, sizeof(palette));
    memset(pixel_float, 0, sizeof(pixel_float));
    mapped = -1;
    cached_lut = NULL;
    selected_name = NULL;

    status = make_allocator(&allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    palette[0] = 0;
    palette[1] = 0;
    palette[2] = 0;
    palette[3] = 255;
    palette[4] = 0;
    palette[5] = 0;
    pixel_float[0] = 1.0f;
    pixel_float[1] = 0.0f;
    pixel_float[2] = 0.0f;

    init_lookup_policy_request(&request,
                               palette,
                               2,
                               1,
                               SIXEL_LUT_POLICY_6BIT,
                               SIXEL_PIXELFORMAT_RGBFLOAT32,
                               NULL,
                               &cached_lut,
                               allocator);

    status = create_lookup_policy_by_selected_name(&request,
                                                   &lookup_policy,
                                                   &selected_name);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    if (selected_name == NULL || strcmp(selected_name, "lookup/6bit") != 0) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }
    if (lookup_policy->vtbl->lookup_source_is_float(lookup_policy) == 0) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }
    if (cached_lut == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    mapped = lookup_policy->vtbl->map_pixel(
        lookup_policy,
        (unsigned char const *)(void const *)pixel_float);
    if (mapped < 0 || mapped >= 2) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    status = SIXEL_OK;

cleanup:
    if (lookup_policy != NULL) {
        lookup_policy->vtbl->unref(lookup_policy);
    }
    if (cached_lut != NULL) {
        sixel_lut_unref(cached_lut);
    }
    sixel_allocator_unref(allocator);

    return SIXEL_SUCCEEDED(status);
}

static int
test_lookup_policy_mono_mode_maps_expected_threshold(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_lookup_policy_interface_t *lookup_policy;
    sixel_lookup_policy_prepare_request_t request;
    unsigned char palette[6];
    unsigned char dark_pixel[3];
    unsigned char bright_pixel[3];
    int dark_index;
    int bright_index;
    char const *selected_name;

    status = SIXEL_FALSE;
    allocator = NULL;
    lookup_policy = NULL;
    memset(&request, 0, sizeof(request));
    memset(palette, 0, sizeof(palette));
    memset(dark_pixel, 0, sizeof(dark_pixel));
    memset(bright_pixel, 0, sizeof(bright_pixel));
    dark_index = -1;
    bright_index = -1;
    selected_name = NULL;

    status = make_allocator(&allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    palette[0] = 0;
    palette[1] = 0;
    palette[2] = 0;
    palette[3] = 255;
    palette[4] = 255;
    palette[5] = 255;

    dark_pixel[0] = 0;
    dark_pixel[1] = 0;
    dark_pixel[2] = 0;
    bright_pixel[0] = 255;
    bright_pixel[1] = 255;
    bright_pixel[2] = 255;

    init_lookup_policy_request(&request,
                               palette,
                               2,
                               1,
                               SIXEL_LUT_POLICY_6BIT,
                               SIXEL_PIXELFORMAT_RGB888,
                               NULL,
                               NULL,
                               allocator);

    status = create_lookup_policy_by_selected_name(&request,
                                                   &lookup_policy,
                                                   &selected_name);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    if (selected_name == NULL
            || strcmp(selected_name, "lookup/mono-darkbg") != 0) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }
    dark_index = lookup_policy->vtbl->map_pixel(lookup_policy, dark_pixel);
    bright_index = lookup_policy->vtbl->map_pixel(lookup_policy, bright_pixel);
    if (dark_index != 0 || bright_index != 1) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    status = SIXEL_OK;

cleanup:
    if (lookup_policy != NULL) {
        lookup_policy->vtbl->unref(lookup_policy);
    }
    sixel_allocator_unref(allocator);

    return SIXEL_SUCCEEDED(status);
}

static int
test_lookup_policy_named_factory_creates_fast_lut(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_lookup_policy_interface_t *lookup_policy;
    sixel_lookup_policy_prepare_request_t request;
    unsigned char palette[6];
    unsigned char pixel[3];
    int mapped;

    status = SIXEL_FALSE;
    allocator = NULL;
    lookup_policy = NULL;
    memset(&request, 0, sizeof(request));
    memset(palette, 0, sizeof(palette));
    memset(pixel, 0, sizeof(pixel));
    mapped = -1;

    status = make_allocator(&allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    palette[0] = 0;
    palette[1] = 0;
    palette[2] = 255;
    palette[3] = 255;
    palette[4] = 255;
    palette[5] = 0;
    pixel[0] = 250;
    pixel[1] = 250;
    pixel[2] = 20;

    init_lookup_policy_request(&request,
                               palette,
                               2,
                               0,
                               SIXEL_LUT_POLICY_NONE,
                               SIXEL_PIXELFORMAT_RGB888,
                               NULL,
                               NULL,
                               allocator);

    status = create_lookup_policy("lookup/eytzinger", &request, &lookup_policy);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }
    mapped = lookup_policy->vtbl->map_pixel(lookup_policy, pixel);
    if (mapped < 0 || mapped >= 2) {
        status = SIXEL_BAD_ARGUMENT;
        goto cleanup;
    }

    status = SIXEL_OK;

cleanup:
    if (lookup_policy != NULL) {
        lookup_policy->vtbl->unref(lookup_policy);
    }
    sixel_allocator_unref(allocator);

    return SIXEL_SUCCEEDED(status);
}

static int
test_lookup_policy_all_named_classes_are_polymorphic(void)
{
    static char const *const class_names[] = {
        "lookup/normal",
        "lookup/mono-darkbg",
        "lookup/mono-lightbg",
        "lookup/certlut",
        "lookup/5bit",
        "lookup/6bit",
        "lookup/eytzinger",
        "lookup/fhedt",
        "lookup/vptree",
        "lookup/rbc",
        "lookup/mahalanobis"
    };
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_lookup_policy_prepare_request_t request;
    sixel_lookup_policy_interface_t *lookup_policy;
    sixel_lookup_policy_vtbl_t const *vtbls[11];
    unsigned char palette[6];
    unsigned char pixel[3];
    size_t class_count;
    size_t i;
    size_t j;
    int mapped;

    status = SIXEL_FALSE;
    allocator = NULL;
    memset(&request, 0, sizeof(request));
    lookup_policy = NULL;
    memset(vtbls, 0, sizeof(vtbls));
    memset(palette, 0, sizeof(palette));
    memset(pixel, 0, sizeof(pixel));
    class_count = sizeof(class_names) / sizeof(class_names[0]);
    i = 0U;
    j = 0U;
    mapped = -1;

    status = make_allocator(&allocator);
    if (SIXEL_FAILED(status)) {
        goto cleanup;
    }

    palette[0] = 0;
    palette[1] = 0;
    palette[2] = 0;
    palette[3] = 255;
    palette[4] = 0;
    palette[5] = 0;
    pixel[0] = 250;
    pixel[1] = 10;
    pixel[2] = 10;

    init_lookup_policy_request(&request,
                               palette,
                               2,
                               1,
                               SIXEL_LUT_POLICY_6BIT,
                               SIXEL_PIXELFORMAT_RGB888,
                               NULL,
                               NULL,
                               allocator);

    for (i = 0U; i < class_count; ++i) {
        status = create_lookup_policy(class_names[i], &request, &lookup_policy);
        if (SIXEL_FAILED(status)) {
            goto cleanup;
        }
        if (lookup_policy == NULL || lookup_policy->vtbl == NULL) {
            status = SIXEL_BAD_ARGUMENT;
            goto cleanup;
        }

        vtbls[i] = lookup_policy->vtbl;
        mapped = lookup_policy->vtbl->map_pixel(lookup_policy, pixel);
        if (mapped < 0 || mapped >= 2) {
            status = SIXEL_BAD_ARGUMENT;
            goto cleanup;
        }

        lookup_policy->vtbl->unref(lookup_policy);
        lookup_policy = NULL;
    }

    for (i = 0U; i < class_count; ++i) {
        for (j = i + 1U; j < class_count; ++j) {
            if (vtbls[i] == vtbls[j]) {
                status = SIXEL_BAD_ARGUMENT;
                goto cleanup;
            }
        }
    }

    status = SIXEL_OK;

cleanup:
    if (lookup_policy != NULL) {
        lookup_policy->vtbl->unref(lookup_policy);
    }
    sixel_allocator_unref(allocator);

    return SIXEL_SUCCEEDED(status);
}

int
test_lookup_0001_lookup_policy(int argc, char **argv)
{
    int success;

    (void)argc;
    (void)argv;

    success = 1;

    if (!test_lookup_policy_normal_mode_maps_expected_color()) {
        fprintf(stderr, "lookup policy normal mode failed\n");
        success = 0;
    }
    if (!test_lookup_policy_fast_mode_maps_float_input()) {
        fprintf(stderr, "lookup policy fast mode failed\n");
        success = 0;
    }
    if (!test_lookup_policy_mono_mode_maps_expected_threshold()) {
        fprintf(stderr, "lookup policy mono mode failed\n");
        success = 0;
    }
    if (!test_lookup_policy_named_factory_creates_fast_lut()) {
        fprintf(stderr, "lookup policy named factory failed\n");
        success = 0;
    }
    if (!test_lookup_policy_all_named_classes_are_polymorphic()) {
        fprintf(stderr, "lookup policy polymorphism failed\n");
        success = 0;
    }

    return success ? EXIT_SUCCESS : EXIT_FAILURE;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
