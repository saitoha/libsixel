/* SPDX-License-Identifier: MIT */
#include <math.h>
#include "fixture.h"

static double
lso2_gpu_reference_distance(double const *q, unsigned char const *palette,
    unsigned int i, unsigned int shift)
{
    double d, sum;
    unsigned int c;

    sum = 0;
    for (c = 0; c < 3; ++c) {
        d = q[c] - ((palette[3*i+c] >> shift) << shift);
        sum += d*d;
    }
    return sum;
}

/* Enumerate all quantized seeds, independently of the GPU's three EDT
 * passes. Earlier z/y/x coordinates win ties; a seed collision keeps the
 * final palette entry. This tests construction as well as table lookup. */
static unsigned int
lso2_gpu_reference_seed(unsigned int const *cell, unsigned char const *palette,
    unsigned int count, unsigned int shift)
{
    unsigned int i, c, best, distance, minimum, order, best_order, seed[3];
    int d;

    minimum = ~0U;
    best_order = ~0U;
    best = 0;
    for (i = 0; i < count; ++i) {
        distance = 0;
        for (c = 0; c < 3; ++c) {
            seed[c] = palette[3*i+c] >> shift;
            d = (int)cell[c]-(int)seed[c];
            distance += (unsigned int)(d*d);
        }
        order = (seed[2] << 16) | (seed[1] << 8) | seed[0];
        if (distance < minimum || (distance == minimum &&
                order <= best_order)) {
            minimum = distance; best_order = order; best = i;
        }
    }
    return best;
}

static unsigned int
lso2_gpu_reference_fhedt(sixel_lookup_policy_configuration_t const *config,
    unsigned char const *palette, unsigned int count, double const *q)
{
    unsigned int cell[3], neighbor[3], corner[3], c, i, best, candidate;
    unsigned int r, shift;
    double minimum, distance;
    int side, boundary;

    r = (unsigned int)config->fhedt_resolution;
    shift = r == 64 ? 2 : r == 128 ? 1 : 0;
    for (c = 0; c < 3; ++c) cell[c] = (unsigned int)q[c] >> shift;
    best = lso2_gpu_reference_seed(cell,palette,count,shift);
    if (!config->fhedt_refine) return best;
    boundary = 0;
    for (c = 0; c < 3; ++c) {
        for (side = -1; side <= 1; side += 2) {
            memcpy(neighbor,cell,sizeof(neighbor));
            if ((side < 0 && cell[c] == 0) ||
                    (side > 0 && cell[c]+1 == r)) continue;
            neighbor[c] = (unsigned int)((int)cell[c]+side);
            boundary |= lso2_gpu_reference_seed(neighbor,palette,count,shift)
                != best;
        }
    }
    if (!boundary) return best;
    /* The shortcut can only return the same zero-distance seed. Enumerating
     * corners is an independent reference for both dist2 settings. */
    minimum = 1e30;
    for (i = 0; i < 8; ++i) {
        for (c = 0; c < 3; ++c) {
            corner[c] = cell[c]+((i >> c)&1);
            if (corner[c] >= r) corner[c] = r-1;
        }
        candidate = lso2_gpu_reference_seed(corner,palette,count,shift);
        distance = lso2_gpu_reference_distance(q,palette,candidate,shift);
        if (distance < minimum) { minimum = distance; best = candidate; }
    }
    return best;
}

static unsigned int
lso2_gpu_reference_eytzinger(unsigned char const *palette, unsigned int count,
    double const *q)
{
    unsigned int order[256], keys[256], i, j, rank, best, at, index, step;
    int position[2], open[2], side;
    double projection, minimum, distance, gap;

    for (i = 0; i < count; ++i) {
        keys[i] = palette[3*i]+palette[3*i+1]+palette[3*i+2];
        j = i;
        while (j && keys[order[j-1]] > keys[i]) {
            order[j] = order[j-1]; --j;
        }
        order[j] = i;
    }
    projection = q[0]+q[1]+q[2];
    rank = 0;
    while (rank+1 < count && keys[order[rank]] < projection) ++rank;
    best = order[rank];
    minimum = lso2_gpu_reference_distance(q,palette,best,0);
    position[0] = position[1] = (int)rank;
    open[0] = open[1] = 1;
    for (step = 0; step < 6; ++step) {
        for (side = 0; side < 2; ++side) {
            if (!open[side]) continue;
            position[side] += side ? 1 : -1;
            if (position[side] < 0 || position[side] >= (int)count) {
                open[side] = 0; continue;
            }
            at = (unsigned int)position[side];
            index = order[at];
            gap = projection-keys[index];
            if (gap*gap > minimum) { open[side] = 0; continue; }
            distance = lso2_gpu_reference_distance(q,palette,index,0);
            if (distance < minimum || (distance == minimum && index < best)) {
                minimum = distance; best = index;
            }
        }
    }
    return best;
}

unsigned int
lso2_gpu_fractional_reference(sixel_lookup_policy_configuration_t const *config,
    unsigned char const *palette, unsigned int count, double const *q)
{
    double center[3], minimum, distance;
    unsigned int i, c, best, shift, maximum, address, v, bucket, low, high;

    if (config->kind == SIXEL_LUT_POLICY_FHEDT)
        return lso2_gpu_reference_fhedt(config,palette,count,q);
    if (config->kind == SIXEL_LUT_POLICY_EYTZINGER)
        return lso2_gpu_reference_eytzinger(palette,count,q);
    memcpy(center,q,sizeof(center));
    if (config->kind == SIXEL_LUT_POLICY_5BIT ||
            config->kind == SIXEL_LUT_POLICY_6BIT) {
        shift = config->kind == SIXEL_LUT_POLICY_5BIT ? 3 : 2;
        maximum = (256U >> shift)-1;
        for (c = 0; c < 3; ++c) {
            address = (unsigned int)floor((q[c]+(1U << (shift-1))) /
                (1U << shift));
            if (address > maximum) address = maximum;
            low = 255; high = 0;
            /* Recover the integer preimage instead of reusing the shader's
             * clipped-center arithmetic. No RGB cube is enumerated. */
            for (v = 0; v < 256; ++v) {
                bucket = (v+(1U << (shift-1))) >> shift;
                if (bucket > maximum) bucket = maximum;
                if (bucket != address) continue;
                if (v < low) low = v;
                if (v > high) high = v;
            }
            center[c] = (low+high)*.5;
        }
    }
    minimum = 1e30;
    best = 0;
    for (i = 0; i < count; ++i) {
        distance = lso2_gpu_reference_distance(center,palette,i,0);
        if (distance < minimum) { minimum = distance; best = i; }
    }
    return best;
}
