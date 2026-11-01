/* SPDX-License-Identifier: MIT */
/* Observe snapshot ownership and allocation, independently of process RSS. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include "src/dither-clamp.h"

static int clamp_alloc_track;
static int clamp_alloc_fail;
static int clamp_alloc_calls;
static int clamp_free_calls;
static size_t clamp_alloc_bytes;
static void *clamp_alloc_owned;

static void *
clamp_snapshot_malloc(size_t bytes)
{
    void *result;

    if (!clamp_alloc_track) {
        return malloc(bytes);
    }
    ++clamp_alloc_calls;
    clamp_alloc_bytes = bytes;
    result = clamp_alloc_fail ? NULL : malloc(bytes);
    clamp_alloc_owned = result;
    return result;
}

static void
clamp_snapshot_free(void *pointer)
{
    if (clamp_alloc_track && pointer != NULL &&
            pointer == clamp_alloc_owned) {
        ++clamp_free_calls;
    }
    free(pointer);
}

int
test_clamp_0044(int argc, char **argv)
{
    sixel_allocator_t *allocator;
    sixel_dither_clamp_t clamp;
    float source[18];
    float saved[18];
    int scenario;
    int policy;
    int n;
    int ok;
    SIXELSTATUS status;

    allocator = NULL;
    memset(&clamp, 0, sizeof(clamp));
    scenario = argc == 2 ? atoi(argv[1]) : -1;
    if (scenario < 0 || scenario > 3) {
        return EXIT_FAILURE;
    }
    policy = scenario == 1 ? SIXEL_DITHER_CLAMP_HARD :
        scenario == 2 ? SIXEL_DITHER_CLAMP_NONE : SIXEL_DITHER_CLAMP_SOFT;
    for (n = 0; n < 18; ++n) {
        source[n] = (float)(n - 3) / 8.f;
    }
    memcpy(saved, source, sizeof(source));
    clamp_alloc_track = 0;
    if (SIXEL_FAILED(sixel_allocator_new(&allocator,
            clamp_snapshot_malloc, NULL, NULL, clamp_snapshot_free))) {
        return EXIT_FAILURE;
    }
    clamp_alloc_calls = clamp_free_calls = 0;
    clamp_alloc_bytes = 0;
    clamp_alloc_owned = NULL;
    clamp_alloc_fail = scenario == 3;
    clamp_alloc_track = 1;
    status = sixel_dither_clamp_init(&clamp, source, 3, 2, 3,
        SIXEL_PIXELFORMAT_RGBFLOAT32, policy, 1.01, allocator);
    if (scenario == 2) {
        /* Unlimited feedback needs no source-dependent bound or copy. */
        ok = status == SIXEL_OK && clamp.source == NULL &&
            clamp_alloc_calls == 0;
    } else if (scenario == 3) {
        ok = status == SIXEL_BAD_ALLOCATION && clamp.source == NULL &&
            clamp_alloc_calls == 1 && clamp_alloc_bytes == sizeof(source);
    } else {
        /* Six RGB float32 pixels need exactly 72 bytes. This copy contract
         * requires bit identity without arithmetic tolerance. */
        ok = status == SIXEL_OK && clamp_alloc_calls == 1 &&
            clamp_alloc_bytes == sizeof(source) &&
            clamp.source != NULL && clamp.source != source &&
            memcmp(clamp.source, saved, sizeof(saved)) == 0;
        source[0] = 7.f;
        ok = ok && clamp.source != NULL &&
            memcmp(clamp.source, saved, sizeof(saved)) == 0;
    }
    sixel_dither_clamp_finish(&clamp);
    ok = ok && clamp.source == NULL &&
        clamp_free_calls == (scenario < 2 ? 1 : 0);
    /* Finish remains safe after failed initialization or an earlier finish. */
    sixel_dither_clamp_finish(&clamp);
    ok = ok && clamp_free_calls == (scenario < 2 ? 1 : 0);
    if (scenario == 3) {
        clamp_alloc_fail = 0;
        status = sixel_dither_clamp_init(&clamp, source, 3, 2, 3,
            SIXEL_PIXELFORMAT_RGBFLOAT32, policy, 1.01, allocator);
        ok = ok && status == SIXEL_OK && clamp.source != NULL &&
            clamp_alloc_calls == 2 &&
            memcmp(clamp.source, saved, sizeof(saved)) == 0;
        sixel_dither_clamp_finish(&clamp);
        ok = ok && clamp_free_calls == 1;
    }
    clamp_alloc_track = 0;
    sixel_allocator_unref(allocator);
    if (!ok) {
        fprintf(stderr, "snapshot scenario=%d allocs=%d bytes=%lu frees=%d\n",
            scenario, clamp_alloc_calls, (unsigned long)clamp_alloc_bytes,
            clamp_free_calls);
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
