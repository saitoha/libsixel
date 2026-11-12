/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <string.h>
#include "lookup-policy-delaunay-gpu.h"
#include "threading.h"
#include "sixel_atomic.h"
/* Readable source: lookup-policy-delaunay-fractional-gpu.metal. */
static char const * const delaunay_chunks[] = {
#include <generated-metal/lookup-policy-delaunay-fractional-gpu-shader.inc.h>
};
#if defined(HAVE_VULKAN)
/* Readable source: lookup-policy-delaunay-gpu.glsl. */
static char const * const delaunay_vulkan_chunks[] = {
#include <generated-vulkan/lookup-policy-delaunay-gpu-shader.inc.h>
};
#endif

/* The policy owns its representation, cache key and completion protocol.
 * No other lookup can borrow this cache or depend on its table layout. */
typedef struct delaunay_plan {
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
    int vulkan;
    int busy;
    int valid;
    sixel_gpu_lookup_build_pass_t passes[3];
} delaunay_plan_t;

typedef struct delaunay_binding {
    sixel_gpu_lookup_binding_interface_t iface;
    sixel_atomic_u32_t ref;
    delaunay_plan_t *plan;
    int finished;
    int rebuild;
} delaunay_binding_t;

typedef struct delaunay_preparer {
    sixel_gpu_lookup_preparer_interface_t iface;
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    sixel_mutex_t gate;
    delaunay_plan_t *cached;
} delaunay_preparer_t;

static void
delaunay_plan_ref(sixel_gpu_lookup_plan_interface_t *self)
{
    delaunay_plan_t *plan;

    plan = (delaunay_plan_t *)self;
    (void)sixel_atomic_fetch_add_u32(&plan->ref, 1);
}

static void
delaunay_plan_unref(sixel_gpu_lookup_plan_interface_t *self)
{
    delaunay_plan_t *plan;
    sixel_allocator_t *allocator;

    plan = (delaunay_plan_t *)self;
    if (sixel_atomic_fetch_sub_u32(&plan->ref, 1) != 1) return;
    allocator = plan->allocator;
    if (plan->workspace != NULL) plan->workspace->vtbl->unref(plan->workspace);
    plan->device->vtbl->unref(plan->device);
    sixel_mutex_destroy(&plan->gate);
    sixel_allocator_free(allocator, plan);
    sixel_allocator_unref(allocator);
}

static SIXELSTATUS
delaunay_describe(sixel_gpu_lookup_plan_interface_t const *self,
    sixel_gpu_lookup_program_view_t *view)
{
    delaunay_plan_t const *plan;

    if (view == NULL) return SIXEL_BAD_ARGUMENT;
    plan = (delaunay_plan_t const *)self;
    memset(view, 0, sizeof(*view));
#if defined(HAVE_VULKAN)
    /* Both query kinds use this policy's GPU-built conservative graph. */
    if (plan->vulkan) {
        view->program_format = "vulkan.lookup-rgb8.v1";
        view->logical_name = "delaunay";
        view->query_kind = plan->query_kind;
        view->chunks = delaunay_vulkan_chunks;
        view->chunk_count = sizeof(delaunay_vulkan_chunks) /
            sizeof(delaunay_vulkan_chunks[0]);
        view->palette = plan->palette;
        view->palette_size = plan->count * 3U;
        view->ncolors = plan->count;
        view->workspace_size = (135424U + 2048U) * sizeof(uint32_t);
        view->build_passes = plan->passes;
        view->build_pass_count = 3;
        return SIXEL_OK;
    }
#endif
    /* Metal byte consumers keep the existing optimized executor. Fractional
     * programs, builders and workspace are owned by this policy alone. */
    if (plan->query_kind == SIXEL_GPU_LOOKUP_QUERY_BYTE) {
        view->program_format = "libsixel.legacy-palette.v1";
        view->logical_name = "delaunay";
        view->palette = plan->palette;
        view->palette_size = plan->count * 3U;
        view->ncolors = plan->count;
        view->constants = &plan->legacy;
        view->constants_size = sizeof(plan->legacy);
        view->workspace_size = 1;
        return SIXEL_OK;
    }
    view->program_format = "metal.lookup-rgb8.v1";
    view->logical_name = "delaunay";
    view->query_kind = plan->query_kind;
    view->chunks = delaunay_chunks;
    view->chunk_count = sizeof(delaunay_chunks) / sizeof(delaunay_chunks[0]);
    view->palette = plan->palette;
    view->palette_size = plan->count * 3U;
    view->ncolors = plan->count;
    view->workspace_size = (135424U + 2048U) * sizeof(uint32_t);
    view->build_passes = plan->passes;
    view->build_pass_count = 3;
    return SIXEL_OK;
}

static void
delaunay_binding_ref(sixel_gpu_lookup_binding_interface_t *self)
{
    delaunay_binding_t *binding;

    binding = (delaunay_binding_t *)self;
    (void)sixel_atomic_fetch_add_u32(&binding->ref, 1);
}

static void
delaunay_binding_unref(sixel_gpu_lookup_binding_interface_t *self)
{
    delaunay_binding_t *binding;
    delaunay_plan_t *plan;

    binding = (delaunay_binding_t *)self;
    if (sixel_atomic_fetch_sub_u32(&binding->ref, 1) != 1) return;
    plan = binding->plan;
    /* An unfinished lease intentionally remains busy; unref cannot certify
     * GPU completion or allow another invocation to overwrite its storage. */
    sixel_allocator_free(plan->allocator, binding);
    delaunay_plan_unref(&plan->iface);
}

static SIXELSTATUS
delaunay_binding_view(sixel_gpu_lookup_binding_interface_t const *self,
    sixel_gpu_lookup_binding_view_t *view)
{
    delaunay_binding_t const *binding;

    if (view == NULL) return SIXEL_BAD_ARGUMENT;
    binding = (delaunay_binding_t const *)self;
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
delaunay_finish(sixel_gpu_lookup_binding_interface_t *self, SIXELSTATUS outcome)
{
    delaunay_binding_t *binding;
    delaunay_plan_t *plan;

    binding = (delaunay_binding_t *)self;
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

static sixel_gpu_lookup_binding_vtbl_t const delaunay_binding_vtbl = {
    delaunay_binding_ref, delaunay_binding_unref,
    delaunay_binding_view, delaunay_finish
};

static SIXELSTATUS
delaunay_begin(sixel_gpu_lookup_plan_interface_t *self,
    sixel_gpu_lookup_binding_interface_t **result)
{
    delaunay_plan_t *plan;
    delaunay_binding_t *binding;
    SIXELSTATUS status;

    if (result == NULL) return SIXEL_BAD_ARGUMENT;
    *result = NULL;
    plan = (delaunay_plan_t *)self;
    sixel_mutex_lock(&plan->gate);
    if (plan->busy) {
        sixel_mutex_unlock(&plan->gate);
        return SIXEL_LOGIC_ERROR;
    }
    status = SIXEL_OK;
    if (plan->workspace == NULL) {
        status = plan->device->vtbl->create_buffer(plan->device,
            plan->query_kind == SIXEL_GPU_LOOKUP_QUERY_BYTE && !plan->vulkan
                ? 1 :
                (135424U + 2048U) * sizeof(uint32_t), &plan->workspace);
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
    binding->iface.vtbl = &delaunay_binding_vtbl;
    binding->ref = 1U;
    binding->plan = plan;
    binding->rebuild = !plan->valid;
    delaunay_plan_ref(self);
    plan->busy = 1;
    sixel_mutex_unlock(&plan->gate);
    *result = &binding->iface;
    return SIXEL_OK;
}

static sixel_gpu_lookup_plan_vtbl_t const delaunay_plan_vtbl = {
    delaunay_plan_ref, delaunay_plan_unref, delaunay_describe, delaunay_begin
};

static void
delaunay_ref(sixel_gpu_lookup_preparer_interface_t *self)
{
    delaunay_preparer_t *preparer;

    preparer = (delaunay_preparer_t *)self;
    (void)sixel_atomic_fetch_add_u32(&preparer->ref, 1);
}

static void
delaunay_unref(sixel_gpu_lookup_preparer_interface_t *self)
{
    delaunay_preparer_t *preparer;
    sixel_allocator_t *allocator;

    preparer = (delaunay_preparer_t *)self;
    if (sixel_atomic_fetch_sub_u32(&preparer->ref, 1) != 1) return;
    allocator = preparer->allocator;
    if (preparer->cached != NULL) delaunay_plan_unref(&preparer->cached->iface);
    sixel_mutex_destroy(&preparer->gate);
    sixel_allocator_free(allocator, preparer);
    sixel_allocator_unref(allocator);
}

static SIXELSTATUS
delaunay_prepare(sixel_gpu_lookup_preparer_interface_t const *self,
    sixel_lookup_execution_request_t const *request,
    sixel_gpu_device_interface_t *device,
    sixel_gpu_lookup_plan_interface_t **result)
{
    delaunay_preparer_t *preparer;
    delaunay_plan_t *plan;
    sixel_palette_entries_view_t entries;
    unsigned int width, pass;
    char const *format;
    int vulkan;
    SIXELSTATUS status;

    if (request == NULL || device == NULL || result == NULL ||
            request->working_palette == NULL) return SIXEL_BAD_ARGUMENT;
    *result = NULL;
    format = device->vtbl->program_format(device);
    vulkan = strcmp(format, "vulkan.lookup-rgb8.v1") == 0;
#if !defined(HAVE_VULKAN)
    vulkan = 0;
#endif
    if (request->pixelformat != SIXEL_PIXELFORMAT_RGB888 ||
            request->colorspace != SIXEL_COLORSPACE_GAMMA ||
            (!vulkan && strcmp(format, "metal.lookup-rgb8.v1") != 0)) {
        return SIXEL_FEATURE_ERROR;
    }
    if (request->query_kind != SIXEL_GPU_LOOKUP_QUERY_BYTE &&
            request->query_kind != SIXEL_GPU_LOOKUP_QUERY_FRACTIONAL) {
        return SIXEL_FEATURE_ERROR;
    }
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
    preparer = (delaunay_preparer_t *)self;
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
        plan->iface.vtbl = &delaunay_plan_vtbl;
        plan->ref = 1U;
        plan->allocator = preparer->allocator;
        sixel_allocator_ref(plan->allocator);
        plan->device = device;
        device->vtbl->ref(device);
        plan->count = request->ncolors;
        plan->query_kind = request->query_kind;
        plan->vulkan = vulkan;
        plan->legacy.kind = SIXEL_LUT_POLICY_DELAUNAY;
        plan->legacy.pixelformat = SIXEL_PIXELFORMAT_RGB888;
        memcpy(plan->palette, entries.entries, plan->count * 3U);
        width = 32;
        while (width < plan->count) width *= 2;
        plan->passes[0].entry_point = "delaunay_bounds";
        plan->passes[0].groups[0] = plan->count;
        plan->passes[0].threads[0] = 32;
        plan->passes[1].entry_point = "delaunay_sort";
        plan->passes[1].groups[0] = plan->count;
        plan->passes[1].threads[0] = width;
        plan->passes[2].entry_point = "delaunay_seeds";
        plan->passes[2].groups[0] = 32;
        plan->passes[2].threads[0] = 128;
        for (pass = 0; pass < 3; ++pass) {
            plan->passes[pass].groups[1] = plan->passes[pass].groups[2] = 1;
            plan->passes[pass].threads[1] = plan->passes[pass].threads[2] = 1;
        }
        if (preparer->cached != NULL) {
            delaunay_plan_unref(&preparer->cached->iface);
        }
        preparer->cached = plan;
    }
    delaunay_plan_ref(&plan->iface);
    *result = &plan->iface;
    sixel_mutex_unlock(&preparer->gate);
    return SIXEL_OK;
}

static sixel_gpu_lookup_preparer_vtbl_t const delaunay_vtbl = {
    delaunay_ref, delaunay_unref, delaunay_prepare
};

SIXELSTATUS
sixel_gpu_lookup_delaunay_create(sixel_allocator_t *allocator,
    sixel_gpu_lookup_preparer_interface_t **result)
{
    delaunay_preparer_t *preparer;
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
    preparer->iface.vtbl = &delaunay_vtbl;
    preparer->ref = 1U;
    preparer->allocator = allocator;
    sixel_allocator_ref(allocator);
    *result = &preparer->iface;
    return SIXEL_OK;
}
