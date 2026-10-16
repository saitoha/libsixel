/* SPDX-License-Identifier: MIT */
/* Reuse must not consume a source or retain an incompatible palette. */
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/factory.h"

int
fhedt_reuse_isolation(int floating)
{
    sixel_allocator_t *allocator;
    sixel_factory_t *factory;
    sixel_lookup_policy_interface_t *source;
    sixel_lookup_policy_interface_t *consumer;
    sixel_lookup_policy_prepare_request_t request;
    unsigned char palette[6] = {255, 255, 255, 0, 0, 0};
    float native[6] = {1, 1, 1, 0, 0, 0};
    float query[3] = {0, 0, 0};
    unsigned char const *pixel;
    char const *classid;
    void *service;
    int result;

    allocator = NULL;
    factory = NULL;
    source = consumer = NULL;
    service = NULL;
    result = EXIT_FAILURE;
    pixel = (unsigned char const *)query;
    classid = floating ? "lookup/fhedt.float32" : "lookup/fhedt.8bit";
    memset(&request, 0, sizeof(request));
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL,
            NULL)) || SIXEL_FAILED(sixel_components_getservice(
                "services/factory", &service))) {
        goto end;
    }
    factory = service;
    if (SIXEL_FAILED(factory->vtbl->create(factory, classid, allocator,
            (void **)&source)) || SIXEL_FAILED(factory->vtbl->create(
                factory, classid, allocator, (void **)&consumer))) {
        goto end;
    }
    request.palette = palette;
    request.palette_float = floating ? native : NULL;
    request.depth = 3;
    request.float_depth = floating ? 3 * (int)sizeof(float) : 0;
    request.reqcolor = 2;
    request.pixelformat = floating ? SIXEL_PIXELFORMAT_RGBFLOAT32
        : SIXEL_PIXELFORMAT_RGB888;
    request.allocator = allocator;
    request.fhedt_resolution_override = 1;
    request.fhedt_resolution = 64;
    request.fhedt_refine_override = 1;
    request.fhedt_refine = 1;
    request.fhedt_shared_override = 1;
    request.fhedt_shared = 1;
    request.fhedt_use_cache_override = 1;
    request.fhedt_use_cache = 1;
    if (SIXEL_FAILED(source->vtbl->prepare(source, &request))) {
        goto end;
    }
    request.reuse_policy = source;
    if (SIXEL_FAILED(consumer->vtbl->prepare(consumer, &request)) ||
            source->vtbl->map_pixel(source, pixel) != 1 ||
            consumer->vtbl->map_pixel(consumer, pixel) != 1) {
        goto end;
    }
    source->vtbl->unref(source);
    source = NULL;
    if (consumer->vtbl->map_pixel(consumer, pixel) != 1 ||
            SIXEL_FAILED(factory->vtbl->create(factory, classid, allocator,
                (void **)&source))) {
        goto end;
    }
    /* Exchange labels while keeping both owners alive and caches warm. */
    memset(palette, 0, 3);
    memset(palette + 3, 255, 3);
    native[0] = native[1] = native[2] = 0;
    native[3] = native[4] = native[5] = 1;
    request.reuse_policy = consumer;
    if (SIXEL_FAILED(source->vtbl->prepare(source, &request)) ||
            consumer->vtbl->map_pixel(consumer, pixel) != 1 ||
            source->vtbl->map_pixel(source, pixel) != 0) {
        goto end;
    }
    consumer->vtbl->unref(consumer);
    consumer = NULL;
    if (source->vtbl->map_pixel(source, pixel) != 0) {
        goto end;
    }
    result = EXIT_SUCCESS;
end:
    if (consumer != NULL) {
        consumer->vtbl->unref(consumer);
    }
    if (source != NULL) {
        source->vtbl->unref(source);
    }
    if (factory != NULL) {
        factory->vtbl->unref(factory);
    }
    sixel_allocator_unref(allocator);
    return result;
}
