/* SPDX-License-Identifier: MIT */
#include <stdio.h>
#include <stdlib.h>
#include "palette_stage2_common.h"

/* Injection is enabled only during synchronous private snapshot cloning. */
static int snapshot_alloc_track;
static int snapshot_alloc_target;
static int snapshot_alloc_calls;
static int snapshot_alloc_failed;

static int
snapshot_alloc_fail(void)
{
    if (!snapshot_alloc_track) {
        return 0;
    }
    ++snapshot_alloc_calls;
    if (snapshot_alloc_calls == snapshot_alloc_target) {
        snapshot_alloc_failed = 1;
        return 1;
    }
    return 0;
}

static void *
snapshot_alloc_malloc(size_t size)
{
    return snapshot_alloc_fail() ? NULL : malloc(size);
}

static void *
snapshot_alloc_calloc(size_t count, size_t size)
{
    return snapshot_alloc_fail() ? NULL : calloc(count, size);
}

static void *
snapshot_alloc_realloc(void *pointer, size_t size)
{
    return snapshot_alloc_fail() ? NULL : realloc(pointer, size);
}

int
test_snapshot_allocation_0072(int argc, char **argv)
{
    sixel_allocator_t *allocator;
    sixel_palette_snapshot_t *snapshot;
    sixel_dither_t *copy;
    SIXELSTATUS status;
    int floating;
    int allocation;
    int total;
    int failed;

    (void)argc;
    (void)argv;
    allocator = NULL;
    snapshot = NULL;
    copy = NULL;
    failed = 1;
    snapshot_alloc_target = 0;
    if (sixel_allocator_new(&allocator, snapshot_alloc_malloc,
            snapshot_alloc_calloc, snapshot_alloc_realloc, free)) {
        return 1;
    }
    for (floating = 0; floating <= 1; ++floating) {
        if (palette_stage2_snapshot(allocator, floating,
                SIXEL_LUT_POLICY_NONE, 0, &snapshot)) {
            goto end;
        }
        snapshot_alloc_calls = 0;
        snapshot_alloc_track = 1;
        if (sixel_palette_snapshot_clone(snapshot, &copy)) {
            goto end;
        }
        total = snapshot_alloc_calls;
        sixel_dither_unref(copy);
        copy = NULL;
        if (total == 0) {
            goto end;
        }
        for (allocation = 1; allocation <= total; ++allocation) {
            snapshot_alloc_calls = snapshot_alloc_failed = 0;
            snapshot_alloc_target = allocation;
            status = sixel_palette_snapshot_clone(snapshot, &copy);
            snapshot_alloc_target = 0;
            if (!snapshot_alloc_failed || status != SIXEL_BAD_ALLOCATION ||
                    copy != NULL) {
                fprintf(stderr, "snapshot clone allocation %d/%d: %x\n",
                    allocation, total, status);
                goto end;
            }
            /* The same retained snapshot remains usable after every fault. */
            if (sixel_palette_snapshot_clone(snapshot, &copy) ||
                    copy->ncolors != 3 || copy->keycolor != 2) {
                goto end;
            }
            sixel_dither_unref(copy);
            copy = NULL;
        }
        snapshot_alloc_track = 0;
        sixel_palette_snapshot_unref(snapshot);
        snapshot = NULL;
    }
    failed = 0;
end:
    snapshot_alloc_track = 0;
    snapshot_alloc_target = 0;
    sixel_dither_unref(copy);
    sixel_palette_snapshot_unref(snapshot);
    sixel_allocator_unref(allocator);
    return failed;
}
