/* SPDX-License-Identifier: MIT */
/* Small sparse fixtures and an independent integer site-scan oracle. */
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "src/lookup-fhedt-8bit.h"
#include "../gpu-palette/ordered_state_common.h"

static int
fh_sparse_expected(unsigned char const *palette, int colors,
    unsigned char const *query, int weighted)
{
    int i, c, d, distance, minimum, key, best_key, best;

    minimum = INT_MAX;
    best_key = INT_MAX;
    best = -1;
    for (i = 0; i < colors; ++i) {
        distance = 0;
        key = 0;
        for (c = 0; c < 3; ++c) {
            d = (int)query[c] - (int)palette[3 * i + c];
            distance += d * d * (weighted ? (c == 0 ? 7 : c == 1 ? 3 : 1)
                : 1);
            key |= (int)palette[3 * i + c] << (8 * c);
        }
        /* Axis ties choose lower Z/Y/X; duplicate sites keep the last
         * label. All integer costs are exact and bounded below INT_MAX. */
        if (distance < minimum ||
                (distance == minimum && key <= best_key)) {
            minimum = distance;
            best_key = key;
            best = i;
        }
    }
    return best;
}

int
test_fh_sparse(int gpu, int resolution, int refine, int weighted, int dense)
{
    static unsigned char const sparse[] = {
        24, 0, 0, 8, 0, 0, 8, 0, 0, 0, 16, 0, 0, 0, 16, 252, 252, 252
    };
    static unsigned char const ties[] = {
        16, 0, 0, 8, 0, 0, 0, 4, 0, 0, 12, 0, 0, 0, 12, 0, 8, 8
    };
    ordered_state_fixture_t fixture;
    sixel_gpu_palette_test_hooks_t hooks;
    sixel_allocator_t *allocator;
    sixel_lookup_fhedt_8bit_t *lookup;
    int i, c, expected, actual, ok, mask;

    allocator = NULL;
    lookup = NULL;
    ok = 0;
    ordered_state_init(&fixture, dense ? 256 : 6, 1);
    memcpy(fixture.palette, sparse, sizeof(sparse));
    if (dense) {
        /* One long list with unsorted X sites and repeated seed voxels. */
        for (i = 0; i < 256; ++i) {
            fixture.palette[3 * i] = (unsigned char)((i * 44 + 20) & 252);
            fixture.palette[3 * i + 1] = 0;
            fixture.palette[3 * i + 2] = 0;
        }
    }
    mask = resolution == 64 ? 252 : resolution == 128 ? 254 : 255;
    for (i = 0; i < ORDERED_STATE_PIXELS; ++i) {
        for (c = 0; c < 3; ++c) fixture.pixels[3 * i + c] &= mask;
    }
    memcpy(fixture.pixels, ties, sizeof(ties));
    if (gpu) {
        memset(&hooks, 0, sizeof(hooks));
        fixture.request.test_hooks = &hooks;
        fixture.request.lut_policy = SIXEL_LUT_POLICY_FHEDT;
        fixture.request.fhedt_configured = 1;
        fixture.request.fhedt_resolution = resolution;
        fixture.request.fhedt_refine = refine;
        fixture.request.fhedt_use_dist2 = 1;
        fixture.request.fhedt_weights[0] = 1;
        fixture.request.fhedt_weights[1] = 1;
        fixture.request.fhedt_weights[2] = 1;
        if (sixel_gpu_palette_apply(&fixture.request) != SIXEL_OK ||
                hooks.kernel != 7 || !hooks.published) goto end;
    } else {
        if (sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL)
                != SIXEL_OK ||
                sixel_lookup_fhedt_8bit_create(allocator, &lookup)
                != SIXEL_OK ||
                sixel_lookup_fhedt_8bit_configure(lookup, fixture.palette,
                    dense ? 256 : 6, resolution, refine, 1, 0, 0,
                    0, 0, 0, 0, weighted ? 7 : 1, weighted ? 3 : 1, 1,
                    SIXEL_PIXELFORMAT_RGB888, 3, 0) != SIXEL_OK) goto end;
    }
    for (i = 0; i < ORDERED_STATE_PIXELS; ++i) {
        expected = fh_sparse_expected(fixture.palette, dense ? 256 : 6,
            fixture.pixels + 3 * i, weighted);
        actual = gpu ? fixture.output[i]
            : sixel_lookup_fhedt_8bit_map(lookup, fixture.pixels + 3 * i);
        if (actual != expected) {
            fprintf(stderr, "sparse query %d: got %d expected %d\n",
                i, actual, expected);
            goto end;
        }
    }
    ok = 1;
end:
    sixel_lookup_fhedt_8bit_unref(lookup);
    sixel_allocator_unref(allocator);
    return ok ? 0 : 1;
}

static size_t fh_allocated_bytes;

static void *
fh_counted_malloc(size_t bytes)
{
    fh_allocated_bytes += bytes;
    return malloc(bytes);
}

int
test_fh_aux_memory(int resolution, int refine)
{
    unsigned char palette[] = {0, 0, 0, 255, 255, 255};
    sixel_allocator_t *allocator;
    sixel_lookup_fhedt_8bit_t *lookup;
    size_t grid, limit;
    int ok;

    allocator = NULL;
    lookup = NULL;
    ok = 0;
    fh_allocated_bytes = 0;
    /* One byte index and twelve scratch bytes per voxel, plus 16 KiB
     * for the object and palette. Even boundary bits exceed that margin.
     * Count allocations, not elapsed time or private structure fields. */
    grid = (size_t)resolution * resolution * resolution;
    limit = grid * 13U + 16384U;
    if (sixel_allocator_new(&allocator, fh_counted_malloc, NULL, NULL, NULL)
            != SIXEL_OK ||
            sixel_lookup_fhedt_8bit_create(allocator, &lookup) != SIXEL_OK ||
            sixel_lookup_fhedt_8bit_configure(lookup, palette, 2,
                resolution, refine, 1, 0, 0, 0, 0, 0, 0, 1, 1, 1,
                SIXEL_PIXELFORMAT_RGB888, 3, 0) != SIXEL_OK) goto end;
    ok = fh_allocated_bytes <= limit;
    if (!ok) fprintf(stderr, "FHEDT allocated %zu bytes; limit %zu\n",
        fh_allocated_bytes, limit);
end:
    sixel_lookup_fhedt_8bit_unref(lookup);
    sixel_allocator_unref(allocator);
    return ok ? 0 : 1;
}

/* emacs Local Variables:      */
/* emacs mode: c               */
/* emacs tab-width: 4          */
/* emacs indent-tabs-mode: nil */
/* emacs c-basic-offset: 4     */
/* emacs End:                  */
/* vim: set expandtab ts=4 sts=4 sw=4 : */
/* EOF */
