/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <string.h>
#include "lookup-policy-6bit-gpu.h"
#include "threading.h"
#include "gpu-lookup-metal.h"
#include "sixel_atomic.h"
#if defined(HAVE_METAL) || defined(HAVE_VULKAN)
#include <pthread.h>
#endif
/* Readable source: lookup-policy-6bit-fractional-gpu.metal. */
static char const * const lookup6_chunks[] = {
#include <generated-metal/lookup-policy-6bit-fractional-gpu-shader.inc.h>
};
#if defined(HAVE_VULKAN)
/* Readable source: lookup-policy-6bit-gpu.glsl. */
static char const * const lookup6_vulkan_chunks[] = {
#include <generated-vulkan/lookup-policy-6bit-gpu-shader.inc.h>
};
#endif

/* The policy owns its representation, cache key and completion protocol.
 * No other lookup can borrow this cache or depend on its table layout. */
typedef struct lookup6_plan {
    sixel_gpu_lookup_plan_interface_t iface;
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    sixel_gpu_device_interface_t *device;
    sixel_gpu_buffer_interface_t *workspace;
    sixel_mutex_t gate;
    unsigned char palette[768];
    unsigned int count;
    sixel_gpu_lookup_query_kind_t query_kind;
    sixel_lookup_policy_configuration_t configuration;
    gpu_metal_lookup_program_t metal;
    size_t workspace_size;
    /* Reserved program ABI words; grid geometry is fixed by this policy. */
    uint32_t constants[4];
    sixel_gpu_lookup_build_pass_t passes[6];
    size_t pass_count;
    int vulkan;
    int busy;
    int valid;
} lookup6_plan_t;

typedef struct lookup6_binding {
    sixel_gpu_lookup_binding_interface_t iface;
    sixel_atomic_u32_t ref;
    lookup6_plan_t *plan;
    int finished;
    int rebuild;
} lookup6_binding_t;

typedef struct lookup6_preparer {
    sixel_gpu_lookup_preparer_interface_t iface;
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    sixel_mutex_t gate;
    lookup6_plan_t *cached;
    sixel_lookup_policy_configuration_t configuration;
} lookup6_preparer_t;

/* Only 6bit preparers recycle through this process-wide idle cache. CPU
 * shared-instance settings never disable it. Active bindings retain exclusive
 * workspaces; neither another palette nor another in-flight frame can
 * overwrite them. One idle allocation bounds the retained cache capacity. */
#if defined(HAVE_METAL) || defined(HAVE_VULKAN)
static pthread_mutex_t lookup6_idle_gate = PTHREAD_MUTEX_INITIALIZER;
static struct {
    sixel_gpu_device_interface_t *device;
    sixel_gpu_buffer_interface_t *workspace;
    unsigned char palette[768];
    unsigned int count;
    int valid;
} lookup6_idle;

/* Metal byte tables belong to this policy's native implementation. */
static int
lookup6_owns_table(lookup6_plan_t const *plan)
{
    return plan->vulkan || plan->query_kind != SIXEL_GPU_LOOKUP_QUERY_BYTE;
}
#endif

static void
lookup6_borrow(lookup6_plan_t *plan)
{
#if defined(HAVE_METAL) || defined(HAVE_VULKAN)
    sixel_gpu_device_interface_t *device;

    if (!lookup6_owns_table(plan)) return;
    device = NULL;
    (void)pthread_mutex_lock(&lookup6_idle_gate);
    if (lookup6_idle.device == plan->device &&
            lookup6_idle.workspace->vtbl->size(lookup6_idle.workspace)
                >= plan->workspace_size) {
        plan->workspace = lookup6_idle.workspace;
        plan->valid = lookup6_idle.valid && lookup6_idle.count == plan->count &&
            memcmp(lookup6_idle.palette, plan->palette, plan->count*3U) == 0;
        device = lookup6_idle.device;
        lookup6_idle.device = NULL;
        lookup6_idle.workspace = NULL;
    }
    (void)pthread_mutex_unlock(&lookup6_idle_gate);
    if (device != NULL) device->vtbl->unref(device);
#else
    (void)plan;
#endif
}

static void
lookup6_recycle(lookup6_plan_t *plan)
{
#if defined(HAVE_METAL) || defined(HAVE_VULKAN)
    sixel_gpu_device_interface_t *device;
    sixel_gpu_buffer_interface_t *workspace;

    if (!lookup6_owns_table(plan) || plan->workspace == NULL ||
            plan->busy) return;
    plan->device->vtbl->ref(plan->device);
    (void)pthread_mutex_lock(&lookup6_idle_gate);
    device = lookup6_idle.device;
    workspace = lookup6_idle.workspace;
    lookup6_idle.device = plan->device;
    lookup6_idle.workspace = plan->workspace;
    lookup6_idle.count = plan->count;
    lookup6_idle.valid = plan->valid;
    memcpy(lookup6_idle.palette, plan->palette, plan->count*3U);
    plan->workspace = NULL;
    plan->valid = 0;
    (void)pthread_mutex_unlock(&lookup6_idle_gate);
    if (workspace != NULL) workspace->vtbl->unref(workspace);
    if (device != NULL) device->vtbl->unref(device);
#else
    (void)plan;
#endif
}

static void
lookup6_plan_ref(sixel_gpu_lookup_plan_interface_t *self)
{
    lookup6_plan_t *plan;

    plan = (lookup6_plan_t *)self;
    (void)sixel_atomic_fetch_add_u32(&plan->ref, 1);
}

static void
lookup6_plan_unref(sixel_gpu_lookup_plan_interface_t *self)
{
    lookup6_plan_t *plan;
    sixel_allocator_t *allocator;

    plan = (lookup6_plan_t *)self;
    if (sixel_atomic_fetch_sub_u32(&plan->ref, 1) != 1) return;
    allocator = plan->allocator;
    lookup6_recycle(plan);
    if (plan->workspace != NULL) plan->workspace->vtbl->unref(plan->workspace);
    plan->device->vtbl->unref(plan->device);
    sixel_mutex_destroy(&plan->gate);
    sixel_allocator_free(allocator, plan);
    sixel_allocator_unref(allocator);
}

static SIXELSTATUS
lookup6_describe(sixel_gpu_lookup_plan_interface_t const *self,
    sixel_gpu_lookup_program_view_t *view)
{
    lookup6_plan_t const *plan;

    if (view == NULL) return SIXEL_BAD_ARGUMENT;
    plan = (lookup6_plan_t const *)self;
    memset(view, 0, sizeof(*view));
#if defined(HAVE_VULKAN)
    if (plan->vulkan) {
        view->program_format = "vulkan.lookup-rgb8.v1";
        view->logical_name = "6bit";
        view->query_kind = plan->query_kind;
        view->chunks = lookup6_vulkan_chunks;
        view->chunk_count = sizeof(lookup6_vulkan_chunks) /
            sizeof(lookup6_vulkan_chunks[0]);
        view->palette = plan->palette;
        view->palette_size = plan->count * 3U;
        view->ncolors = plan->count;
        view->workspace_size = plan->workspace_size;
        view->constants = plan->constants;
        view->constants_size = sizeof(plan->constants);
        view->build_passes = plan->passes;
        view->build_pass_count = plan->pass_count;
        return SIXEL_OK;
    }
#endif
    /* Byte consumers receive this policy's native descriptor. Fractional
     * programs, builders and workspace are owned by this policy alone. */
    if (plan->query_kind == SIXEL_GPU_LOOKUP_QUERY_BYTE) {
        view->program_format = "metal.lookup-native-rgb8.v1";
        view->logical_name = "6bit";
        view->palette = plan->palette;
        view->palette_size = plan->count * 3U;
        view->ncolors = plan->count;
        view->constants = &plan->metal;
        view->constants_size = sizeof(plan->metal);
        view->workspace_size = 1;
        return SIXEL_OK;
    }
    view->program_format = "metal.lookup-rgb8.v1";
    view->logical_name = "6bit";
    view->query_kind = plan->query_kind;
    view->chunks = lookup6_chunks;
    view->chunk_count = sizeof(lookup6_chunks) / sizeof(lookup6_chunks[0]);
    view->palette = plan->palette;
    view->palette_size = plan->count * 3U;
    view->ncolors = plan->count;
    view->workspace_size = plan->workspace_size;
    view->constants = plan->constants;
    view->constants_size = sizeof(plan->constants);
    view->build_passes = plan->passes;
    view->build_pass_count = plan->pass_count;
    return SIXEL_OK;
}

static void
lookup6_binding_ref(sixel_gpu_lookup_binding_interface_t *self)
{
    lookup6_binding_t *binding;

    binding = (lookup6_binding_t *)self;
    (void)sixel_atomic_fetch_add_u32(&binding->ref, 1);
}

static void
lookup6_binding_unref(sixel_gpu_lookup_binding_interface_t *self)
{
    lookup6_binding_t *binding;
    lookup6_plan_t *plan;

    binding = (lookup6_binding_t *)self;
    if (sixel_atomic_fetch_sub_u32(&binding->ref, 1) != 1) return;
    plan = binding->plan;
    /* An unfinished lease intentionally remains busy; unref cannot certify
     * GPU completion or allow another invocation to overwrite its storage. */
    sixel_allocator_free(plan->allocator, binding);
    lookup6_plan_unref(&plan->iface);
}

static SIXELSTATUS
lookup6_binding_view(sixel_gpu_lookup_binding_interface_t const *self,
    sixel_gpu_lookup_binding_view_t *view)
{
    lookup6_binding_t const *binding;

    if (view == NULL) return SIXEL_BAD_ARGUMENT;
    binding = (lookup6_binding_t const *)self;
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
lookup6_finish(sixel_gpu_lookup_binding_interface_t *self, SIXELSTATUS outcome)
{
    lookup6_binding_t *binding;
    lookup6_plan_t *plan;

    binding = (lookup6_binding_t *)self;
    plan = binding->plan;
    sixel_mutex_lock(&plan->gate);
    if (binding->finished) {
        sixel_mutex_unlock(&plan->gate);
        return SIXEL_LOGIC_ERROR;
    }
    binding->finished = 1;
    if (binding->rebuild) plan->valid = outcome == SIXEL_OK;
    plan->busy = 0;
    /* A completed solve no longer needs the table. Return it while its
     * solution can still own the frozen plan; warm-up keeps capacity pinned. */
    if (outcome == SIXEL_OK) lookup6_recycle(plan);
    sixel_mutex_unlock(&plan->gate);
    return SIXEL_OK;
}

static sixel_gpu_lookup_binding_vtbl_t const lookup6_binding_vtbl = {
    lookup6_binding_ref, lookup6_binding_unref,
    lookup6_binding_view, lookup6_finish
};

static SIXELSTATUS
lookup6_begin(sixel_gpu_lookup_plan_interface_t *self,
    sixel_gpu_lookup_binding_interface_t **result)
{
    lookup6_plan_t *plan;
    lookup6_binding_t *binding;
    SIXELSTATUS status;

    if (result == NULL) return SIXEL_BAD_ARGUMENT;
    *result = NULL;
    plan = (lookup6_plan_t *)self;
    sixel_mutex_lock(&plan->gate);
    if (plan->busy) {
        sixel_mutex_unlock(&plan->gate);
        return SIXEL_LOGIC_ERROR;
    }
    status = SIXEL_OK;
    if (plan->workspace == NULL) lookup6_borrow(plan);
    if (plan->workspace == NULL) {
        status = plan->device->vtbl->create_buffer(plan->device,
            plan->workspace_size, &plan->workspace);
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
    binding->iface.vtbl = &lookup6_binding_vtbl;
    binding->ref = 1U;
    binding->plan = plan;
    binding->rebuild = !plan->valid;
    lookup6_plan_ref(self);
    plan->busy = 1;
    sixel_mutex_unlock(&plan->gate);
    *result = &binding->iface;
    return SIXEL_OK;
}

static sixel_gpu_lookup_plan_vtbl_t const lookup6_plan_vtbl = {
    lookup6_plan_ref, lookup6_plan_unref, lookup6_describe, lookup6_begin
};

static void
lookup6_ref(sixel_gpu_lookup_preparer_interface_t *self)
{
    lookup6_preparer_t *preparer;

    preparer = (lookup6_preparer_t *)self;
    (void)sixel_atomic_fetch_add_u32(&preparer->ref, 1);
}

static void
lookup6_unref(sixel_gpu_lookup_preparer_interface_t *self)
{
    lookup6_preparer_t *preparer;
    sixel_allocator_t *allocator;

    preparer = (lookup6_preparer_t *)self;
    if (sixel_atomic_fetch_sub_u32(&preparer->ref, 1) != 1) return;
    allocator = preparer->allocator;
    if (preparer->cached != NULL) lookup6_plan_unref(&preparer->cached->iface);
    sixel_mutex_destroy(&preparer->gate);
    sixel_allocator_free(allocator, preparer);
    sixel_allocator_unref(allocator);
}

static SIXELSTATUS
lookup6_prepare(sixel_gpu_lookup_preparer_interface_t const *self,
    sixel_lookup_execution_request_t const *request,
    sixel_gpu_device_interface_t *device,
    sixel_gpu_lookup_plan_interface_t **result)
{
    lookup6_preparer_t *preparer;
    lookup6_plan_t *plan;
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
    preparer = (lookup6_preparer_t *)self;
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
        plan->iface.vtbl = &lookup6_plan_vtbl;
        plan->ref = 1U;
        plan->allocator = preparer->allocator;
        sixel_allocator_ref(plan->allocator);
        plan->device = device;
        device->vtbl->ref(device);
        plan->count = request->ncolors;
        plan->query_kind = request->query_kind;
        plan->metal.configuration = &plan->configuration;
        plan->metal.implementation = sixel_lookup_6bit_gpu_metal();
        plan->configuration = preparer->configuration;
        /* 64 cells per channel; no other policy can resize this table. */
        plan->workspace_size = 262144U;
        plan->pass_count = 1;
        plan->passes[0].entry_point = "lookup_6bit_fractional_build";
        plan->passes[0].groups[0] = plan->workspace_size/128;
        plan->passes[0].threads[0] = 128;
        plan->passes[0].groups[1] = plan->passes[0].groups[2] = 1;
        plan->passes[0].threads[1] = plan->passes[0].threads[2] = 1;
        plan->vulkan = vulkan;
        if (vulkan) {
            /* lookup-policy-6bit-gpu.glsl: one 64-wide invocation per
             * cell for both query kinds. */
            plan->passes[0].entry_point = "lookup_6bit_build";
            plan->passes[0].groups[0] = plan->workspace_size / 64U;
            plan->passes[0].threads[0] = 64;
        } else if (plan->query_kind == SIXEL_GPU_LOOKUP_QUERY_BYTE) {
            plan->workspace_size = 1;
        }
        memcpy(plan->palette, entries.entries, plan->count * 3U);
        if (preparer->cached != NULL) {
            lookup6_plan_unref(&preparer->cached->iface);
        }
        preparer->cached = plan;
    }
    lookup6_plan_ref(&plan->iface);
    *result = &plan->iface;
    sixel_mutex_unlock(&preparer->gate);
    return SIXEL_OK;
}

static sixel_gpu_lookup_preparer_vtbl_t const lookup6_vtbl = {
    lookup6_ref, lookup6_unref, lookup6_prepare
};

SIXELSTATUS
sixel_gpu_lookup_6bit_create(sixel_allocator_t *allocator,
    sixel_lookup_policy_configuration_t const *configuration,
    sixel_gpu_lookup_preparer_interface_t **result)
{
    lookup6_preparer_t *preparer;
#if SIXEL_ENABLE_THREADS
    SIXELSTATUS status;
#endif

    if (allocator == NULL || configuration == NULL || result == NULL)
        return SIXEL_BAD_ARGUMENT;
    *result = NULL;
    if (configuration->kind != SIXEL_LUT_POLICY_6BIT)
        return SIXEL_BAD_ARGUMENT;
    preparer = sixel_allocator_calloc(allocator, 1, sizeof(*preparer));
    if (preparer == NULL) return SIXEL_BAD_ALLOCATION;
#if SIXEL_ENABLE_THREADS
    status = sixel_mutex_init(&preparer->gate);
    if (SIXEL_FAILED(status)) {
        sixel_allocator_free(allocator, preparer);
        return status;
    }
#endif
    preparer->iface.vtbl = &lookup6_vtbl;
    preparer->ref = 1U;
    preparer->allocator = allocator;
    preparer->configuration = *configuration;
    sixel_allocator_ref(allocator);
    *result = &preparer->iface;
    return SIXEL_OK;
}
