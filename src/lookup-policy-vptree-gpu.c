/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <string.h>
#include "lookup-policy-vptree-gpu.h"
#include "threading.h"
#include "sixel_atomic.h"
/* The readable source is lookup-policy-vptree-gpu.metal. */
static char const * const vptree_chunks[] = {
#include <generated-metal/lookup-policy-vptree-gpu-shader.inc.h>
};

/* The policy owns its representation, cache key and completion protocol.
 * No other lookup can borrow this cache or depend on its table layout. */
typedef struct vptree_plan {
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
    sixel_gpu_lookup_build_pass_t passes[2];
} vptree_plan_t;

typedef struct vptree_binding {
    sixel_gpu_lookup_binding_interface_t iface;
    sixel_atomic_u32_t ref;
    vptree_plan_t *plan;
    int finished;
    int rebuild;
} vptree_binding_t;

typedef struct vptree_preparer {
    sixel_gpu_lookup_preparer_interface_t iface;
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    sixel_mutex_t gate;
    vptree_plan_t *cached;
} vptree_preparer_t;

static void
vptree_plan_ref(sixel_gpu_lookup_plan_interface_t *self)
{
    vptree_plan_t *plan;

    plan = (vptree_plan_t *)self;
    (void)sixel_atomic_fetch_add_u32(&plan->ref, 1);
}

static void
vptree_plan_unref(sixel_gpu_lookup_plan_interface_t *self)
{
    vptree_plan_t *plan;
    sixel_allocator_t *allocator;

    plan = (vptree_plan_t *)self;
    if (sixel_atomic_fetch_sub_u32(&plan->ref, 1) != 1) return;
    allocator = plan->allocator;
    if (plan->workspace != NULL) plan->workspace->vtbl->unref(plan->workspace);
    plan->device->vtbl->unref(plan->device);
    sixel_mutex_destroy(&plan->gate);
    sixel_allocator_free(allocator, plan);
    sixel_allocator_unref(allocator);
}

static SIXELSTATUS
vptree_describe(sixel_gpu_lookup_plan_interface_t const *self,
    sixel_gpu_lookup_program_view_t *view)
{
    vptree_plan_t const *plan;

    if (view == NULL) return SIXEL_BAD_ARGUMENT;
    plan = (vptree_plan_t const *)self;
    memset(view, 0, sizeof(*view));
    view->program_format = "metal.lookup-rgb8.v1";
    view->logical_name = "vptree";
    view->chunks = vptree_chunks;
    view->chunk_count = sizeof(vptree_chunks) / sizeof(vptree_chunks[0]);
    view->palette = plan->palette;
    view->palette_size = plan->count * 3U;
    view->ncolors = plan->count;
    view->workspace_size = 163840U * sizeof(uint32_t);
    view->build_passes = plan->passes;
    view->build_pass_count = 2;
    return SIXEL_OK;
}

static void
vptree_binding_ref(sixel_gpu_lookup_binding_interface_t *self)
{
    vptree_binding_t *binding;

    binding = (vptree_binding_t *)self;
    (void)sixel_atomic_fetch_add_u32(&binding->ref, 1);
}

static void
vptree_binding_unref(sixel_gpu_lookup_binding_interface_t *self)
{
    vptree_binding_t *binding;
    vptree_plan_t *plan;

    binding = (vptree_binding_t *)self;
    if (sixel_atomic_fetch_sub_u32(&binding->ref, 1) != 1) return;
    plan = binding->plan;
    /* An unfinished lease intentionally remains busy; unref cannot certify
     * GPU completion or allow another invocation to overwrite its storage. */
    sixel_allocator_free(plan->allocator, binding);
    vptree_plan_unref(&plan->iface);
}

static SIXELSTATUS
vptree_binding_view(sixel_gpu_lookup_binding_interface_t const *self,
    sixel_gpu_lookup_binding_view_t *view)
{
    vptree_binding_t const *binding;

    if (view == NULL) return SIXEL_BAD_ARGUMENT;
    binding = (vptree_binding_t const *)self;
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
vptree_finish(sixel_gpu_lookup_binding_interface_t *self, SIXELSTATUS outcome)
{
    vptree_binding_t *binding;
    vptree_plan_t *plan;

    binding = (vptree_binding_t *)self;
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

static sixel_gpu_lookup_binding_vtbl_t const vptree_binding_vtbl = {
    vptree_binding_ref, vptree_binding_unref, vptree_binding_view, vptree_finish
};

static SIXELSTATUS
vptree_begin(sixel_gpu_lookup_plan_interface_t *self,
    sixel_gpu_lookup_binding_interface_t **result)
{
    vptree_plan_t *plan;
    vptree_binding_t *binding;
    SIXELSTATUS status;

    if (result == NULL) return SIXEL_BAD_ARGUMENT;
    *result = NULL;
    plan = (vptree_plan_t *)self;
    sixel_mutex_lock(&plan->gate);
    if (plan->busy) {
        sixel_mutex_unlock(&plan->gate);
        return SIXEL_LOGIC_ERROR;
    }
    status = SIXEL_OK;
    if (plan->workspace == NULL) {
        status = plan->device->vtbl->create_buffer(plan->device,
            163840U * sizeof(uint32_t), &plan->workspace);
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
    binding->iface.vtbl = &vptree_binding_vtbl;
    binding->ref = 1U;
    binding->plan = plan;
    binding->rebuild = !plan->valid;
    vptree_plan_ref(self);
    plan->busy = 1;
    sixel_mutex_unlock(&plan->gate);
    *result = &binding->iface;
    return SIXEL_OK;
}

static sixel_gpu_lookup_plan_vtbl_t const vptree_plan_vtbl = {
    vptree_plan_ref, vptree_plan_unref, vptree_describe, vptree_begin
};

static void
vptree_ref(sixel_gpu_lookup_preparer_interface_t *self)
{
    vptree_preparer_t *preparer;

    preparer = (vptree_preparer_t *)self;
    (void)sixel_atomic_fetch_add_u32(&preparer->ref, 1);
}

static void
vptree_unref(sixel_gpu_lookup_preparer_interface_t *self)
{
    vptree_preparer_t *preparer;
    sixel_allocator_t *allocator;

    preparer = (vptree_preparer_t *)self;
    if (sixel_atomic_fetch_sub_u32(&preparer->ref, 1) != 1) return;
    allocator = preparer->allocator;
    if (preparer->cached != NULL) vptree_plan_unref(&preparer->cached->iface);
    sixel_mutex_destroy(&preparer->gate);
    sixel_allocator_free(allocator, preparer);
    sixel_allocator_unref(allocator);
}

static SIXELSTATUS
vptree_prepare(sixel_gpu_lookup_preparer_interface_t const *self,
    sixel_lookup_execution_request_t const *request,
    sixel_gpu_device_interface_t *device,
    sixel_gpu_lookup_plan_interface_t **result)
{
    vptree_preparer_t *preparer;
    vptree_plan_t *plan;
    sixel_palette_entries_view_t entries;
    unsigned int width;
    SIXELSTATUS status;

    if (request == NULL || device == NULL || result == NULL ||
            request->working_palette == NULL) return SIXEL_BAD_ARGUMENT;
    *result = NULL;
    if (request->query_kind != SIXEL_GPU_LOOKUP_QUERY_BYTE) {
        return SIXEL_FEATURE_ERROR;
    }
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
    preparer = (vptree_preparer_t *)self;
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
        plan->iface.vtbl = &vptree_plan_vtbl;
        plan->ref = 1U;
        plan->allocator = preparer->allocator;
        sixel_allocator_ref(plan->allocator);
        plan->device = device;
        device->vtbl->ref(device);
        plan->count = request->ncolors;
        memcpy(plan->palette, entries.entries, plan->count * 3U);
        width = 32;
        while (width < plan->count) width *= 2;
        plan->passes[0].entry_point = "vptree_build_rows";
        plan->passes[0].groups[0] = plan->count;
        plan->passes[0].threads[0] = width;
        plan->passes[1].entry_point = "vptree_build_seeds";
        plan->passes[1].groups[0] = 256;
        plan->passes[1].threads[0] = 128;
        plan->passes[0].groups[1] = plan->passes[0].groups[2] = 1;
        plan->passes[0].threads[1] = plan->passes[0].threads[2] = 1;
        plan->passes[1].groups[1] = plan->passes[1].groups[2] = 1;
        plan->passes[1].threads[1] = plan->passes[1].threads[2] = 1;
        if (preparer->cached != NULL) {
            vptree_plan_unref(&preparer->cached->iface);
        }
        preparer->cached = plan;
    }
    vptree_plan_ref(&plan->iface);
    *result = &plan->iface;
    sixel_mutex_unlock(&preparer->gate);
    return SIXEL_OK;
}

static sixel_gpu_lookup_preparer_vtbl_t const vptree_vtbl = {
    vptree_ref, vptree_unref, vptree_prepare
};

SIXELSTATUS
sixel_gpu_lookup_vptree_create(sixel_allocator_t *allocator,
    sixel_gpu_lookup_preparer_interface_t **result)
{
    vptree_preparer_t *preparer;
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
    preparer->iface.vtbl = &vptree_vtbl;
    preparer->ref = 1U;
    preparer->allocator = allocator;
    sixel_allocator_ref(allocator);
    *result = &preparer->iface;
    return SIXEL_OK;
}
