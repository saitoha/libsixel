/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <string.h>
#include "gpu-lookup-rbc.h"
#include "threading.h"
#include "sixel_atomic.h"
#include "gpu-lookup-rbc-shader.inc.h"

/* Three GPU passes assign colors, pack cluster boxes, and build cell masks.
 * Their storage layout belongs to the private shader below.
 *
 * The policy owns its representation, cache key and completion protocol.
 * No other lookup can borrow this cache or depend on its table layout. */
typedef struct rbc_plan {
    sixel_gpu_lookup_plan_interface_t iface;
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    sixel_gpu_device_interface_t *device;
    sixel_gpu_buffer_interface_t *workspace;
    sixel_mutex_t gate;
    unsigned char palette[768];
    unsigned int count;
    int busy;
    int valid;
    sixel_gpu_lookup_build_pass_t passes[3];
} rbc_plan_t;

typedef struct rbc_binding {
    sixel_gpu_lookup_binding_interface_t iface;
    sixel_atomic_u32_t ref;
    rbc_plan_t *plan;
    int finished;
    int rebuild;
} rbc_binding_t;

typedef struct rbc_preparer {
    sixel_gpu_lookup_preparer_interface_t iface;
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    sixel_mutex_t gate;
    rbc_plan_t *cached;
} rbc_preparer_t;

static void
rbc_plan_ref(sixel_gpu_lookup_plan_interface_t *self)
{
    rbc_plan_t *plan;

    plan = (rbc_plan_t *)self;
    (void)sixel_atomic_fetch_add_u32(&plan->ref, 1);
}

static void
rbc_plan_unref(sixel_gpu_lookup_plan_interface_t *self)
{
    rbc_plan_t *plan;
    sixel_allocator_t *allocator;

    plan = (rbc_plan_t *)self;
    if (sixel_atomic_fetch_sub_u32(&plan->ref, 1) != 1) return;
    allocator = plan->allocator;
    if (plan->workspace != NULL) plan->workspace->vtbl->unref(plan->workspace);
    plan->device->vtbl->unref(plan->device);
    sixel_mutex_destroy(&plan->gate);
    sixel_allocator_free(allocator, plan);
    sixel_allocator_unref(allocator);
}

static SIXELSTATUS
rbc_describe(sixel_gpu_lookup_plan_interface_t const *self,
    sixel_gpu_lookup_program_view_t *view)
{
    rbc_plan_t const *plan;

    if (view == NULL) return SIXEL_BAD_ARGUMENT;
    plan = (rbc_plan_t const *)self;
    memset(view, 0, sizeof(*view));
    view->program_format = "metal.lookup-rgb8.v1";
    view->logical_name = "rbc";
    view->chunks = rbc_chunks;
    view->chunk_count = sizeof(rbc_chunks) / sizeof(rbc_chunks[0]);
    view->palette = plan->palette;
    view->palette_size = plan->count * 3U;
    view->ncolors = plan->count;
    view->workspace_size = 69936U * sizeof(uint32_t);
    view->build_passes = plan->passes;
    view->build_pass_count = 3;
    return SIXEL_OK;
}

static void
rbc_binding_ref(sixel_gpu_lookup_binding_interface_t *self)
{
    rbc_binding_t *binding;

    binding = (rbc_binding_t *)self;
    (void)sixel_atomic_fetch_add_u32(&binding->ref, 1);
}

static void
rbc_binding_unref(sixel_gpu_lookup_binding_interface_t *self)
{
    rbc_binding_t *binding;
    rbc_plan_t *plan;

    binding = (rbc_binding_t *)self;
    if (sixel_atomic_fetch_sub_u32(&binding->ref, 1) != 1) return;
    plan = binding->plan;
    /* An unfinished lease intentionally remains busy; unref cannot certify
     * GPU completion or allow another invocation to overwrite its storage. */
    sixel_allocator_free(plan->allocator, binding);
    rbc_plan_unref(&plan->iface);
}

static SIXELSTATUS
rbc_binding_view(sixel_gpu_lookup_binding_interface_t const *self,
    sixel_gpu_lookup_binding_view_t *view)
{
    rbc_binding_t const *binding;

    if (view == NULL) return SIXEL_BAD_ARGUMENT;
    binding = (rbc_binding_t const *)self;
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
rbc_finish(sixel_gpu_lookup_binding_interface_t *self, SIXELSTATUS outcome)
{
    rbc_binding_t *binding;
    rbc_plan_t *plan;

    binding = (rbc_binding_t *)self;
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

static sixel_gpu_lookup_binding_vtbl_t const rbc_binding_vtbl = {
    rbc_binding_ref, rbc_binding_unref, rbc_binding_view,
    rbc_finish
};

static SIXELSTATUS
rbc_begin(sixel_gpu_lookup_plan_interface_t *self,
    sixel_gpu_lookup_binding_interface_t **result)
{
    rbc_plan_t *plan;
    rbc_binding_t *binding;
    SIXELSTATUS status;

    if (result == NULL) return SIXEL_BAD_ARGUMENT;
    *result = NULL;
    plan = (rbc_plan_t *)self;
    sixel_mutex_lock(&plan->gate);
    if (plan->busy) {
        sixel_mutex_unlock(&plan->gate);
        return SIXEL_LOGIC_ERROR;
    }
    status = SIXEL_OK;
    if (plan->workspace == NULL) {
        status = plan->device->vtbl->create_buffer(plan->device,
            69936U * sizeof(uint32_t), &plan->workspace);
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
    binding->iface.vtbl = &rbc_binding_vtbl;
    binding->ref = 1U;
    binding->plan = plan;
    binding->rebuild = !plan->valid;
    rbc_plan_ref(self);
    plan->busy = 1;
    sixel_mutex_unlock(&plan->gate);
    *result = &binding->iface;
    return SIXEL_OK;
}

static sixel_gpu_lookup_plan_vtbl_t const rbc_plan_vtbl = {
    rbc_plan_ref, rbc_plan_unref, rbc_describe, rbc_begin
};

static void
rbc_ref(sixel_gpu_lookup_preparer_interface_t *self)
{
    rbc_preparer_t *preparer;

    preparer = (rbc_preparer_t *)self;
    (void)sixel_atomic_fetch_add_u32(&preparer->ref, 1);
}

static void
rbc_unref(sixel_gpu_lookup_preparer_interface_t *self)
{
    rbc_preparer_t *preparer;
    sixel_allocator_t *allocator;

    preparer = (rbc_preparer_t *)self;
    if (sixel_atomic_fetch_sub_u32(&preparer->ref, 1) != 1) return;
    allocator = preparer->allocator;
    if (preparer->cached != NULL) rbc_plan_unref(&preparer->cached->iface);
    sixel_mutex_destroy(&preparer->gate);
    sixel_allocator_free(allocator, preparer);
    sixel_allocator_unref(allocator);
}

static SIXELSTATUS
rbc_prepare(sixel_gpu_lookup_preparer_interface_t const *self,
    sixel_lookup_execution_request_t const *request,
    sixel_gpu_device_interface_t *device,
    sixel_gpu_lookup_plan_interface_t **result)
{
    rbc_preparer_t *preparer;
    rbc_plan_t *plan;
    sixel_palette_entries_view_t entries;
    SIXELSTATUS status;

    if (request == NULL || device == NULL || result == NULL ||
            request->working_palette == NULL) return SIXEL_BAD_ARGUMENT;
    *result = NULL;
    if (request->pixelformat != SIXEL_PIXELFORMAT_RGB888 ||
            request->colorspace != SIXEL_COLORSPACE_GAMMA ||
            strcmp(device->vtbl->program_format(device),
                "metal.lookup-rgb8.v1") != 0) return SIXEL_FEATURE_ERROR;
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
    preparer = (rbc_preparer_t *)self;
    sixel_mutex_lock(&preparer->gate);
    plan = preparer->cached;
    if (plan == NULL || plan->device != device ||
            plan->count != request->ncolors ||
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
        plan->iface.vtbl = &rbc_plan_vtbl;
        plan->ref = 1U;
        plan->allocator = preparer->allocator;
        sixel_allocator_ref(plan->allocator);
        plan->device = device;
        device->vtbl->ref(device);
        plan->count = request->ncolors;
        memcpy(plan->palette, entries.entries, plan->count * 3U);
        plan->passes[0].entry_point = "rbc_assign";
        plan->passes[0].groups[0] = (plan->count + 31U) / 32U;
        plan->passes[0].threads[0] = 32;
        plan->passes[0].groups[1] = plan->passes[0].groups[2] = 1;
        plan->passes[0].threads[1] = plan->passes[0].threads[2] = 1;
        plan->passes[1].entry_point = "rbc_pack_clusters";
        plan->passes[1].groups[0] = plan->count < 16 ? plan->count : 16;
        plan->passes[1].threads[0] = 32;
        plan->passes[1].groups[1] = plan->passes[1].groups[2] = 1;
        plan->passes[1].threads[1] = plan->passes[1].threads[2] = 1;
        plan->passes[2].entry_point = "rbc_seed";
        plan->passes[2].groups[0] = 256;
        plan->passes[2].threads[0] = 128;
        plan->passes[2].groups[1] = plan->passes[2].groups[2] = 1;
        plan->passes[2].threads[1] = plan->passes[2].threads[2] = 1;
        if (preparer->cached != NULL) {
            rbc_plan_unref(&preparer->cached->iface);
        }
        preparer->cached = plan;
    }
    rbc_plan_ref(&plan->iface);
    *result = &plan->iface;
    sixel_mutex_unlock(&preparer->gate);
    return SIXEL_OK;
}

static sixel_gpu_lookup_preparer_vtbl_t const rbc_vtbl = {
    rbc_ref, rbc_unref, rbc_prepare
};

SIXELSTATUS
sixel_gpu_lookup_rbc_create(sixel_allocator_t *allocator,
    sixel_gpu_lookup_preparer_interface_t **result)
{
    rbc_preparer_t *preparer;
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
    preparer->iface.vtbl = &rbc_vtbl;
    preparer->ref = 1U;
    preparer->allocator = allocator;
    sixel_allocator_ref(allocator);
    *result = &preparer->iface;
    return SIXEL_OK;
}
