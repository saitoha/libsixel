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
                     sixel_allocator_t *allocator,
                     void **object)
{
    SIXELSTATUS status;
    sixel_factory_t *factory;
    void *service;

    status = SIXEL_FALSE;
    factory = NULL;
    service = NULL;

    if (name == NULL || allocator == NULL || object == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    *object = NULL;

    status = sixel_components_getservice("services/factory", &service);
    if (SIXEL_FAILED(status)) {
        return status;
    }
    factory = (sixel_factory_t *)service;

    status = factory->vtbl->create(factory, allocator, name, object);
    factory->vtbl->unref(factory);
    return status;
}

static void
safe_unref_lookup_policy_dither(sixel_lookup_policy_interface_t **lookup_policy)
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
prepare_none_lookup_policy(sixel_dither_t *dither,
                           int pixelformat,
                           sixel_lookup_policy_interface_t **lookup_policy)
{
    SIXELSTATUS status;
    sixel_lookup_policy_prepare_request_t request;
    char const *class_name;

    status = SIXEL_FALSE;
    memset(&request, 0, sizeof(request));
    class_name = NULL;

    if (dither == NULL || dither->palette == NULL || lookup_policy == NULL) {
        return 0;
    }

    class_name = "lookup/none.8bit";
    if (SIXEL_PIXELFORMAT_IS_FLOAT32(pixelformat)) {
        class_name = "lookup/none.float32";
    }

    status = create_policy_object(class_name,
                                  dither->allocator,
                                  (void **)lookup_policy);
    if (SIXEL_FAILED(status)) {
        return 0;
    }

    request.palette = dither->palette->entries;
    request.palette_float = NULL;
    request.depth = 3;
    request.float_depth = 0;
    request.reqcolor = 2;
    request.pixelformat = pixelformat;
    request.parallel_dither_active = 0;
    request.shared_instance_enabled = 1;
    request.reuse_policy = NULL;
    request.reuse_policy_slot = NULL;
    request.allocator = dither->allocator;

    status = (*lookup_policy)->vtbl->prepare(*lookup_policy, &request);
    if (SIXEL_FAILED(status)) {
        safe_unref_lookup_policy_dither(lookup_policy);
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
    float pixel_float[3];
    sixel_index_t result[1];
    size_t i;
    int supports_parallel;
    int is_float_class;
    int pixelformat;
    static char const * const class_names[] = {
        "dither/none.8bit",
        "dither/none.float32",
        "dither/fs.8bit",
        "dither/fs.float32",
        "dither/atkinson.8bit",
        "dither/atkinson.float32",
        "dither/jajuni.8bit",
        "dither/jajuni.float32",
        "dither/stucki.8bit",
        "dither/stucki.float32",
        "dither/burkes.8bit",
        "dither/burkes.float32",
        "dither/sierra1.8bit",
        "dither/sierra1.float32",
        "dither/sierra2.8bit",
        "dither/sierra2.float32",
        "dither/sierra3.8bit",
        "dither/sierra3.float32",
        "dither/lso2.8bit",
        "dither/lso2.float32",
        "dither/a_dither.8bit",
        "dither/a_dither.float32",
        "dither/x_dither.8bit",
        "dither/x_dither.float32",
        "dither/bluenoise.8bit",
        "dither/bluenoise.float32",
        "dither/interframe.8bit",
        "dither/interframe.float32"
    };

    status = SIXEL_FALSE;
    dither = NULL;
    lookup_policy = NULL;
    dither_policy = NULL;
    supports_parallel = 0;
    is_float_class = 0;
    pixelformat = SIXEL_PIXELFORMAT_RGB888;
    memset(&prepare_request, 0, sizeof(prepare_request));
    memset(&apply_request, 0, sizeof(apply_request));
    memset(pixel_float, 0, sizeof(pixel_float));

    palette[0] = 0x00;
    palette[1] = 0x00;
    palette[2] = 0x00;
    palette[3] = 0xff;
    palette[4] = 0xff;
    palette[5] = 0xff;
    pixel[0] = 0x90;
    pixel[1] = 0x80;
    pixel[2] = 0x70;
    pixel_float[0] = 144.0f;
    pixel_float[1] = 128.0f;
    pixel_float[2] = 112.0f;
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

    for (i = 0; i < sizeof(class_names) / sizeof(class_names[0]); ++i) {
        is_float_class = strstr(class_names[i], ".float32") != NULL;
        pixelformat = SIXEL_PIXELFORMAT_RGB888;
        if (is_float_class) {
            pixelformat = SIXEL_PIXELFORMAT_RGBFLOAT32;
        }

        safe_unref_lookup_policy_dither(&lookup_policy);
        if (!prepare_none_lookup_policy(dither, pixelformat, &lookup_policy)) {
            sixel_dither_unref(dither);
            return 0;
        }

        status = create_policy_object(class_names[i],
                                      dither->allocator,
                                      (void **)&dither_policy);
        if (SIXEL_FAILED(status) || dither_policy == NULL
                || dither_policy->vtbl == NULL) {
            safe_unref_lookup_policy_dither(&lookup_policy);
            sixel_dither_unref(dither);
            return 0;
        }

        memset(&prepare_request, 0, sizeof(prepare_request));
        prepare_request.dither = dither;
        prepare_request.depth = 3;
        prepare_request.reqcolor = 2;
        prepare_request.method_for_scan = SIXEL_SCAN_RASTER;
        prepare_request.pixelformat = pixelformat;

        status = dither_policy->vtbl->prepare(dither_policy, &prepare_request);
        if (SIXEL_FAILED(status)) {
            safe_unref_dither_policy(&dither_policy);
            safe_unref_lookup_policy_dither(&lookup_policy);
            sixel_dither_unref(dither);
            return 0;
        }

        supports_parallel = dither_policy->vtbl->supports_parallel_bands(
            dither_policy);
        if (strstr(class_names[i], "dither/interframe.") != NULL) {
            if (supports_parallel != 0) {
                safe_unref_dither_policy(&dither_policy);
                safe_unref_lookup_policy_dither(&lookup_policy);
                sixel_dither_unref(dither);
                return 0;
            }
        } else {
            if (supports_parallel == 0) {
                safe_unref_dither_policy(&dither_policy);
                safe_unref_lookup_policy_dither(&lookup_policy);
                sixel_dither_unref(dither);
                return 0;
            }
        }

        memset(&apply_request, 0, sizeof(apply_request));
        apply_request.result = result;
        apply_request.data = pixel;
        apply_request.width = 1;
        apply_request.height = 1;
        apply_request.band_origin = 0;
        apply_request.output_start = 0;
        apply_request.depth = 3;
        apply_request.palette = dither->palette->entries;
        apply_request.method_for_scan = SIXEL_SCAN_RASTER;
        apply_request.lookup_policy = lookup_policy;
        apply_request.dither = dither;
        apply_request.pixelformat = pixelformat;
        if (is_float_class) {
            apply_request.data = (unsigned char *)(void *)pixel_float;
        }

        status = dither_policy->vtbl->apply(dither_policy, &apply_request);
        safe_unref_dither_policy(&dither_policy);
        if (SIXEL_FAILED(status)) {
            safe_unref_lookup_policy_dither(&lookup_policy);
            sixel_dither_unref(dither);
            return 0;
        }
    }

    safe_unref_lookup_policy_dither(&lookup_policy);
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
