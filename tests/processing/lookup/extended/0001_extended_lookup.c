/* SPDX-License-Identifier: MIT */
/* Native float nearest-color contracts, independent of RGB byte views. */
#include <float.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/factory.h"

static int const lookup_formats[] = {
    SIXEL_PIXELFORMAT_RGBFLOAT32, SIXEL_PIXELFORMAT_LINEARRGBFLOAT32,
    SIXEL_PIXELFORMAT_OKLABFLOAT32, SIXEL_PIXELFORMAT_CIELABFLOAT32,
    SIXEL_PIXELFORMAT_DIN99DFLOAT32
};
static char const *const lookup_classes[] = {
    "lookup/certlut.float32", "lookup/5bit.float32", "lookup/6bit.float32"
};

static int
lookup_prepare(sixel_lookup_policy_interface_t *policy,
               sixel_allocator_t *allocator, float const *palette,
               int space, int count)
{
    /* Deliberately contradictory byte coordinates expose loss of the
     * authoritative float view. No tested class borrows this byte view. */
    static unsigned char const bytes[12] = { 0 };
    sixel_lookup_policy_prepare_request_t request;

    memset(&request, 0, sizeof(request));
    request.palette = bytes;
    request.palette_float = palette;
    request.depth = 3;
    request.float_depth = 3 * sizeof(float);
    request.reqcolor = count;
    request.pixelformat = lookup_formats[space];
    request.allocator = allocator;
    return SIXEL_SUCCEEDED(policy->vtbl->prepare(policy, &request));
}

static int
lookup_map(sixel_lookup_policy_interface_t *policy, float const *query,
           int expected)
{
    int actual;

    actual = policy->vtbl->map_pixel(policy, (unsigned char const *)query);
    if (actual != expected) {
        fprintf(stderr, "query %.9g %.9g %.9g: got %d, expected %d\n",
            (double)query[0], (double)query[1], (double)query[2],
            actual, expected);
    }
    return actual == expected;
}

static int
lookup_values(sixel_lookup_policy_interface_t *policy,
              sixel_allocator_t *allocator, int space, char const *mode)
{
    /* The documented ranges are 1 on all RGB/OKLab axes, (1,3,3) in
     * CIELAB and (1,2,2) in DIN99d. All samples below are binary fractions;
     * each nearest-color decision has a strict, generous distance margin. */
    static float const ranges[5][3] = {
        { 1, 1, 1 }, { 1, 1, 1 }, { 1, 1, 1 }, { 1, 3, 3 }, { 1, 2, 2 }
    };
    float palette[12];
    float original[12];
    float query[3];
    float saved[3];
    float scale;
    int axis;
    int ok;

    for (axis = 0; axis < 3; ++axis) {
        memset(palette, 0, sizeof(palette));
        memset(query, 0, sizeof(query));
        scale = ranges[space][axis];
        if (strcmp(mode, "range") == 0) {
            palette[axis] = -.25f * scale;
            palette[3 + axis] = -1.5f * scale;
            palette[6 + axis] = 1.5f * scale;
            palette[9 + axis] = 2 * scale;
            query[axis] = -1.25f * scale;
        } else if (strcmp(mode, "fraction") == 0) {
            /* The first two centers differ by 1/1024 of the range and
             * project to the same byte. The query is 3/4 toward #1. */
            palette[axis] = (.5f + 1.f / 2048) * scale;
            palette[3 + axis] = (.5f + 3.f / 2048) * scale;
            palette[6 + axis] = .75f * scale;
            palette[9 + axis] = -.5f * scale;
            query[axis] = (.5f + 2.5f / 2048) * scale;
        } else if (strcmp(mode, "overflow") == 0) {
            /* Every nonzero squared difference exceeds FLT_MAX, while
             * double represents all distances and their strict ordering. */
            palette[axis] = -FLT_MAX;
            palette[3 + axis] = FLT_MAX / 2;
            palette[6 + axis] = FLT_MAX / 4;
            palette[9 + axis] = -FLT_MAX / 2;
            query[axis] = FLT_MAX;
        } else if (strcmp(mode, "underflow") == 0) {
            /* Inputs are normal float values; their squared differences
             * are below float range. This does not require subnormal I/O. */
            palette[axis] = FLT_MIN;
            palette[3 + axis] = 3 * FLT_MIN;
            palette[6 + axis] = 2 * FLT_MIN;
            palette[9 + axis] = -FLT_MIN;
            query[axis] = 4 * FLT_MIN;
        } else {
            /* Normalized distance chooses #1 (1/64 vs 9/256). Unweighted
             * Lab/DIN99d coordinates instead choose #0. */
            palette[0] = .1875f;
            palette[4] = .125f * ranges[space][1];
            palette[6] = 2;
            palette[9] = -2;
        }
        memcpy(original, palette, sizeof(palette));
        memcpy(saved, query, sizeof(query));
        ok = lookup_prepare(policy, allocator, palette, space, 4) &&
            lookup_map(policy, query, 1);
        if (strcmp(mode, "range") == 0 && ok) {
            query[axis] = 1.875f * scale;
            memcpy(saved, query, sizeof(query));
            ok = lookup_map(policy, query, 3);
        }
        if (!ok || memcmp(original, palette, sizeof(palette)) != 0 ||
                memcmp(saved, query, sizeof(query)) != 0) {
            fprintf(stderr, "%s space=%d axis=%d\n", mode, space, axis);
            return 0;
        }
    }
    return 1;
}

static int
lookup_isolation(sixel_lookup_policy_interface_t *first,
                 sixel_lookup_policy_interface_t *second,
                 sixel_allocator_t *allocator)
{
    float a[9] = { -1, 0, 0, .5f, 0, 0, 2, 0, 0 };
    float b[9] = { 2, 0, 0, -1, 0, 0, .5f, 0, 0 };
    float c[9] = { .5f, 0, 0, 2, 0, 0, -1, 0, 0 };
    float query[3] = { 1.875f, 0, 0 };

    /* Interleave two live objects and reprepare only the first. The same
     * physical query must follow each object's new palette order. */
    return lookup_prepare(first, allocator, a, 0, 3) &&
        lookup_map(first, query, 2) &&
        lookup_prepare(second, allocator, b, 0, 3) &&
        lookup_map(first, query, 2) && lookup_map(second, query, 0) &&
        lookup_prepare(first, allocator, c, 0, 3) &&
        lookup_map(first, query, 1) && lookup_map(second, query, 0);
}

int
test_extended_lookup(int argc, char **argv)
{
    sixel_allocator_t *allocator;
    sixel_factory_t *factory;
    sixel_lookup_policy_interface_t *first;
    sixel_lookup_policy_interface_t *second;
    void *service;
    int backend;
    int space;
    int ok;

    if (argc != 4) {
        return EXIT_FAILURE;
    }
    backend = atoi(argv[2]);
    space = atoi(argv[3]);
    if (backend < 0 || backend > 2 || space < 0 || space > 4) {
        return EXIT_FAILURE;
    }
    allocator = NULL;
    factory = NULL;
    first = NULL;
    second = NULL;
    service = NULL;
    ok = 0;
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL,
            NULL, NULL)) ||
            SIXEL_FAILED(sixel_components_getservice("services/factory",
                &service))) {
        goto end;
    }
    factory = service;
    if (SIXEL_FAILED(factory->vtbl->create(factory, lookup_classes[backend],
            allocator, (void **)&first)) ||
            SIXEL_FAILED(factory->vtbl->create(factory,
                lookup_classes[backend], allocator, (void **)&second))) {
        goto end;
    }
    ok = strcmp(argv[1], "isolation") == 0
        ? lookup_isolation(first, second, allocator)
        : lookup_values(first, allocator, space, argv[1]);
end:
    if (second != NULL) {
        second->vtbl->unref(second);
    }
    if (first != NULL) {
        first->vtbl->unref(first);
    }
    if (factory != NULL) {
        factory->vtbl->unref(factory);
    }
    sixel_allocator_unref(allocator);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
