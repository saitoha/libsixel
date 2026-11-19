/* SPDX-License-Identifier: MIT */
#ifndef LIBSIXEL_LOOKUP_FHEDT_DISTANCES_H
#define LIBSIXEL_LOOKUP_FHEDT_DISTANCES_H

#include <stddef.h>
#include <sixel.h>

/* A limited heap can still fit the R=256 grid in two 64 MiB slabs. */
#define SIXEL_FHEDT_DISTANCE_SLAB (64U * 1024U * 1024U / sizeof(double))

typedef struct sixel_fhedt_distances {
    double *slab[2];
} sixel_fhedt_distances_t;

static inline int
sixel_fhedt_distances_alloc(sixel_fhedt_distances_t *grid,
    sixel_allocator_t *allocator, size_t count)
{
    size_t first_count;

    first_count = count;
#if defined(SIXEL_SPLIT_FHEDT_DISTANCE_GRID)
    if (first_count > SIXEL_FHEDT_DISTANCE_SLAB) {
        first_count = SIXEL_FHEDT_DISTANCE_SLAB;
    }
#endif
    grid->slab[1] = NULL;
    grid->slab[0] = sixel_allocator_malloc(allocator,
                                          first_count * sizeof(double));
    if (grid->slab[0] == NULL) {
        return 0;
    }
    if (first_count != count) {
        grid->slab[1] = sixel_allocator_malloc(allocator,
                                          (count - first_count)
                                          * sizeof(double));
        return grid->slab[1] != NULL;
    }
    return 1;
}

static inline void
sixel_fhedt_distances_free(sixel_fhedt_distances_t *grid,
    sixel_allocator_t *allocator)
{
    if (grid->slab[1] != NULL) {
        sixel_allocator_free(allocator, grid->slab[1]);
    }
    sixel_allocator_free(allocator, grid->slab[0]);
}

static inline double *
sixel_fhedt_distance_at(sixel_fhedt_distances_t const *grid, size_t offset)
{
#if defined(SIXEL_SPLIT_FHEDT_DISTANCE_GRID)
    if (offset >= SIXEL_FHEDT_DISTANCE_SLAB) {
        return grid->slab[1] + (offset - SIXEL_FHEDT_DISTANCE_SLAB);
    }
#endif
    return grid->slab[0] + offset;
}

#endif
