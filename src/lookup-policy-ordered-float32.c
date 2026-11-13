/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <float.h>
#include <math.h>
#include <string.h>
#include "lookup-policy-ordered.h"
#include "pixelformat.h"
#include "sixel_atomic.h"

/* Native float coordinates retain the normalized double-distance contract.
 * This component owns its representation and never constructs another policy.
 * Bounds use conservative relative slack; final comparisons retain channel
 * order and the lowest original palette index at an exact tie. */
typedef struct ordered_float {
    sixel_lookup_policy_interface_t iface;
    sixel_atomic_u32_t refs;
    sixel_allocator_t *allocator;
    int count, bounded;
    double palette[256][3], weights[3];
    double radii[256][256];
    unsigned char indexes[256][256];
} ordered_float_t;

static void
ordered_float_ref(sixel_lookup_policy_interface_t *self)
{
    ordered_float_t *lookup;

    lookup = (ordered_float_t *)self;
    (void)sixel_atomic_fetch_add_u32(&lookup->refs, 1);
}

static void
ordered_float_unref(sixel_lookup_policy_interface_t *self)
{
    ordered_float_t *lookup;
    sixel_allocator_t *allocator;

    lookup = (ordered_float_t *)self;
    if (sixel_atomic_fetch_sub_u32(&lookup->refs, 1) != 1) return;
    allocator = lookup->allocator;
    sixel_allocator_free(allocator, lookup);
    sixel_allocator_unref(allocator);
}

/* Accumulate from zero, including zero-weight channels. This preserves the
 * established NaN/Inf behavior; an entirely nonfinite scan returns zero. */
static double
ordered_float_distance(ordered_float_t const *lookup, double const *q, int i)
{
    double distance, difference;
    int c;

    distance = 0.0;
    for (c = 0; c < 3; ++c) {
        difference = q[c] - lookup->palette[i][c];
        distance += difference * difference * lookup->weights[c];
    }
    return distance;
}

/* Every row orders competitors by distance from a palette seed. Stable
 * insertion sorting also fixes the original-index order at equal radii. */
static void
ordered_float_build(ordered_float_t *lookup)
{
    double radius;
    int row, i, j;

    for (row = 0; row < lookup->count; ++row) {
        for (i = 0; i < lookup->count; ++i) {
            radius = ordered_float_distance(lookup, lookup->palette[row], i);
            j = i;
            while (j > 0 && lookup->radii[row][j - 1] > radius) {
                lookup->radii[row][j] = lookup->radii[row][j - 1];
                lookup->indexes[row][j] = lookup->indexes[row][j - 1];
                --j;
            }
            lookup->radii[row][j] = radius;
            lookup->indexes[row][j] = (unsigned char)i;
        }
    }
}

static SIXELSTATUS
ordered_float_prepare(sixel_lookup_policy_interface_t *self,
    sixel_lookup_policy_prepare_request_t const *request)
{
    ordered_float_t *lookup;
    float range, weight;
    int stride, i, c;

    if (self == NULL) return SIXEL_BAD_ARGUMENT;
    lookup = (ordered_float_t *)self;
    lookup->count = 0;
    lookup->bounded = 0;
    if (request == NULL || request->allocator == NULL ||
            request->palette == NULL || request->depth != 3 ||
            request->reqcolor < 1 || request->reqcolor > 256)
        return SIXEL_BAD_ARGUMENT;
    stride = 0;
    if (request->palette_float != NULL && request->float_depth > 0) {
        if (request->float_depth < 3 * (int)sizeof(float))
            return SIXEL_BAD_ARGUMENT;
        stride = request->float_depth / (int)sizeof(float);
    }
    lookup->count = request->reqcolor;
    lookup->bounded = 1;
    for (c = 0; c < 3; ++c) {
        range = sixel_pixelformat_float_channel_range(request->pixelformat, c);
        if (range <= 0.0f) range = 1.0f;
        weight = (1.0f / range) * (1.0f / range);
        lookup->weights[c] = weight;
        if (!isfinite(weight) || weight < 0) lookup->bounded = 0;
    }
    for (i = 0; i < lookup->count; ++i) {
        for (c = 0; c < 3; ++c) {
            lookup->palette[i][c] = stride > 0
                ? request->palette_float[i * stride + c]
                : sixel_pixelformat_byte_to_float(request->pixelformat, c,
                    request->palette[i * 3 + c]);
            if (!isfinite(lookup->palette[i][c])) lookup->bounded = 0;
        }
    }
    if (lookup->bounded) ordered_float_build(lookup);
    return SIXEL_OK;
}

static int
ordered_float_hint(sixel_lookup_policy_interface_t const *self,
    unsigned char const *pixel, int hint)
{
    ordered_float_t const *lookup;
    float sample[3];
    double q[3], minimum, distance, radius;
    int best, seed, i, index, bounded;

    if (self == NULL || pixel == NULL) return 0;
    lookup = (ordered_float_t const *)self;
    if (lookup->count == 0) return 0;
    memcpy(sample, pixel, sizeof(sample));
    bounded = lookup->bounded;
    for (i = 0; i < 3; ++i) {
        q[i] = sample[i];
        if (!isfinite(q[i])) bounded = 0;
    }
    seed = hint >= 0 && hint < lookup->count ? hint : 0;
    radius = ordered_float_distance(lookup, q, seed) * (4.0 * (1.0 + 1e-9));
    best = 0;
    minimum = DBL_MAX;
    for (i = 0; i < lookup->count; ++i) {
        /* Triangle inequality: a competitor beyond twice the seed radius
         * cannot beat the seed. Nonfinite inputs use this policy's scan. */
        if (bounded && lookup->radii[seed][i] > radius) break;
        index = bounded ? lookup->indexes[seed][i] : i;
        distance = ordered_float_distance(lookup, q, index);
        if (distance < minimum || (distance == minimum && index < best)) {
            minimum = distance;
            best = index;
        }
    }
    return best;
}

static int
ordered_float_map(sixel_lookup_policy_interface_t const *self,
    unsigned char const *pixel)
{
    return ordered_float_hint(self, pixel, -1);
}

static void
ordered_float_map_pixels(sixel_lookup_policy_interface_t const *self,
    unsigned char const *pixels, size_t count, int pixel_stride,
    sixel_index_t *result, int result_stride, void *workspace)
{
    size_t i;

    (void)workspace;
    if (self == NULL || pixels == NULL || result == NULL) return;
    for (i = 0; i < count; ++i) {
        *result = (sixel_index_t)ordered_float_map(self, pixels);
        /* A negative stride must not form a pointer before the allocation. */
        if (i + 1 < count) {
            pixels += pixel_stride;
            result += result_stride;
        }
    }
}

static int
ordered_float_kind(sixel_lookup_policy_interface_t const *self)
{
    (void)self;
    return SIXEL_LUT_POLICY_ORDERED;
}

static sixel_lookup_policy_vtbl_t const ordered_float_vtbl = {
    ordered_float_ref, ordered_float_unref, ordered_float_prepare,
    ordered_float_map,
    ordered_float_hint, ordered_float_map_pixels, NULL, ordered_float_kind, NULL
};

SIXELSTATUS
sixel_lookup_policy_ordered_float32_new(sixel_allocator_t *allocator,
    void **result)
{
    ordered_float_t *lookup;

    if (allocator == NULL || result == NULL) return SIXEL_BAD_ARGUMENT;
    *result = NULL;
    lookup = sixel_allocator_calloc(allocator, 1, sizeof(*lookup));
    if (lookup == NULL) return SIXEL_BAD_ALLOCATION;
    lookup->iface.vtbl = &ordered_float_vtbl;
    lookup->refs = 1;
    lookup->allocator = allocator;
    sixel_allocator_ref(allocator);
    *result = &lookup->iface;
    return SIXEL_OK;
}
