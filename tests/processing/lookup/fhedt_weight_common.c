/* SPDX-License-Identifier: MIT */
/* Independent metric witnesses exercise the policy, not backend weights. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/factory.h"

static int
fh_weight_prepare(sixel_lookup_policy_interface_t *policy,
                  sixel_allocator_t *allocator, float const *palette,
                  int space, char const *ratios, int refine, int count,
                  sixel_lookup_policy_interface_t *reuse)
{
    static unsigned char const bytes[9] = { 0 };
    sixel_lookup_policy_prepare_request_t request;

    memset(&request, 0, sizeof(request));
    request.palette = bytes;
    request.palette_float = palette;
    request.depth = 3;
    request.float_depth = 3 * sizeof(float);
    request.reqcolor = count;
    request.pixelformat = space == 0 ? SIXEL_PIXELFORMAT_CIELABFLOAT32
        : space == 1 ? SIXEL_PIXELFORMAT_DIN99DFLOAT32
        : space == 4 ? SIXEL_PIXELFORMAT_LINEARRGBFLOAT32
        : space == 3 ? SIXEL_PIXELFORMAT_OKLABFLOAT32
        : SIXEL_PIXELFORMAT_RGBFLOAT32;
    request.allocator = allocator;
    request.fhedt_weights_override = strcmp(ratios, "default") != 0;
    if (strcmp(ratios, "auto") != 0 && strcmp(ratios, "default") != 0 &&
            sscanf(ratios, "%lf/%lf/%lf", &request.fhedt_weights[0],
                &request.fhedt_weights[1], &request.fhedt_weights[2]) != 3) {
        return 0;
    }
    request.fhedt_resolution = 64;
    request.fhedt_resolution_override = 1;
    request.fhedt_use_dist2 = 0;
    request.fhedt_use_dist2_override = 1;
    request.fhedt_refine = refine;
    request.fhedt_refine_override = 1;
    request.fhedt_use_cache = 1;
    request.fhedt_use_cache_override = 1;
    request.fhedt_shared = 1;
    request.fhedt_shared_override = 1;
    request.reuse_policy = reuse;
    return SIXEL_SUCCEEDED(policy->vtbl->prepare(policy, &request));
}

static int
fh_weight_map(sixel_lookup_policy_interface_t *policy, float const *query,
              int expected)
{
    int actual;

    actual = policy->vtbl->map_pixel(policy, (unsigned char const *)query);
    if (actual != expected) {
        fprintf(stderr, "weighted lookup: got %d, expected %d\n",
                actual, expected);
    }
    return actual == expected;
}

int
test_fh_weight_common(int argc, char **argv)
{
    sixel_allocator_t *allocator;
    sixel_factory_t *factory;
    void *service;
    sixel_lookup_policy_interface_t *policy;
    sixel_lookup_policy_interface_t *other;
    float palette[9];
    float saved[9];
    float query[3];
    float range;
    float span;
    int space;
    int refine;
    int count;
    int expected;
    int result;
    int pass;

    allocator = NULL;
    factory = NULL;
    service = NULL;
    policy = NULL;
    other = NULL;
    result = EXIT_FAILURE;
    if (argc != 5) {
        return EXIT_FAILURE;
    }
    space = atoi(argv[1]);
    refine = strcmp(argv[3], "refine") == 0;
    count = strcmp(argv[3], "expanded") == 0 ? 3 : 2;
    expected = atoi(argv[4]);
    range = space == 0 ? 3.f : space == 1 ? 2.f : 1.f;
    span = count == 3 ? 2.f : 1.f;
    memset(palette, 0, sizeof(palette));
    memset(query, 0, sizeof(query));

    /* Sources and query lie on R64 lattice sites. Squared grid distances
     * are 18^2 against 10^2 before weighting. Default L emphasis divides
     * the former by 9 (CIELAB/OKLab) or 4 (DIN99d). Fractional L ratios
     * 1.79/1.81 compare costs 324 against 320.41/327.61 before scaling:
     * margin >= 3.59 versus roundoff < 0.01 in those units. Expanded L
     * uses six steps over twice the span.
     * A distant third seed expands the grid without changing the metric. */
    query[0] = span * 32.f / 63.f;
    query[1] = -range / 2.f + range * 14.f / 63.f;
    palette[0] = query[0];
    palette[1] = -range / 2.f + range * 32.f / 63.f;
    palette[3] = query[0] + span * (count == 3 ? 6.f : 10.f) / 63.f;
    palette[4] = query[1];
    palette[6] = 2.f;
    palette[7] = range / 2.f;
    if (space == 2 || space == 4) {
        /* RGB uses unit spans with a nominal box starting at zero. */
        query[1] += .5f;
        palette[1] += .5f;
        palette[4] += .5f;
    }
    if (refine) {
        /* Both entries round to one seed whose last label is #1.
         * Native collision refinement must choose #0 with weighting:
         * .02^2/range^4 < .00625^2 < .02^2/range^2.
         * The tightest margin exceeds 5e-6, versus < 1e-8 arithmetic
         * uncertainty. OKLab uses .004^2/9 < .002^2 < .004^2,
         * with > 2e-6 margin. Disabling refinement fails weighted cases. */
        palette[0] = .5f;
        palette[1] = space == 3 ? .004f : .02f;
        palette[3] = space == 3 ? .502f : .50625f;
        palette[4] = 0.f;
        query[0] = .5f;
        query[1] = 0.f;
    }
    if (strcmp(argv[3], "outside") == 0) {
        /* L=1.25 is outside the nominal and palette bounds. Distances
         * are .25^2 + .8^2/range^(4 or 2), versus .35^2. Both spaces
         * reverse the winner; all margins exceed .01. */
        palette[0] = 1.f;
        palette[1] = .8f;
        palette[3] = .9f;
        palette[4] = 0.f;
        query[0] = 1.25f;
        query[1] = 0.f;
    }
    if (strcmp(argv[3], "opponent") == 0) {
        /* Equal L makes its coefficient irrelevant. In normalized grid
         * units the two costs are (s_a*18)^2 and (s_b*10)^2. Swapping
         * the fractional a/b scales (.5 versus 1) reverses the winner,
         * with a margin of at least 19 units. */
        query[2] = query[1];
        palette[2] = query[2];
        palette[3] = query[0];
        palette[5] = query[2] + range * 10.f / 63.f;
    }
    memcpy(saved, palette, sizeof(saved));
    if (SIXEL_FAILED(sixel_allocator_new(&allocator, NULL, NULL, NULL,
            NULL)) ||
            SIXEL_FAILED(sixel_components_getservice("services/factory",
                &service))) {
        goto end;
    }
    factory = service;
    if (SIXEL_FAILED(factory->vtbl->create(factory,
                "lookup/fhedt.float32", allocator, (void **)&policy))) {
        goto end;
    }
    if (!fh_weight_prepare(policy, allocator, palette, space, argv[2],
            refine, count, NULL) || !fh_weight_map(policy, query, expected)) {
        goto end;
    }
    if (strcmp(argv[3], "isolation") == 0) {
        if (SIXEL_FAILED(factory->vtbl->create(factory,
                "lookup/fhedt.float32", allocator, (void **)&other))) {
            goto end;
        }
        /* Warm the voxel cache in two live objects, then change one
         * metric. A reuse hint from the opposite mode must be ignored
         * without stealing its state or reusing a stale grid/cache. */
        for (pass = 0; pass < 3; ++pass) {
            if (!fh_weight_prepare(other, allocator, palette, space,
                    pass % 2 ? "3/1/1" : "1/1/1", 0, count,
                    pass == 0 ? policy : NULL) ||
                    !fh_weight_map(other, query, pass % 2 ? 0 : 1) ||
                    !fh_weight_map(policy, query, expected)) {
                goto end;
            }
        }
    }
    if (memcmp(saved, palette, sizeof(saved)) != 0) {
        goto end;
    }
    result = EXIT_SUCCESS;
end:
    if (other != NULL) {
        other->vtbl->unref(other);
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
