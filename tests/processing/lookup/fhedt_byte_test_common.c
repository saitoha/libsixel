/* SPDX-License-Identifier: MIT */
/* Exhaustive integer distances are independent of the separable EDT. */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/lookup-fhedt-8bit.h"

static int
fh_byte_setup(sixel_lookup_fhedt_8bit_t *lookup,
              unsigned char const *palette, int count, int resolution,
              int refine, int dist2, int cache, int weighted)
{
    uint32_t signature;

    if (SIXEL_FAILED(sixel_lookup_fhedt_8bit_configure(lookup,
        palette, count, resolution, refine, dist2, cache, 0, 8, 8, 0, 0,
        weighted ? 7 : 1, weighted ? 3 : 1, 1,
        SIXEL_PIXELFORMAT_RGB888, 3, 0))) {
        return 0;
    }
    /* The policy adapter publishes this signature before cache access. */
    signature = sixel_lookup_fhedt_8bit_signature(palette, count,
        resolution, refine, weighted ? 7 : 1, weighted ? 3 : 1, 1, 3);
    sixel_lookup_fhedt_8bit_shared_set_signature(lookup->shared, signature);
    return 1;
}

static int
fh_byte_expect(sixel_lookup_fhedt_8bit_t *lookup,
               unsigned char const *query, int expected)
{
    int actual;

    actual = sixel_lookup_fhedt_8bit_map(lookup, query);
    if (actual != expected) {
        fprintf(stderr, "byte FHEDT query %u,%u,%u: got %d expected %d\n",
            (unsigned)query[0], (unsigned)query[1], (unsigned)query[2],
            actual, expected);
    }
    return actual == expected;
}

static int
fh_byte_labels(sixel_lookup_fhedt_8bit_t *lookup, int mode, int resolution)
{
    unsigned char palette[257 * 3];
    unsigned char query[3] = { 0, 16, 0 };
    unsigned char black[3] = { 0, 0, 0 };
    int count;
    int last;
    int axis;

    count = mode == 4 ? 257 : 2;
    last = count - 1;
    axis = mode == 1 ? 2 : 1;
    memset(palette, 0, sizeof(palette));
    palette[last * 3] = 12;
    palette[last * 3 + axis] = 8;
    query[1] = 0;
    query[axis] = 16;
    /* At R64 the relevant Y/Z line has costs 0 at site 0 and 9 at
     * site 2. Site 2 itself selects source 0, but site 4 selects source 2.
     * In-place label writes destroy that still-needed second source.
     * Squared byte distances are 256 versus 208 (grid costs 16 versus 13).
     * The winner is strict and remains so at R128/R256. */
    if (!fh_byte_setup(lookup, palette, count, resolution, mode >= 6,
            mode == 7, mode == 7, 0) ||
            !fh_byte_expect(lookup, query, last)) {
        return 0;
    }
    if (mode == 4) {
        /* Duplicate seed sites deliberately retain the last palette label;
         * this also checks that label 256 does not narrow to one byte. */
        return fh_byte_expect(lookup, black, 255);
    }
    if (mode != 7) {
        return 1;
    }
    if (!fh_byte_expect(lookup, query, 1)) {
        return 0;
    }
    memcpy(palette, palette + 3, 3);
    memset(palette + 3, 0, 3);
    /* Reprepare the same object with reversed labels after warming its
     * voxel cache. No old label may survive the new grid publication. */
    return fh_byte_setup(lookup, palette, 2, resolution, 1, 1, 1, 0)
        && fh_byte_expect(lookup, query, 0)
        && fh_byte_expect(lookup, query, 0);
}

static int
fh_byte_grid(sixel_lookup_fhedt_8bit_t *lookup, int weighted, int resolution)
{
    static int const seeds[9][3] = {
        {0,0,0}, {63,63,63}, {3,19,47}, {41,7,24}, {15,51,9},
        {56,34,13}, {28,46,55}, {9,12,39}, {32,32,32}
    };
    unsigned char palette[27];
    unsigned char query[3];
    int points[9][3];
    int coord[3];
    int weights[3];
    int distances[9];
    int best;
    int delta;
    int index;
    int axis;
    int actual;
    int step;
    int x;
    int y;
    int z;

    step = 256 / resolution;
    weights[0] = weighted ? 7 : 1;
    weights[1] = weighted ? 3 : 1;
    weights[2] = 1;
    for (index = 0; index < 9; ++index) {
        for (axis = 0; axis < 3; ++axis) {
            points[index][axis] =
                (seeds[index][axis] * (resolution - 1) + 31) / 63;
            palette[index * 3 + axis] =
                (unsigned char)(points[index][axis] * step);
        }
    }
    if (!fh_byte_setup(lookup, palette, 9, resolution, 0, 0, 0, weighted)) {
        return 0;
    }
    /* Every lattice site is checked against all seeds. Integer coordinates
     * and weights give exact costs below 2^20, with >=1 between unequal
     * costs. Accept every minimum-distance tie, not a specific tie index.
     * This proves discrete-grid labels, not arbitrary subvoxel refinement. */
    for (z = 0; z < resolution; ++z) {
        for (y = 0; y < resolution; ++y) {
            for (x = 0; x < resolution; ++x) {
                coord[0] = x;
                coord[1] = y;
                coord[2] = z;
                for (axis = 0; axis < 3; ++axis) {
                    query[axis] = (unsigned char)(coord[axis] * step);
                }
                best = INT_MAX;
                for (index = 0; index < 9; ++index) {
                    distances[index] = 0;
                    for (axis = 0; axis < 3; ++axis) {
                        delta = coord[axis] - points[index][axis];
                        distances[index] += weights[axis] * delta * delta;
                    }
                    if (distances[index] < best) {
                        best = distances[index];
                    }
                }
                actual = sixel_lookup_fhedt_8bit_map(lookup, query);
                if (actual < 0 || actual >= 9 || distances[actual] != best) {
                    fprintf(stderr, "byte grid W=%d q=%d,%d,%d index=%d\n",
                        weighted, x, y, z, actual);
                    return 0;
                }
            }
        }
    }
    return 1;
}

int
test_fh_byte_common(int mode, int resolution)
{
    sixel_allocator_t *allocator;
    sixel_lookup_fhedt_8bit_t *lookup;
    int ok;

    allocator = NULL;
    lookup = NULL;
    ok = 0;
    if (mode < 0 || mode > 7 ||
            (resolution != 64 && resolution != 128 && resolution != 256) ||
            SIXEL_FAILED(sixel_allocator_new(&allocator,
                NULL, NULL, NULL, NULL)) ||
            SIXEL_FAILED(sixel_lookup_fhedt_8bit_create(allocator, &lookup))) {
        goto end;
    }
    ok = mode == 2 || mode == 3 || mode == 5
        ? fh_byte_grid(lookup, mode == 3, resolution)
        : fh_byte_labels(lookup, mode, resolution);
end:
    sixel_lookup_fhedt_8bit_unref(lookup);
    sixel_allocator_unref(allocator);
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
