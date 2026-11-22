/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <string.h>
#include "palette-heckbert-gpu.h"
#include "sixel_atomic.h"

typedef struct heckbert_gpu_solver {
    sixel_heckbert_solver_interface_t iface;
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
} heckbert_gpu_solver_t;

static void
heckbert_gpu_ref(sixel_heckbert_solver_interface_t *self)
{
    heckbert_gpu_solver_t *solver;

    solver = (heckbert_gpu_solver_t *)self;
    (void)sixel_atomic_fetch_add_u32(&solver->ref, 1);
}

static void
heckbert_gpu_unref(sixel_heckbert_solver_interface_t *self)
{
    heckbert_gpu_solver_t *solver;
    sixel_allocator_t *allocator;

    solver = (heckbert_gpu_solver_t *)self;
    if (sixel_atomic_fetch_sub_u32(&solver->ref, 1) != 1) return;
    allocator = solver->allocator;
    sixel_allocator_free(allocator, solver);
    sixel_allocator_unref(allocator);
}

/* The kernel implements the norm axis with SSE cuts and SSE box order on
 * three channels; any other configuration stays with the CPU solver. */
static SIXELSTATUS
heckbert_gpu_solve(sixel_heckbert_solver_interface_t *self,
                   sixel_heckbert_solve_request_t const *request)
{
    (void)self;
    if (request == NULL || request->tuples == NULL ||
            request->weights == NULL || request->axis_weights == NULL ||
            request->order == NULL || request->box_start == NULL ||
            request->box_size == NULL || request->box_sum == NULL ||
            request->box_priority == NULL || request->box_count == NULL) {
        return SIXEL_BAD_ARGUMENT;
    }
    if (request->method_for_largest != SIXEL_LARGE_NORM ||
            !request->cut_sse || !request->order_sse ||
            request->count < 2u || request->colors < 2u ||
            request->colors > 256u) {
        return SIXEL_FEATURE_ERROR;
    }
#if defined(HAVE_METAL)
    return sixel_palette_heckbert_gpu_metal_solve(request);
#elif defined(HAVE_VULKAN)
    return sixel_palette_heckbert_gpu_vulkan_solve(request);
#else
    return SIXEL_FEATURE_ERROR;
#endif
}

static sixel_heckbert_solver_vtbl_t const heckbert_gpu_vtbl = {
    heckbert_gpu_ref, heckbert_gpu_unref, heckbert_gpu_solve
};

/* Ownership escapes through the returned component interface. */
#if defined(HAVE_DIAGNOSTIC_WANALYZER_MALLOC_LEAK)
# pragma GCC diagnostic push
# pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
#endif
SIXELSTATUS
sixel_palette_heckbert_gpu_new(sixel_allocator_t *allocator, void **result)
{
    heckbert_gpu_solver_t *solver;

    if (allocator == NULL || result == NULL) return SIXEL_BAD_ARGUMENT;
    *result = NULL;
    solver = sixel_allocator_calloc(allocator, 1, sizeof(*solver));
    if (solver == NULL) return SIXEL_BAD_ALLOCATION;
    solver->iface.vtbl = &heckbert_gpu_vtbl;
    solver->ref = 1;
    solver->allocator = allocator;
    sixel_allocator_ref(allocator);
    *result = &solver->iface;
    return SIXEL_OK;
}
#if defined(HAVE_DIAGNOSTIC_WANALYZER_MALLOC_LEAK)
# pragma GCC diagnostic pop
#endif
