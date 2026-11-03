/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <stdlib.h>
#include <string.h>
#include "src/allocator.h"
#include "src/retained-plane-workers.h"
#include "src/threading.h"
#include "retained_workers_common.h"

#if SIXEL_ENABLE_THREADS
static int retained_workers_allocation_calls;

static void *
retained_workers_reject_malloc(size_t size)
{
    (void)size;
    ++retained_workers_allocation_calls;
    return NULL;
}

static void *
retained_workers_reject_calloc(size_t count, size_t size)
{
    (void)count;
    (void)size;
    ++retained_workers_allocation_calls;
    return NULL;
}

static void *
retained_workers_reject_realloc(void *ptr, size_t size)
{
    (void)ptr;
    (void)size;
    ++retained_workers_allocation_calls;
    return NULL;
}

static void
retained_workers_count_work(void *context, int worker, int begin)
{
    sixel_atomic_u32_t *counts;

    counts = context;
    if (worker) (void)sixel_atomic_fetch_add_u32(counts + !!begin, 1);
}
#endif

int
retained_workers_check(int participants,
    enum retained_workers_observation observation)
{
#if SIXEL_ENABLE_THREADS
    sixel_retained_workers_t *workers;
    sixel_allocator_t *allocator;
    sixel_retained_rows_t rows;
    sixel_retained_update_stats_t stats;
    sixel_wire_color_t palette[2];
    sixel_wire_palette_view_t view;
    sixel_threading_snapshot_t policy;
    sixel_thread_budget_scope_t scope;
    sixel_atomic_u32_t first_counts[2];
    sixel_atomic_u32_t second_counts[2];
    unsigned char indexes[17 * 5];
    unsigned char skip[17 * 5];
    unsigned char paint[17 * 5];
    unsigned char rgb[23 * 7 * 3];
    unsigned char valid[23 * 7];
    unsigned char expected_rgb[23 * 7 * 3];
    unsigned char expected_valid[23 * 7];
    unsigned char expected_paint[17 * 5];
    size_t i;
    size_t target;
    int ok;

    workers = NULL;
    allocator = NULL;
    ok = 0;
    memset(&rows, 0, sizeof(rows));
    memset(&stats, 0, sizeof(stats));
    memset(&view, 0, sizeof(view));
    memset(palette, 0, sizeof(palette));
    memset(paint, 0, sizeof(paint));
    memset(expected_paint, 0, sizeof(expected_paint));
    memset(first_counts, 0, sizeof(first_counts));
    memset(second_counts, 0, sizeof(second_counts));
    memset(rgb, 97, sizeof(rgb));
    memset(expected_rgb, 97, sizeof(expected_rgb));
    memset(valid, 0, sizeof(valid));
    memset(expected_valid, 0, sizeof(expected_valid));
    palette[0].emitted_rgb[0] = 255;
    palette[1].emitted_rgb[1] = 255;
    for (i = 0; i < sizeof(indexes); ++i) {
        indexes[i] = (unsigned char)(i % 2);
        skip[i] = i % 3 == 0 ? 0 : (i % 3 == 1 ? 1 : 3);
        if (skip[i] != 0) continue;
        target = (i / 17 + 1) * 23 + i % 17 + 3;
        expected_rgb[target * 3] = i % 2 == 0 ? 255 : 0;
        expected_rgb[target * 3 + 1] = i % 2 == 1 ? 255 : 0;
        expected_rgb[target * 3 + 2] = 0;
        expected_valid[target] = 1;
        expected_paint[i] = 1;
    }
    if (sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL) != SIXEL_OK) {
        goto end;
    }
    workers = sixel_retained_workers_new(allocator, participants);
    if (workers == NULL) goto end;
    view.colors = palette;
    view.count = 2;
    rows.indexes = indexes;
    rows.skip = skip;
    rows.paint = paint;
    rows.rgb = rgb + (23 + 3) * 3;
    rows.valid = valid + 23 + 3;
    rows.width = 17;
    rows.stride = 23;
    sixel_retained_rows_prepare(&rows, &view);
    rows.neon = 0;
    if (observation == RETAINED_WORKERS_NO_ALLOCATION) {
        retained_workers_allocation_calls = 0;
        allocator->fn_malloc = retained_workers_reject_malloc;
        allocator->fn_calloc = retained_workers_reject_calloc;
        allocator->fn_realloc = retained_workers_reject_realloc;
    }
    (void)sixel_threading_policy_capture(&policy, 0);
    policy.work = retained_workers_count_work;
    policy.work_context = first_counts;
    sixel_threading_policy_enter(&scope, &policy);
    sixel_retained_workers_run(workers, &rows, 5, participants, &stats);
    sixel_thread_budget_leave(&scope);
    if (stats.worker_threads != participants) goto end;
    if (observation == RETAINED_WORKERS_POLICY ||
            observation == RETAINED_WORKERS_REUSE_SMALLER) {
        memset(&stats, 0, sizeof(stats));
        policy.work_context = second_counts;
        sixel_threading_policy_enter(&scope, &policy);
        sixel_retained_workers_run(workers, &rows, 5,
            observation == RETAINED_WORKERS_POLICY ? participants : 2,
            &stats);
        sixel_thread_budget_leave(&scope);
    }
    switch (observation) {
    case RETAINED_WORKERS_RGB:
        ok = memcmp(rgb, expected_rgb, sizeof(rgb)) == 0;
        break;
    case RETAINED_WORKERS_VALID:
        ok = memcmp(valid, expected_valid, sizeof(valid)) == 0;
        break;
    case RETAINED_WORKERS_PAINT:
        ok = memcmp(paint, expected_paint, sizeof(paint)) == 0;
        break;
    case RETAINED_WORKERS_COUNT:
        ok = stats.painted_pixels == 29;
        break;
    case RETAINED_WORKERS_REFINEMENT:
        ok = stats.refinement_needed == 1;
        break;
    case RETAINED_WORKERS_POLICY:
        ok = sixel_atomic_fetch_add_u32(&first_counts[1], 0) ==
                (unsigned int)(participants - 1) &&
            sixel_atomic_fetch_add_u32(&second_counts[1], 0) ==
                (unsigned int)(participants - 1);
        break;
    case RETAINED_WORKERS_BALANCED:
        ok = sixel_atomic_fetch_add_u32(&first_counts[0], 0) ==
                (unsigned int)(participants - 1) &&
            sixel_atomic_fetch_add_u32(&first_counts[1], 0) ==
                (unsigned int)(participants - 1);
        break;
    case RETAINED_WORKERS_NO_ALLOCATION:
        ok = retained_workers_allocation_calls == 0;
        break;
    case RETAINED_WORKERS_REUSE_SMALLER:
        ok = stats.worker_threads == 2 &&
            sixel_atomic_fetch_add_u32(&second_counts[1], 0) == 1;
        break;
    }
end:
    sixel_retained_workers_free(workers);
    sixel_allocator_unref(allocator);
    return ok ? 0 : 1;
#else
    (void)participants;
    (void)observation;
    return 77;
#endif
}
