/* SPDX-License-Identifier: MIT */
/* The GPU Heckbert solver returns a complete median-cut partition, and an
 * input outside its domain leaves every output untouched. */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sixel.h>
#include <6cells.h>
#include "src/palette-heckbert-gpu.h"
#if defined(HAVE_VULKAN) && !defined(HAVE_METAL)
#include "src/gpu-runtime-vulkan.h"
#include "src/palette-resident-vulkan.h"
#endif

enum {
    PARTITION_COUNT = 2000,
    PARTITION_COLORS = 64,
    VULKAN_LIMIT = 4096,
    UNTOUCHED = 0xdeadbeefu
};

typedef struct partition_case {
    double *tuples;
    unsigned int *weights;
    unsigned int *order;
    unsigned int *seen;
    unsigned int box_start[256];
    unsigned int box_size[256];
    unsigned int box_sum[256];
    double box_priority[256];
    unsigned int box_count;
} partition_case_t;

#if defined(HAVE_METAL) || defined(HAVE_VULKAN)
/* A fixed LCG keeps the histogram identical on every run. */
static void
partition_fill(partition_case_t *c, unsigned int count, unsigned int weight)
{
    unsigned int state;
    unsigned int i;
    unsigned int plane;

    state = 12345u;
    for (i = 0u; i < count; ++i) {
        for (plane = 0u; plane < 3u; ++plane) {
            state = state * 1103515245u + 12345u;
            c->tuples[i * 3u + plane] = (double)((state >> 16) & 0xffu);
        }
        c->weights[i] = weight != 0u ? weight : 1u + i % 16u;
        c->order[i] = UNTOUCHED;
    }
    c->box_count = UNTOUCHED;
}

static void
partition_request(partition_case_t *c, unsigned int count,
    double const *axis_weights, sixel_heckbert_solve_request_t *request)
{
    memset(request, 0, sizeof(*request));
    request->tuples = c->tuples;
    request->weights = c->weights;
    request->axis_weights = axis_weights;
    request->count = count;
    request->colors = PARTITION_COLORS;
    request->method_for_largest = SIXEL_LARGE_NORM;
    request->cut_sse = 1;
    request->order_sse = 1;
    request->order = c->order;
    request->box_start = c->box_start;
    request->box_size = c->box_size;
    request->box_sum = c->box_sum;
    request->box_priority = c->box_priority;
    request->box_count = &c->box_count;
}

/* Every entry appears once, boxes tile the order, and sums match. */
static int
partition_valid(partition_case_t *c, unsigned int count)
{
    unsigned int i;
    unsigned int box;
    unsigned int start;
    unsigned int sum;

    if (c->box_count != PARTITION_COLORS) return 0;
    memset(c->seen, 0, (size_t)count * sizeof(*c->seen));
    for (i = 0u; i < count; ++i) {
        if (c->order[i] >= count || c->seen[c->order[i]] != 0u) return 0;
        c->seen[c->order[i]] = 1u;
    }
    memset(c->seen, 0, (size_t)count * sizeof(*c->seen));
    for (box = 0u; box < c->box_count; ++box) {
        start = c->box_start[box];
        if (start >= count || c->box_size[box] == 0u ||
                c->box_size[box] > count - start || c->seen[start] != 0u ||
                !isfinite(c->box_priority[box]) ||
                c->box_priority[box] < 0.0)
            return 0;
        c->seen[start] = box + 1u;
        sum = 0u;
        for (i = start; i < start + c->box_size[box]; ++i)
            sum += c->weights[c->order[i]];
        if (sum != c->box_sum[box]) return 0;
    }
    start = 0u;
    for (box = 0u; box < c->box_count; ++box) {
        if (start >= count || c->seen[start] == 0u) return 0;
        start += c->box_size[c->seen[start] - 1u];
    }
    return start == count;
}

static int
partition_untouched(partition_case_t const *c, unsigned int count)
{
    unsigned int i;

    if (c->box_count != UNTOUCHED) return 0;
    for (i = 0u; i < count; ++i) {
        if (c->order[i] != UNTOUCHED) return 0;
    }
    return 1;
}

/* Without a usable device the solver declines before any output. */
static int
partition_device_ready(void)
{
#if defined(HAVE_VULKAN) && !defined(HAVE_METAL)
    sixel_vk_runtime_t *runtime;
    int ready;

    sixel_vk_lock();
    runtime = sixel_vk_runtime_get_locked();
    ready = runtime != NULL && sixel_vk_heckbert_threads(runtime) != 0u;
    sixel_vk_unlock();
    return ready;
#else
    return 1;
#endif
}
#endif

int
test_gpu_solver_partition(int argc, char **argv)
{
#if defined(HAVE_METAL) || defined(HAVE_VULKAN)
    static double const axis_weights[4] = { 1.0, 1.0, 1.0, 0.0 };
    sixel_allocator_t *allocator;
    sixel_heckbert_solver_interface_t *solver;
    sixel_heckbert_solve_request_t request;
    partition_case_t c;
    unsigned int capacity;
    SIXELSTATUS status;
    int answer;

    (void)argc;
    (void)argv;
    if (!partition_device_ready()) return 77;
    answer = 1;
    allocator = NULL;
    solver = NULL;
    memset(&c, 0, sizeof(c));
    capacity = VULKAN_LIMIT + 1u;
    c.tuples = malloc((size_t)capacity * 3u * sizeof(*c.tuples));
    c.weights = malloc((size_t)capacity * sizeof(*c.weights));
    c.order = malloc((size_t)capacity * sizeof(*c.order));
    c.seen = malloc((size_t)capacity * sizeof(*c.seen));
    if (c.tuples == NULL || c.weights == NULL || c.order == NULL ||
            c.seen == NULL)
        goto end;
    if (sixel_allocator_new(&allocator, NULL, NULL, NULL, NULL) != SIXEL_OK)
        goto end;
    if (sixel_palette_heckbert_gpu_new(allocator, (void **)&solver)
            != SIXEL_OK)
        goto end;

    partition_fill(&c, PARTITION_COUNT, 0u);
    partition_request(&c, PARTITION_COUNT, axis_weights, &request);
    status = solver->vtbl->solve(solver, &request);
#if defined(HAVE_METAL)
    if (status == SIXEL_FEATURE_ERROR && partition_untouched(&c,
            PARTITION_COUNT)) {
        answer = 77;
        goto end;
    }
#endif
    if (status != SIXEL_OK || !partition_valid(&c, PARTITION_COUNT))
        goto end;

    /* FP32 counts integers exactly only through 2^24. */
    partition_fill(&c, PARTITION_COUNT, 1u << 14);
    partition_request(&c, PARTITION_COUNT, axis_weights, &request);
    if (solver->vtbl->solve(solver, &request) != SIXEL_FEATURE_ERROR ||
            !partition_untouched(&c, PARTITION_COUNT))
        goto end;

#if defined(HAVE_VULKAN) && !defined(HAVE_METAL)
    /* The Vulkan bucket sort stops at 4096 histogram entries. */
    partition_fill(&c, VULKAN_LIMIT + 1u, 0u);
    partition_request(&c, VULKAN_LIMIT + 1u, axis_weights, &request);
    if (solver->vtbl->solve(solver, &request) != SIXEL_FEATURE_ERROR ||
            !partition_untouched(&c, VULKAN_LIMIT + 1u))
        goto end;
#endif
    answer = 0;
end:
    if (solver != NULL) solver->vtbl->unref(solver);
    sixel_allocator_unref(allocator);
    free(c.tuples);
    free(c.weights);
    free(c.order);
    free(c.seen);
    return answer;
#else
    (void)argc;
    (void)argv;
    return 77;
#endif
}
