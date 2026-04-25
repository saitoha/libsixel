/*
 * SPDX-License-Identifier: MIT
 *
 * Unit tests for dither-policy dispatch through the component factory.
 */

#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif

#include <stdio.h>
#include <string.h>

#include <sixel.h>

#include "src/components.h"
#include "src/factory.h"
#include "src/dither.h"
#include "src/dither-policy.h"
#include "src/lookup-policy.h"

/*
 * IDL usage in this unit
 *
 * IComponents.getservice("services/factory", &factory)
 * IFactory.create("dither/...", &policy)
 * IFactory.create("lookup/...", &policy)
 * IDitherPolicy.prepare(request)
 * IDitherPolicy.apply(request)
 */

static SIXELSTATUS
create_policy_object(char const *name,
                     void **object)
{
    SIXELSTATUS status;
    sixel_factory_t *factory;
    void *service;

    status = SIXEL_FALSE;
    factory = NULL;
    service = NULL;

    if (name == NULL || object == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *object = NULL;

    status = sixel_components_getservice("services/factory", &service);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    factory = (sixel_factory_t *)service;

    status = factory->vtbl->create(factory, name, object);
    factory->vtbl->unref(factory);
    return status;
}

static void
safe_unref_lookup_policy(sixel_lookup_policy_interface_t **lookup_policy)
{
    if (lookup_policy == NULL || *lookup_policy == NULL) {
        return;
    }
    if ((*lookup_policy)->vtbl != NULL) {
        (*lookup_policy)->vtbl->unref(*lookup_policy);
    }
    *lookup_policy = NULL;
}

static void
safe_unref_dither_policy(sixel_dither_policy_interface_t **dither_policy)
{
    if (dither_policy == NULL || *dither_policy == NULL) {
        return;
    }
    if ((*dither_policy)->vtbl != NULL) {
        (*dither_policy)->vtbl->unref(*dither_policy);
    }
    *dither_policy = NULL;
}

static int
prepare_normal_lookup_policy(sixel_dither_t *dither,
                             sixel_lookup_policy_interface_t **lookup_policy)
{
    SIXELSTATUS status;
    sixel_lookup_policy_prepare_request_t request;

    status = SIXEL_FALSE;
    memset(&request, 0, sizeof(request));

    if (dither == NULL || dither->palette == NULL || lookup_policy == NULL) {
        return 0;
    }

    status = create_policy_object("lookup/normal", (void **)lookup_policy);
    if (SIXEL_FAILED(status)) {
        return 0;
    }

    request.palette = dither->palette->entries;
    request.palette_float = NULL;
    request.depth = 3;
    request.float_depth = 0;
    request.reqcolor = 2;
    request.pixelformat = SIXEL_PIXELFORMAT_RGB888;
    request.parallel_dither_active = 0;
    request.shared_instance_enabled = 1;
    request.reuse_policy = NULL;
    request.reuse_policy_slot = NULL;
    request.allocator = dither->allocator;

    status = (*lookup_policy)->vtbl->prepare(*lookup_policy, &request);
    if (SIXEL_FAILED(status)) {
        safe_unref_lookup_policy(lookup_policy);
        return 0;
    }

    return 1;
}

static int
test_dither_policy_named_classes_contract(void)
{
    SIXELSTATUS status;
    sixel_dither_t *dither;
    sixel_lookup_policy_interface_t *lookup_policy;
    sixel_dither_policy_interface_t *dither_policy;
    sixel_dither_policy_prepare_request_t prepare_request;
    sixel_dither_policy_apply_request_t apply_request;
    unsigned char palette[6];
    unsigned char pixel[3];
    sixel_index_t result[1];
    int ncolors;
    size_t i;
    static char const * const class_names[] = {
        "dither/none",
        "dither/fs",
        "dither/atkinson",
        "dither/jajuni",
        "dither/stucki",
        "dither/burkes",
        "dither/sierra1",
        "dither/sierra2",
        "dither/sierra3",
        "dither/lso2",
        "dither/a_dither",
        "dither/x_dither",
        "dither/bluenoise",
        "dither/interframe"
    };

    status = SIXEL_FALSE;
    dither = NULL;
    lookup_policy = NULL;
    dither_policy = NULL;
    memset(&prepare_request, 0, sizeof(prepare_request));
    memset(&apply_request, 0, sizeof(apply_request));

    palette[0] = 0x00;
    palette[1] = 0x00;
    palette[2] = 0x00;
    palette[3] = 0xff;
    palette[4] = 0xff;
    palette[5] = 0xff;
    pixel[0] = 0x90;
    pixel[1] = 0x80;
    pixel[2] = 0x70;
    result[0] = 0;

    status = sixel_dither_new(&dither, 2, NULL);
    if (SIXEL_FAILED(status) || dither == NULL || dither->palette == NULL) {
        return 0;
    }

    memcpy(dither->palette->entries, palette, sizeof(palette));
    dither->palette->entry_count = 2U;
    dither->palette->depth = 3;
    dither->ncolors = 2;
    dither->reqcolors = 2;
    dither->method_for_scan = SIXEL_SCAN_RASTER;
    dither->pixelformat = SIXEL_PIXELFORMAT_RGB888;

    if (!prepare_normal_lookup_policy(dither, &lookup_policy)) {
        sixel_dither_unref(dither);
        return 0;
    }

    for (i = 0; i < sizeof(class_names) / sizeof(class_names[0]); ++i) {
        int supports_parallel;

        status = create_policy_object(class_names[i], (void **)&dither_policy);
        if (SIXEL_FAILED(status) || dither_policy == NULL
                || dither_policy->vtbl == NULL) {
            safe_unref_lookup_policy(&lookup_policy);
            sixel_dither_unref(dither);
            return 0;
        }

        memset(&prepare_request, 0, sizeof(prepare_request));
        prepare_request.dither = dither;
        prepare_request.depth = 3;
        prepare_request.reqcolor = 2;
        prepare_request.method_for_scan = SIXEL_SCAN_RASTER;
        prepare_request.pixelformat = SIXEL_PIXELFORMAT_RGB888;
        prepare_request.optimize_palette = 0;
        prepare_request.complexion = 1;

        status = dither_policy->vtbl->prepare(dither_policy, &prepare_request);
        if (SIXEL_FAILED(status)) {
            safe_unref_dither_policy(&dither_policy);
            safe_unref_lookup_policy(&lookup_policy);
            sixel_dither_unref(dither);
            return 0;
        }

        supports_parallel = dither_policy->vtbl->supports_parallel_bands(
            dither_policy);
        if (strcmp(class_names[i], "dither/interframe") == 0) {
            if (supports_parallel != 0) {
                safe_unref_dither_policy(&dither_policy);
                safe_unref_lookup_policy(&lookup_policy);
                sixel_dither_unref(dither);
                return 0;
            }
        } else {
            if (supports_parallel == 0) {
                safe_unref_dither_policy(&dither_policy);
                safe_unref_lookup_policy(&lookup_policy);
                sixel_dither_unref(dither);
                return 0;
            }
        }

        ncolors = 2;
        memset(&apply_request, 0, sizeof(apply_request));
        apply_request.result = result;
        apply_request.data = pixel;
        apply_request.width = 1;
        apply_request.height = 1;
        apply_request.band_origin = 0;
        apply_request.output_start = 0;
        apply_request.depth = 3;
        apply_request.palette = dither->palette->entries;
        apply_request.reqcolor = 2;
        apply_request.method_for_scan = SIXEL_SCAN_RASTER;
        apply_request.foptimize_palette = 0;
        apply_request.complexion = 1;
        apply_request.lookup_policy = lookup_policy;
        apply_request.ncolors = &ncolors;
        apply_request.dither = dither;
        apply_request.pixelformat = SIXEL_PIXELFORMAT_RGB888;

        status = dither_policy->vtbl->apply(dither_policy, &apply_request);
        safe_unref_dither_policy(&dither_policy);
        if (SIXEL_FAILED(status)) {
            safe_unref_lookup_policy(&lookup_policy);
            sixel_dither_unref(dither);
            return 0;
        }
    }

    safe_unref_lookup_policy(&lookup_policy);
    sixel_dither_unref(dither);
    return 1;
}

int
test_dither_0001_dither_policy(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!test_dither_policy_named_classes_contract()) {
        fprintf(stderr, "dither policy contract failed\n");
        return 1;
    }

    return 0;
}
