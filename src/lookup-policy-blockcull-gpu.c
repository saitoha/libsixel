/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <string.h>
#include "lookup-policy-blockcull-gpu.h"
#include "threading.h"
#include "sixel_atomic.h"
/* Readable source: lookup-policy-blockcull-fractional-gpu.metal. */
/* Each generated chunk intentionally concatenates short C literals. */
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wstring-concatenation"
#endif
static char const * const blockcull_chunks[] = {
#include <generated-metal/lookup-policy-blockcull-fractional-gpu-shader.inc.h>
};
#if defined(__clang__)
#pragma clang diagnostic pop
#endif

/* The policy owns its representation, cache key and completion protocol.
 * No other lookup can borrow this cache or depend on its table layout. */
static sixel_gpu_lookup_batch_dispatch_t const blockcull_batch = {
    "blockcull_fractional", { 8, 8 }, { 32, 1, 1 }
};

typedef struct blockcull_plan {
    sixel_gpu_lookup_plan_interface_t iface;
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    sixel_gpu_device_interface_t *device;
    sixel_gpu_buffer_interface_t *workspace;
    sixel_mutex_t gate;
    unsigned char palette[768];
    unsigned int count;
    sixel_gpu_lookup_query_kind_t query_kind;
    sixel_lookup_policy_configuration_t legacy;
    int busy;
    int valid;
} blockcull_plan_t;

typedef struct blockcull_binding {
    sixel_gpu_lookup_binding_interface_t iface;
    sixel_atomic_u32_t ref;
    blockcull_plan_t *plan;
    int finished;
    int rebuild;
} blockcull_binding_t;

typedef struct blockcull_preparer {
    sixel_gpu_lookup_preparer_interface_t iface;
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    sixel_mutex_t gate;
    blockcull_plan_t *cached;
} blockcull_preparer_t;

static void
blockcull_plan_ref(sixel_gpu_lookup_plan_interface_t *self)
{
    blockcull_plan_t *plan;

    plan = (blockcull_plan_t *)self;
    (void)sixel_atomic_fetch_add_u32(&plan->ref, 1);
}

static void
blockcull_plan_unref(sixel_gpu_lookup_plan_interface_t *self)
{
    blockcull_plan_t *plan;
    sixel_allocator_t *allocator;

    plan = (blockcull_plan_t *)self;
    if (sixel_atomic_fetch_sub_u32(&plan->ref, 1) != 1) return;
    allocator = plan->allocator;
    if (plan->workspace != NULL) plan->workspace->vtbl->unref(plan->workspace);
    plan->device->vtbl->unref(plan->device);
    sixel_mutex_destroy(&plan->gate);
    sixel_allocator_free(allocator, plan);
    sixel_allocator_unref(allocator);
}

static SIXELSTATUS
blockcull_describe(sixel_gpu_lookup_plan_interface_t const *self,
    sixel_gpu_lookup_program_view_t *view)
{
    blockcull_plan_t const *plan;

    if (view == NULL) return SIXEL_BAD_ARGUMENT;
    plan = (blockcull_plan_t const *)self;
    memset(view, 0, sizeof(*view));
    /* Byte consumers keep the existing optimized executor. Fractional
     * programs, builders and workspace are owned by this policy alone. */
    if (plan->query_kind == SIXEL_GPU_LOOKUP_QUERY_BYTE) {
        view->program_format = "libsixel.legacy-palette.v1";
        view->logical_name = "blockcull";
        view->palette = plan->palette;
        view->palette_size = plan->count * 3U;
        view->ncolors = plan->count;
        view->constants = &plan->legacy;
        view->constants_size = sizeof(plan->legacy);
        view->workspace_size = 1;
        return SIXEL_OK;
    }
    view->program_format = "metal.lookup-rgb8.v1";
    view->logical_name = "blockcull";
    view->query_kind = plan->query_kind;
    view->chunks = blockcull_chunks;
    view->chunk_count = sizeof(blockcull_chunks) / sizeof(blockcull_chunks[0]);
    view->palette = plan->palette;
    view->palette_size = plan->count * 3U;
    view->ncolors = plan->count;
    view->workspace_size = 1;
    view->build_pass_count = 0;
    view->batch = &blockcull_batch;
    return SIXEL_OK;
}

static void
blockcull_binding_ref(sixel_gpu_lookup_binding_interface_t *self)
{
    blockcull_binding_t *binding;

    binding = (blockcull_binding_t *)self;
    (void)sixel_atomic_fetch_add_u32(&binding->ref, 1);
}

static void
blockcull_binding_unref(sixel_gpu_lookup_binding_interface_t *self)
{
    blockcull_binding_t *binding;
    blockcull_plan_t *plan;

    binding = (blockcull_binding_t *)self;
    if (sixel_atomic_fetch_sub_u32(&binding->ref, 1) != 1) return;
    plan = binding->plan;
    /* An unfinished lease intentionally remains busy; unref cannot certify
     * GPU completion or allow another invocation to overwrite its storage. */
    sixel_allocator_free(plan->allocator, binding);
    blockcull_plan_unref(&plan->iface);
}

static SIXELSTATUS
blockcull_binding_view(sixel_gpu_lookup_binding_interface_t const *self,
    sixel_gpu_lookup_binding_view_t *view)
{
    blockcull_binding_t const *binding;

    if (view == NULL) return SIXEL_BAD_ARGUMENT;
    binding = (blockcull_binding_t const *)self;
    sixel_mutex_lock(&binding->plan->gate);
    if (binding->finished) {
        sixel_mutex_unlock(&binding->plan->gate);
        return SIXEL_LOGIC_ERROR;
    }
    view->workspace = binding->plan->workspace;
    view->needs_build = binding->rebuild;
    sixel_mutex_unlock(&binding->plan->gate);
    return SIXEL_OK;
}

static SIXELSTATUS
blockcull_finish(sixel_gpu_lookup_binding_interface_t *self,
    SIXELSTATUS outcome)
{
    blockcull_binding_t *binding;
    blockcull_plan_t *plan;

    binding = (blockcull_binding_t *)self;
    plan = binding->plan;
    sixel_mutex_lock(&plan->gate);
    if (binding->finished) {
        sixel_mutex_unlock(&plan->gate);
        return SIXEL_LOGIC_ERROR;
    }
    binding->finished = 1;
    if (binding->rebuild) plan->valid = outcome == SIXEL_OK;
    plan->busy = 0;
    sixel_mutex_unlock(&plan->gate);
    return SIXEL_OK;
}

static sixel_gpu_lookup_binding_vtbl_t const blockcull_binding_vtbl = {
    blockcull_binding_ref, blockcull_binding_unref,
    blockcull_binding_view, blockcull_finish
};

static SIXELSTATUS
blockcull_begin(sixel_gpu_lookup_plan_interface_t *self,
    sixel_gpu_lookup_binding_interface_t **result)
{
    blockcull_plan_t *plan;
    blockcull_binding_t *binding;
    SIXELSTATUS status;

    if (result == NULL) return SIXEL_BAD_ARGUMENT;
    *result = NULL;
    plan = (blockcull_plan_t *)self;
    sixel_mutex_lock(&plan->gate);
    if (plan->busy) {
        sixel_mutex_unlock(&plan->gate);
        return SIXEL_LOGIC_ERROR;
    }
    status = SIXEL_OK;
    if (plan->workspace == NULL) {
        status = plan->device->vtbl->create_buffer(plan->device,
            1, &plan->workspace);
    }
    if (SIXEL_FAILED(status)) {
        sixel_mutex_unlock(&plan->gate);
        return status;
    }
    binding = sixel_allocator_calloc(plan->allocator, 1, sizeof(*binding));
    if (binding == NULL) {
        sixel_mutex_unlock(&plan->gate);
        return SIXEL_BAD_ALLOCATION;
    }
    binding->iface.vtbl = &blockcull_binding_vtbl;
    binding->ref = 1U;
    binding->plan = plan;
    binding->rebuild = !plan->valid;
    blockcull_plan_ref(self);
    plan->busy = 1;
    sixel_mutex_unlock(&plan->gate);
    *result = &binding->iface;
    return SIXEL_OK;
}

static sixel_gpu_lookup_plan_vtbl_t const blockcull_plan_vtbl = {
    blockcull_plan_ref, blockcull_plan_unref,
    blockcull_describe, blockcull_begin
};

static void
blockcull_ref(sixel_gpu_lookup_preparer_interface_t *self)
{
    blockcull_preparer_t *preparer;

    preparer = (blockcull_preparer_t *)self;
    (void)sixel_atomic_fetch_add_u32(&preparer->ref, 1);
}

static void
blockcull_unref(sixel_gpu_lookup_preparer_interface_t *self)
{
    blockcull_preparer_t *preparer;
    sixel_allocator_t *allocator;

    preparer = (blockcull_preparer_t *)self;
    if (sixel_atomic_fetch_sub_u32(&preparer->ref, 1) != 1) return;
    allocator = preparer->allocator;
    if (preparer->cached != NULL)
        blockcull_plan_unref(&preparer->cached->iface);
    sixel_mutex_destroy(&preparer->gate);
    sixel_allocator_free(allocator, preparer);
    sixel_allocator_unref(allocator);
}

static SIXELSTATUS
blockcull_prepare(sixel_gpu_lookup_preparer_interface_t const *self,
    sixel_lookup_execution_request_t const *request,
    sixel_gpu_device_interface_t *device,
    sixel_gpu_lookup_plan_interface_t **result)
{
    blockcull_preparer_t *preparer;
    blockcull_plan_t *plan;
    sixel_palette_entries_view_t entries;
    SIXELSTATUS status;

    if (request == NULL || device == NULL || result == NULL ||
            request->working_palette == NULL) return SIXEL_BAD_ARGUMENT;
    *result = NULL;
    if (request->pixelformat != SIXEL_PIXELFORMAT_RGB888 ||
            request->colorspace != SIXEL_COLORSPACE_GAMMA ||
            strcmp(device->vtbl->program_format(device),
                "metal.lookup-rgb8.v1") != 0) return SIXEL_FEATURE_ERROR;
    if (request->query_kind != SIXEL_GPU_LOOKUP_QUERY_BYTE &&
            request->query_kind != SIXEL_GPU_LOOKUP_QUERY_FRACTIONAL) {
        return SIXEL_FEATURE_ERROR;
    }
    if (request->query_kind == SIXEL_GPU_LOOKUP_QUERY_FRACTIONAL &&
            !request->allow_batch) return SIXEL_FEATURE_ERROR;
    if (request->ncolors < 1 || request->ncolors > 256) {
        return SIXEL_BAD_ARGUMENT;
    }
    status = request->working_palette->vtbl->get_entries(
        request->working_palette, &entries);
    if (SIXEL_FAILED(status)) return status;
    if (entries.depth != 3 || entries.entries == NULL ||
            entries.entry_count < request->ncolors ||
            entries.entries_size < request->ncolors * 3U) {
        return SIXEL_BAD_ARGUMENT;
    }
    preparer = (blockcull_preparer_t *)self;
    sixel_mutex_lock(&preparer->gate);
    plan = preparer->cached;
    if (plan == NULL || plan->device != device ||
            plan->count != request->ncolors ||
            plan->query_kind != request->query_kind ||
            memcmp(plan->palette, entries.entries, request->ncolors * 3U)) {
        plan = sixel_allocator_calloc(preparer->allocator, 1, sizeof(*plan));
        if (plan == NULL) {
            sixel_mutex_unlock(&preparer->gate);
            return SIXEL_BAD_ALLOCATION;
        }
#if SIXEL_ENABLE_THREADS
        status = sixel_mutex_init(&plan->gate);
        if (SIXEL_FAILED(status)) {
            sixel_allocator_free(preparer->allocator, plan);
            sixel_mutex_unlock(&preparer->gate);
            return status;
        }
#endif
        plan->iface.vtbl = &blockcull_plan_vtbl;
        plan->ref = 1U;
        plan->allocator = preparer->allocator;
        sixel_allocator_ref(plan->allocator);
        plan->device = device;
        device->vtbl->ref(device);
        plan->count = request->ncolors;
        plan->query_kind = request->query_kind;
        plan->legacy.kind = SIXEL_LUT_POLICY_BLOCKCULL;
        plan->legacy.pixelformat = SIXEL_PIXELFORMAT_RGB888;
        memcpy(plan->palette, entries.entries, plan->count * 3U);
        if (preparer->cached != NULL) {
            blockcull_plan_unref(&preparer->cached->iface);
        }
        preparer->cached = plan;
    }
    blockcull_plan_ref(&plan->iface);
    *result = &plan->iface;
    sixel_mutex_unlock(&preparer->gate);
    return SIXEL_OK;
}

static sixel_gpu_lookup_preparer_vtbl_t const blockcull_vtbl = {
    blockcull_ref, blockcull_unref, blockcull_prepare
};

SIXELSTATUS
sixel_gpu_lookup_blockcull_create(sixel_allocator_t *allocator,
    sixel_gpu_lookup_preparer_interface_t **result)
{
    blockcull_preparer_t *preparer;
#if SIXEL_ENABLE_THREADS
    SIXELSTATUS status;
#endif

    if (allocator == NULL || result == NULL) return SIXEL_BAD_ARGUMENT;
    *result = NULL;
    preparer = sixel_allocator_calloc(allocator, 1, sizeof(*preparer));
    if (preparer == NULL) return SIXEL_BAD_ALLOCATION;
#if SIXEL_ENABLE_THREADS
    status = sixel_mutex_init(&preparer->gate);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(allocator, preparer);
        return status;
    }
#endif
    preparer->iface.vtbl = &blockcull_vtbl;
    preparer->ref = 1U;
    preparer->allocator = allocator;
    sixel_allocator_ref(allocator);
    *result = &preparer->iface;
    return SIXEL_OK;
}
