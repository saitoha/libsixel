/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <float.h>
#include <math.h>
#include <string.h>
#include "lookup-policy-gridlut.h"
#include "pixelformat.h"
#include "sixel_atomic.h"

/* Native float coordinates retain the normalized double-distance contract.
 * This component owns its representation and never constructs another policy.
 * Bounds use conservative relative slack; final comparisons retain channel
 * order and the lowest original palette index at an exact tie. */
typedef struct gridlut_float {
    sixel_lookup_policy_interface_t iface;
    sixel_atomic_u32_t refs;
    sixel_allocator_t *allocator;
    int count, bounded;
    double palette[256][3], weights[3];
    double edges[3][17];
    unsigned char counts[4096], indexes[4096][64];
} gridlut_float_t;

static void
gridlut_float_ref(sixel_lookup_policy_interface_t *self)
{
    gridlut_float_t *lookup;

    lookup = (gridlut_float_t *)self;
    (void)sixel_atomic_fetch_add_u32(&lookup->refs, 1);
}

static void
gridlut_float_unref(sixel_lookup_policy_interface_t *self)
{
    gridlut_float_t *lookup;
    sixel_allocator_t *allocator;

    lookup = (gridlut_float_t *)self;
    if (sixel_atomic_fetch_sub_u32(&lookup->refs, 1) != 1) return;
    allocator = lookup->allocator;
    sixel_allocator_free(allocator, lookup);
    sixel_allocator_unref(allocator);
}

/* Accumulate from zero, including zero-weight channels. This preserves the
 * established NaN/Inf behavior; an entirely nonfinite scan returns zero. */
static double
gridlut_float_distance(gridlut_float_t const *lookup, double const *q, int i)
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

/* The grid spans the palette's native coordinate box, rather than a
 * byte cube. Stored edges are also used for addressing, so rounded cell
 * arithmetic cannot exclude a query from the box used to certify its list. */
static void
gridlut_float_build(gridlut_float_t *lookup)
{
    double low[3], high[3], minimum, maximum, bound, distance, gap, a, b;
    int c, i, cell, axis, kept;

    for (c = 0; c < 3; ++c) {
        minimum = maximum = lookup->palette[0][c];
        for (i = 1; i < lookup->count; ++i) {
            if (lookup->palette[i][c] < minimum)
                minimum = lookup->palette[i][c];
            if (lookup->palette[i][c] > maximum)
                maximum = lookup->palette[i][c];
        }
        for (i = 0; i <= 16; ++i)
            lookup->edges[c][i] = minimum + (maximum - minimum) * (i / 16.0);
    }
    for (cell = 0; cell < 4096; ++cell) {
        for (c = 0; c < 3; ++c) {
            axis = (cell >> (8 - c * 4)) & 15;
            low[c] = lookup->edges[c][axis];
            high[c] = lookup->edges[c][axis + 1];
        }
        bound = DBL_MAX;
        for (i = 0; i < lookup->count; ++i) {
            distance = 0.0;
            for (c = 0; c < 3; ++c) {
                a = lookup->palette[i][c] - low[c];
                b = high[c] - lookup->palette[i][c];
                gap = a > b ? a : b;
                distance += gap * gap * lookup->weights[c];
            }
            if (distance < bound) bound = distance;
        }
        bound *= 1.0 + 1e-9;
        kept = 0;
        for (i = 0; i < lookup->count; ++i) {
            distance = 0.0;
            for (c = 0; c < 3; ++c) {
                gap = lookup->palette[i][c] < low[c]
                    ? low[c] - lookup->palette[i][c]
                    : lookup->palette[i][c] > high[c]
                        ? lookup->palette[i][c] - high[c] : 0.0;
                distance += gap * gap * lookup->weights[c];
            }
            if (distance <= bound) {
                if (kept < 64) lookup->indexes[cell][kept] = (unsigned char)i;
                ++kept;
            }
        }
        /* Zero denotes overflow, never an incomplete candidate list. */
        lookup->counts[cell] = kept <= 64 ? (unsigned char)kept : 0;
    }
}

static SIXELSTATUS
gridlut_float_prepare(sixel_lookup_policy_interface_t *self,
    sixel_lookup_policy_prepare_request_t const *request)
{
    gridlut_float_t *lookup;
    float range, weight;
    int stride, i, c;

    if (self == NULL) return SIXEL_BAD_ARGUMENT;
    lookup = (gridlut_float_t *)self;
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
    if (lookup->bounded) gridlut_float_build(lookup);
    return SIXEL_OK;
}

static int
gridlut_float_hint(sixel_lookup_policy_interface_t const *self,
    unsigned char const *pixel, int hint)
{
    gridlut_float_t const *lookup;
    float sample[3];
    double q[3], minimum, distance;
    int best, i, c, index, bounded, cell, axis, count;

    (void)hint;
    if (self == NULL || pixel == NULL) return 0;
    lookup = (gridlut_float_t const *)self;
    if (lookup->count == 0) return 0;
    memcpy(sample, pixel, sizeof(sample));
    bounded = lookup->bounded;
    cell = 0;
    for (c = 0; c < 3; ++c) {
        q[c] = sample[c];
        if (!isfinite(q[c]) || q[c] < lookup->edges[c][0] ||
                q[c] > lookup->edges[c][16]) bounded = 0;
        axis = 0;
        while (axis < 15 && q[c] > lookup->edges[c][axis + 1]) ++axis;
        cell = cell * 16 + axis;
    }
    count = bounded ? lookup->counts[cell] : 0;
    if (count == 0) bounded = 0;
    if (!bounded) count = lookup->count;
    best = 0;
    minimum = DBL_MAX;
    for (i = 0; i < count; ++i) {
        index = bounded ? lookup->indexes[cell][i] : i;
        distance = gridlut_float_distance(lookup, q, index);
        if (distance < minimum) {
            minimum = distance;
            best = index;
        }
    }
    return best;
}

static int
gridlut_float_map(sixel_lookup_policy_interface_t const *self,
    unsigned char const *pixel)
{
    return gridlut_float_hint(self, pixel, -1);
}

static void
gridlut_float_map_pixels(sixel_lookup_policy_interface_t const *self,
    unsigned char const *pixels, size_t count, int pixel_stride,
    sixel_index_t *result, int result_stride, void *workspace)
{
    size_t i;

    (void)workspace;
    if (self == NULL || pixels == NULL || result == NULL) return;
    for (i = 0; i < count; ++i) {
        *result = (sixel_index_t)gridlut_float_map(self, pixels);
        /* A negative stride must not form a pointer before the allocation. */
        if (i + 1 < count) {
            pixels += pixel_stride;
            result += result_stride;
        }
    }
}

static int
gridlut_float_kind(sixel_lookup_policy_interface_t const *self)
{
    (void)self;
    return SIXEL_LUT_POLICY_GRIDLUT;
}

static sixel_lookup_policy_vtbl_t const gridlut_float_vtbl = {
    gridlut_float_ref, gridlut_float_unref, gridlut_float_prepare,
    gridlut_float_map,
    gridlut_float_hint, gridlut_float_map_pixels, NULL, gridlut_float_kind, NULL
};

SIXELSTATUS
sixel_lookup_policy_gridlut_float32_new(sixel_allocator_t *allocator,
    void **result)
{
    gridlut_float_t *lookup;

    if (allocator == NULL || result == NULL) return SIXEL_BAD_ARGUMENT;
    *result = NULL;
    lookup = sixel_allocator_calloc(allocator, 1, sizeof(*lookup));
    if (lookup == NULL) return SIXEL_BAD_ALLOCATION;
    lookup->iface.vtbl = &gridlut_float_vtbl;
    lookup->refs = 1;
    lookup->allocator = allocator;
    sixel_allocator_ref(allocator);
    *result = &lookup->iface;
    return SIXEL_OK;
}
