/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <string.h>
#include "lookup-policy-gridlut-gpu.h"
#include "threading.h"
#include "gpu-lookup-metal.h"
#include "sixel_atomic.h"
/* Readable source: lookup-policy-gridlut-fractional-gpu.metal. */
static char const * const gridlut_chunks[] = {
#include <generated-metal/lookup-policy-gridlut-fractional-gpu-shader.inc.h>
};
#if defined(HAVE_VULKAN)
/* Readable source: lookup-policy-gridlut-gpu.glsl. */
static char const * const gridlut_vulkan_chunks[] = {
#include <generated-vulkan/lookup-policy-gridlut-gpu-shader.inc.h>
};
#endif

/* The policy owns its representation, cache key and completion protocol.
 * No other lookup can borrow this cache or depend on its table layout. */
typedef struct gridlut_plan {
    sixel_gpu_lookup_plan_interface_t iface;
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    sixel_gpu_device_interface_t *device;
    sixel_gpu_buffer_interface_t *workspace;
    sixel_mutex_t gate;
    unsigned char palette[768];
    unsigned int count;
    sixel_gpu_lookup_query_kind_t query_kind;
    uint32_t fractional;
    sixel_lookup_policy_configuration_t configuration;
    gpu_metal_lookup_program_t metal;
    sixel_gpu_lookup_build_pass_t pass;
    int vulkan;
    int busy;
    int valid;
} gridlut_plan_t;

typedef struct gridlut_binding {
    sixel_gpu_lookup_binding_interface_t iface;
    sixel_atomic_u32_t ref;
    gridlut_plan_t *plan;
    int finished;
    int rebuild;
} gridlut_binding_t;

typedef struct gridlut_preparer {
    sixel_gpu_lookup_preparer_interface_t iface;
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    sixel_mutex_t gate;
    gridlut_plan_t *cached;
} gridlut_preparer_t;

static void
gridlut_plan_ref(sixel_gpu_lookup_plan_interface_t *self)
{
    gridlut_plan_t *plan;

    plan = (gridlut_plan_t *)self;
    (void)sixel_atomic_fetch_add_u32(&plan->ref, 1);
}

static void
gridlut_plan_unref(sixel_gpu_lookup_plan_interface_t *self)
{
    gridlut_plan_t *plan;
    sixel_allocator_t *allocator;

    plan = (gridlut_plan_t *)self;
    if (sixel_atomic_fetch_sub_u32(&plan->ref, 1) != 1) return;
    allocator = plan->allocator;
    if (plan->workspace != NULL) plan->workspace->vtbl->unref(plan->workspace);
    plan->device->vtbl->unref(plan->device);
    sixel_mutex_destroy(&plan->gate);
    sixel_allocator_free(allocator, plan);
    sixel_allocator_unref(allocator);
}

static SIXELSTATUS
gridlut_describe(sixel_gpu_lookup_plan_interface_t const *self,
    sixel_gpu_lookup_program_view_t *view)
{
    gridlut_plan_t const *plan;

    if (view == NULL) return SIXEL_BAD_ARGUMENT;
    plan = (gridlut_plan_t const *)self;
    memset(view, 0, sizeof(*view));
#if defined(HAVE_VULKAN)
    /* Vulkan owns both queries. Its builder uses the constant to retain
     * the respective Metal byte and fractional cell bounds. */
    if (plan->vulkan) {
        view->program_format = "vulkan.lookup-rgb8.v1";
        view->logical_name = "gridlut";
        view->query_kind = plan->query_kind;
        view->chunks = gridlut_vulkan_chunks;
        view->chunk_count = sizeof(gridlut_vulkan_chunks) /
            sizeof(gridlut_vulkan_chunks[0]);
        view->palette = plan->palette;
        view->palette_size = plan->count * 3U;
        view->ncolors = plan->count;
        view->constants = &plan->fractional;
        view->constants_size = sizeof(plan->fractional);
        view->workspace_size = 266240;
        view->build_passes = &plan->pass;
        view->build_pass_count = 1;
        return SIXEL_OK;
    }
#endif
    /* Metal byte consumers keep the optimized executor. Fractional
     * programs, builders and workspace are owned by this policy alone. */
    if (plan->query_kind == SIXEL_GPU_LOOKUP_QUERY_BYTE) {
        view->program_format = "metal.lookup-native-rgb8.v1";
        view->logical_name = "gridlut";
        view->palette = plan->palette;
        view->palette_size = plan->count * 3U;
        view->ncolors = plan->count;
        view->constants = &plan->metal;
        view->constants_size = sizeof(plan->metal);
        view->workspace_size = 1;
        return SIXEL_OK;
    }
    view->program_format = "metal.lookup-rgb8.v1";
    view->logical_name = "gridlut";
    view->query_kind = plan->query_kind;
    view->chunks = gridlut_chunks;
    view->chunk_count = sizeof(gridlut_chunks) / sizeof(gridlut_chunks[0]);
    view->palette = plan->palette;
    view->palette_size = plan->count * 3U;
    view->ncolors = plan->count;
    view->workspace_size = 266240;
    view->build_passes = &plan->pass;
    view->build_pass_count = 1;
    return SIXEL_OK;
}

static void
gridlut_binding_ref(sixel_gpu_lookup_binding_interface_t *self)
{
    gridlut_binding_t *binding;

    binding = (gridlut_binding_t *)self;
    (void)sixel_atomic_fetch_add_u32(&binding->ref, 1);
}

static void
gridlut_binding_unref(sixel_gpu_lookup_binding_interface_t *self)
{
    gridlut_binding_t *binding;
    gridlut_plan_t *plan;

    binding = (gridlut_binding_t *)self;
    if (sixel_atomic_fetch_sub_u32(&binding->ref, 1) != 1) return;
    plan = binding->plan;
    /* An unfinished lease intentionally remains busy; unref cannot certify
     * GPU completion or allow another invocation to overwrite its storage. */
    sixel_allocator_free(plan->allocator, binding);
    gridlut_plan_unref(&plan->iface);
}

static SIXELSTATUS
gridlut_binding_view(sixel_gpu_lookup_binding_interface_t const *self,
    sixel_gpu_lookup_binding_view_t *view)
{
    gridlut_binding_t const *binding;

    if (view == NULL) return SIXEL_BAD_ARGUMENT;
    binding = (gridlut_binding_t const *)self;
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
gridlut_finish(sixel_gpu_lookup_binding_interface_t *self, SIXELSTATUS outcome)
{
    gridlut_binding_t *binding;
    gridlut_plan_t *plan;

    binding = (gridlut_binding_t *)self;
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

static sixel_gpu_lookup_binding_vtbl_t const gridlut_binding_vtbl = {
    gridlut_binding_ref, gridlut_binding_unref,
    gridlut_binding_view, gridlut_finish
};

static SIXELSTATUS
gridlut_begin(sixel_gpu_lookup_plan_interface_t *self,
    sixel_gpu_lookup_binding_interface_t **result)
{
    gridlut_plan_t *plan;
    gridlut_binding_t *binding;
    SIXELSTATUS status;

    if (result == NULL) return SIXEL_BAD_ARGUMENT;
    *result = NULL;
    plan = (gridlut_plan_t *)self;
    sixel_mutex_lock(&plan->gate);
    if (plan->busy) {
        sixel_mutex_unlock(&plan->gate);
        return SIXEL_LOGIC_ERROR;
    }
    status = SIXEL_OK;
    if (plan->workspace == NULL) {
        status = plan->device->vtbl->create_buffer(plan->device,
            plan->query_kind == SIXEL_GPU_LOOKUP_QUERY_BYTE &&
                !plan->vulkan ? 1 : 266240, &plan->workspace);
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
    binding->iface.vtbl = &gridlut_binding_vtbl;
    binding->ref = 1U;
    binding->plan = plan;
    binding->rebuild = !plan->valid;
    gridlut_plan_ref(self);
    plan->busy = 1;
    sixel_mutex_unlock(&plan->gate);
    *result = &binding->iface;
    return SIXEL_OK;
}

static sixel_gpu_lookup_plan_vtbl_t const gridlut_plan_vtbl = {
    gridlut_plan_ref, gridlut_plan_unref, gridlut_describe, gridlut_begin
};

static void
gridlut_ref(sixel_gpu_lookup_preparer_interface_t *self)
{
    gridlut_preparer_t *preparer;

    preparer = (gridlut_preparer_t *)self;
    (void)sixel_atomic_fetch_add_u32(&preparer->ref, 1);
}

static void
gridlut_unref(sixel_gpu_lookup_preparer_interface_t *self)
{
    gridlut_preparer_t *preparer;
    sixel_allocator_t *allocator;

    preparer = (gridlut_preparer_t *)self;
    if (sixel_atomic_fetch_sub_u32(&preparer->ref, 1) != 1) return;
    allocator = preparer->allocator;
    if (preparer->cached != NULL) gridlut_plan_unref(&preparer->cached->iface);
    sixel_mutex_destroy(&preparer->gate);
    sixel_allocator_free(allocator, preparer);
    sixel_allocator_unref(allocator);
}

static SIXELSTATUS
gridlut_prepare(sixel_gpu_lookup_preparer_interface_t const *self,
    sixel_lookup_execution_request_t const *request,
    sixel_gpu_device_interface_t *device,
    sixel_gpu_lookup_plan_interface_t **result)
{
    gridlut_preparer_t *preparer;
    gridlut_plan_t *plan;
    sixel_palette_entries_view_t entries;
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
    preparer = (gridlut_preparer_t *)self;
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
        plan->iface.vtbl = &gridlut_plan_vtbl;
        plan->ref = 1U;
        plan->allocator = preparer->allocator;
        sixel_allocator_ref(plan->allocator);
        plan->device = device;
        device->vtbl->ref(device);
        plan->count = request->ncolors;
        plan->query_kind = request->query_kind;
        plan->vulkan = vulkan;
        /* The Vulkan builder constant is a uint32 word, independent of
         * the C enum's storage. Metal keeps its existing builder ABI. */
        plan->fractional = request->query_kind ==
            SIXEL_GPU_LOOKUP_QUERY_FRACTIONAL;
        plan->metal.configuration = &plan->configuration;
        plan->metal.implementation = sixel_lookup_gridlut_gpu_metal();
        plan->configuration.kind = SIXEL_LUT_POLICY_GRIDLUT;
        plan->configuration.pixelformat = SIXEL_PIXELFORMAT_RGB888;
        memcpy(plan->palette, entries.entries, plan->count * 3U);
        plan->pass.entry_point = "gridlut_build";
        plan->pass.groups[0] = 128;
        plan->pass.threads[0] = 32;
        plan->pass.groups[1] = plan->pass.groups[2] = 1;
        plan->pass.threads[1] = plan->pass.threads[2] = 1;
        if (preparer->cached != NULL) {
            gridlut_plan_unref(&preparer->cached->iface);
        }
        preparer->cached = plan;
    }
    gridlut_plan_ref(&plan->iface);
    *result = &plan->iface;
    sixel_mutex_unlock(&preparer->gate);
    return SIXEL_OK;
}

static sixel_gpu_lookup_preparer_vtbl_t const gridlut_vtbl = {
    gridlut_ref, gridlut_unref, gridlut_prepare
};

SIXELSTATUS
sixel_gpu_lookup_gridlut_create(sixel_allocator_t *allocator,
    sixel_gpu_lookup_preparer_interface_t **result)
{
    gridlut_preparer_t *preparer;
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
    preparer->iface.vtbl = &gridlut_vtbl;
    preparer->ref = 1U;
    preparer->allocator = allocator;
    sixel_allocator_ref(allocator);
    *result = &preparer->iface;
    return SIXEL_OK;
}
