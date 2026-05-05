/*
 * SPDX-License-Identifier: MIT
 *
 * Unit test for encoder_core creation through the component factory.
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
test_encoder_core_create_one(sixel_factory_t *factory,
                             sixel_allocator_t *allocator,
                             char const *classid)
{
    SIXELSTATUS status;
    sixel_encoder_core_t *core;
    sixel_encoder_core_options_t options;
    sixel_encoder_core_encode_request_t request;
    void *object;

    status = SIXEL_FALSE;
    core = NULL;
    object = NULL;
    memset(&options, 0, sizeof(options));
    memset(&request, 0, sizeof(request));

    status = factory->vtbl->create(factory, classid, allocator, &object);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    core = (sixel_encoder_core_t *)object;
    object = NULL;
    if (core == NULL || core->vtbl == NULL ||
        core->vtbl->get_options == NULL ||
        core->vtbl->set_options == NULL ||
        core->vtbl->encode == NULL ||
        core->vtbl->unref == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    status = core->vtbl->get_options(core, &options);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    options.has_gri_arg_limit = 0;
    options.palette_type = SIXEL_PALETTETYPE_HLS;
    options.encode_policy = SIXEL_ENCODEPOLICY_SIZE;
    options.ormode = 1;
    options.pixelformat = SIXEL_PIXELFORMAT_RGBA8888;
    options.source_colorspace = SIXEL_COLORSPACE_LINEAR;
    options.colorspace = SIXEL_COLORSPACE_OKLAB;
    status = core->vtbl->set_options(core, &options);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    memset(&options, 0, sizeof(options));
    status = core->vtbl->get_options(core, &options);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    if (options.has_gri_arg_limit != 0 ||
        options.palette_type != SIXEL_PALETTETYPE_HLS ||
        options.encode_policy != SIXEL_ENCODEPOLICY_SIZE ||
        options.ormode != 1 ||
        options.pixelformat != SIXEL_PIXELFORMAT_RGBA8888 ||
        options.source_colorspace != SIXEL_COLORSPACE_LINEAR ||
        options.colorspace != SIXEL_COLORSPACE_OKLAB) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    status = core->vtbl->encode(core, &request);
    if (status != SIXEL_NOT_IMPLEMENTED) {
        goto end;
    }
    status = SIXEL_OK;

end:
    if (object != NULL) {
        ((sixel_encoder_core_t *)object)->vtbl->unref(
            (sixel_encoder_core_t *)object);
    }
    if (core != NULL && core->vtbl != NULL &&
        core->vtbl->unref != NULL) {
        core->vtbl->unref(core);
    }
    return SIXEL_SUCCEEDED(status);
}

static int
test_encoder_core_factory_component(void)
{
    SIXELSTATUS status;
    sixel_allocator_t *allocator;
    sixel_factory_t *factory;
    void *service;

    status = SIXEL_FALSE;
    allocator = NULL;
    factory = NULL;
    service = NULL;

    status = sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL);
    if (SIXEL_FAILED(status)) {
        goto end;
    }

    status = sixel_components_getservice("services/factory", &service);
    if (SIXEL_FAILED(status)) {
        goto end;
    }
    factory = (sixel_factory_t *)service;
    if (factory == NULL || factory->vtbl == NULL ||
        factory->vtbl->create == NULL ||
        factory->vtbl->unref == NULL) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    if (!test_encoder_core_create_one(factory,
                                      allocator,
                                      "codec/encoder-core") ||
        !test_encoder_core_create_one(factory,
                                      allocator,
                                      "codec/encoder-core.normal") ||
        !test_encoder_core_create_one(factory,
                                      allocator,
                                      "codec/encoder-core.highcolor") ||
        !test_encoder_core_create_one(factory,
                                      allocator,
                                      "codec/encoder-core.ormode")) {
        status = SIXEL_BAD_ARGUMENT;
        goto end;
    }

    status = SIXEL_OK;

end:
    if (factory != NULL && factory->vtbl != NULL &&
        factory->vtbl->unref != NULL) {
        factory->vtbl->unref(factory);
    }
    if (allocator != NULL) {
        sixel_allocator_unref(allocator);
    }
    return SIXEL_SUCCEEDED(status);
}

int
test_encoder_core_0001_encoder_core_factory(int argc, char **argv)
{
    (void)argc;
    (void)argv;

    if (!test_encoder_core_factory_component()) {
        fprintf(stderr, "encoder_core factory component contract failed\n");
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
