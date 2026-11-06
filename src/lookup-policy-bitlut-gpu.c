/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <string.h>
#include "lookup-policy-bitlut-gpu.h"
#include "threading.h"
#include "sixel_atomic.h"
#if defined(HAVE_METAL)
#include <pthread.h>
#endif
/* Readable source: lookup-policy-bitlut-fractional-gpu.metal. */
static char const * const bitlut_chunks[] = {
#include <generated-metal/lookup-policy-bitlut-fractional-gpu-shader.inc.h>
};

/* The policy owns its representation, cache key and completion protocol.
 * No other lookup can borrow this cache or depend on its table layout. */
typedef struct bitlut_plan {
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
    size_t workspace_size;
    uint32_t constants[4];
    sixel_gpu_lookup_build_pass_t passes[6];
    size_t pass_count;
    int busy;
    int valid;
} bitlut_plan_t;

typedef struct bitlut_binding {
    sixel_gpu_lookup_binding_interface_t iface;
    sixel_atomic_u32_t ref;
    bitlut_plan_t *plan;
    int finished;
    int rebuild;
} bitlut_binding_t;

typedef struct bitlut_preparer {
    sixel_gpu_lookup_preparer_interface_t iface;
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    sixel_mutex_t gate;
    bitlut_plan_t *cached;
    sixel_lookup_policy_configuration_t configuration;
} bitlut_preparer_t;

/* This policy always recycles through its process-wide idle cache. CPU
 * shared-instance settings never disable it. Active bindings retain exclusive
 * workspaces; neither another palette nor another in-flight frame can
 * overwrite them. One idle allocation bounds the retained cache capacity. */
#if defined(HAVE_METAL)
static pthread_mutex_t bitlut_idle_gate = PTHREAD_MUTEX_INITIALIZER;
static struct {
    sixel_gpu_device_interface_t *device;
    sixel_gpu_buffer_interface_t *workspace;
    unsigned char palette[768];
    unsigned int count;
    uint32_t constants[4];
    int valid;
} bitlut_idle;
#endif

static void
bitlut_borrow(bitlut_plan_t *plan)
{
#if defined(HAVE_METAL)
    sixel_gpu_device_interface_t *device;

    if (plan->query_kind == SIXEL_GPU_LOOKUP_QUERY_BYTE) return;
    device = NULL;
    (void)pthread_mutex_lock(&bitlut_idle_gate);
    if (bitlut_idle.device == plan->device &&
            bitlut_idle.workspace->vtbl->size(bitlut_idle.workspace)
                >= plan->workspace_size) {
        plan->workspace = bitlut_idle.workspace;
        plan->valid = bitlut_idle.valid && bitlut_idle.count == plan->count &&
            memcmp(bitlut_idle.constants, plan->constants,
                sizeof(plan->constants)) == 0 &&
            memcmp(bitlut_idle.palette, plan->palette, plan->count*3U) == 0;
        device = bitlut_idle.device;
        bitlut_idle.device = NULL;
        bitlut_idle.workspace = NULL;
    }
    (void)pthread_mutex_unlock(&bitlut_idle_gate);
    if (device != NULL) device->vtbl->unref(device);
#else
    (void)plan;
#endif
}

static void
bitlut_recycle(bitlut_plan_t *plan)
{
#if defined(HAVE_METAL)
    sixel_gpu_device_interface_t *device;
    sixel_gpu_buffer_interface_t *workspace;

    if (plan->query_kind == SIXEL_GPU_LOOKUP_QUERY_BYTE ||
            plan->workspace == NULL || plan->busy) return;
    plan->device->vtbl->ref(plan->device);
    (void)pthread_mutex_lock(&bitlut_idle_gate);
    device = bitlut_idle.device;
    workspace = bitlut_idle.workspace;
    bitlut_idle.device = plan->device;
    bitlut_idle.workspace = plan->workspace;
    bitlut_idle.count = plan->count;
    bitlut_idle.valid = plan->valid;
    memcpy(bitlut_idle.constants, plan->constants, sizeof(plan->constants));
    memcpy(bitlut_idle.palette, plan->palette, plan->count*3U);
    plan->workspace = NULL;
    plan->valid = 0;
    (void)pthread_mutex_unlock(&bitlut_idle_gate);
    if (workspace != NULL) workspace->vtbl->unref(workspace);
    if (device != NULL) device->vtbl->unref(device);
#else
    (void)plan;
#endif
}

static void
bitlut_plan_ref(sixel_gpu_lookup_plan_interface_t *self)
{
    bitlut_plan_t *plan;

    plan = (bitlut_plan_t *)self;
    (void)sixel_atomic_fetch_add_u32(&plan->ref, 1);
}

static void
bitlut_plan_unref(sixel_gpu_lookup_plan_interface_t *self)
{
    bitlut_plan_t *plan;
    sixel_allocator_t *allocator;

    plan = (bitlut_plan_t *)self;
    if (sixel_atomic_fetch_sub_u32(&plan->ref, 1) != 1) return;
    allocator = plan->allocator;
    bitlut_recycle(plan);
    if (plan->workspace != NULL) plan->workspace->vtbl->unref(plan->workspace);
    plan->device->vtbl->unref(plan->device);
    sixel_mutex_destroy(&plan->gate);
    sixel_allocator_free(allocator, plan);
    sixel_allocator_unref(allocator);
}

static SIXELSTATUS
bitlut_describe(sixel_gpu_lookup_plan_interface_t const *self,
    sixel_gpu_lookup_program_view_t *view)
{
    bitlut_plan_t const *plan;

    if (view == NULL) return SIXEL_BAD_ARGUMENT;
    plan = (bitlut_plan_t const *)self;
    memset(view, 0, sizeof(*view));
    /* Byte consumers keep the existing optimized executor. Fractional
     * programs, builders and workspace are owned by this policy alone. */
    if (plan->query_kind == SIXEL_GPU_LOOKUP_QUERY_BYTE) {
        view->program_format = "libsixel.legacy-palette.v1";
        view->logical_name = "bitlut";
        view->palette = plan->palette;
        view->palette_size = plan->count * 3U;
        view->ncolors = plan->count;
        view->constants = &plan->legacy;
        view->constants_size = sizeof(plan->legacy);
        view->workspace_size = 1;
        return SIXEL_OK;
    }
    view->program_format = "metal.lookup-rgb8.v1";
    view->logical_name = "bitlut";
    view->query_kind = plan->query_kind;
    view->chunks = bitlut_chunks;
    view->chunk_count = sizeof(bitlut_chunks) / sizeof(bitlut_chunks[0]);
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
bitlut_binding_ref(sixel_gpu_lookup_binding_interface_t *self)
{
    bitlut_binding_t *binding;

    binding = (bitlut_binding_t *)self;
    (void)sixel_atomic_fetch_add_u32(&binding->ref, 1);
}

static void
bitlut_binding_unref(sixel_gpu_lookup_binding_interface_t *self)
{
    bitlut_binding_t *binding;
    bitlut_plan_t *plan;

    binding = (bitlut_binding_t *)self;
    if (sixel_atomic_fetch_sub_u32(&binding->ref, 1) != 1) return;
    plan = binding->plan;
    /* An unfinished lease intentionally remains busy; unref cannot certify
     * GPU completion or allow another invocation to overwrite its storage. */
    sixel_allocator_free(plan->allocator, binding);
    bitlut_plan_unref(&plan->iface);
}

static SIXELSTATUS
bitlut_binding_view(sixel_gpu_lookup_binding_interface_t const *self,
    sixel_gpu_lookup_binding_view_t *view)
{
    bitlut_binding_t const *binding;

    if (view == NULL) return SIXEL_BAD_ARGUMENT;
    binding = (bitlut_binding_t const *)self;
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
bitlut_finish(sixel_gpu_lookup_binding_interface_t *self, SIXELSTATUS outcome)
{
    bitlut_binding_t *binding;
    bitlut_plan_t *plan;

    binding = (bitlut_binding_t *)self;
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
    if (outcome == SIXEL_OK) bitlut_recycle(plan);
    sixel_mutex_unlock(&plan->gate);
    return SIXEL_OK;
}

static sixel_gpu_lookup_binding_vtbl_t const bitlut_binding_vtbl = {
    bitlut_binding_ref, bitlut_binding_unref,
    bitlut_binding_view, bitlut_finish
};

static SIXELSTATUS
bitlut_begin(sixel_gpu_lookup_plan_interface_t *self,
    sixel_gpu_lookup_binding_interface_t **result)
{
    bitlut_plan_t *plan;
    bitlut_binding_t *binding;
    SIXELSTATUS status;

    if (result == NULL) return SIXEL_BAD_ARGUMENT;
    *result = NULL;
    plan = (bitlut_plan_t *)self;
    sixel_mutex_lock(&plan->gate);
    if (plan->busy) {
        sixel_mutex_unlock(&plan->gate);
        return SIXEL_LOGIC_ERROR;
    }
    status = SIXEL_OK;
    if (plan->workspace == NULL) bitlut_borrow(plan);
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
    binding->iface.vtbl = &bitlut_binding_vtbl;
    binding->ref = 1U;
    binding->plan = plan;
    binding->rebuild = !plan->valid;
    bitlut_plan_ref(self);
    plan->busy = 1;
    sixel_mutex_unlock(&plan->gate);
    *result = &binding->iface;
    return SIXEL_OK;
}

static sixel_gpu_lookup_plan_vtbl_t const bitlut_plan_vtbl = {
    bitlut_plan_ref, bitlut_plan_unref, bitlut_describe, bitlut_begin
};

static void
bitlut_ref(sixel_gpu_lookup_preparer_interface_t *self)
{
    bitlut_preparer_t *preparer;

    preparer = (bitlut_preparer_t *)self;
    (void)sixel_atomic_fetch_add_u32(&preparer->ref, 1);
}

static void
bitlut_unref(sixel_gpu_lookup_preparer_interface_t *self)
{
    bitlut_preparer_t *preparer;
    sixel_allocator_t *allocator;

    preparer = (bitlut_preparer_t *)self;
    if (sixel_atomic_fetch_sub_u32(&preparer->ref, 1) != 1) return;
    allocator = preparer->allocator;
    if (preparer->cached != NULL) bitlut_plan_unref(&preparer->cached->iface);
    sixel_mutex_destroy(&preparer->gate);
    sixel_allocator_free(allocator, preparer);
    sixel_allocator_unref(allocator);
}

static SIXELSTATUS
bitlut_prepare(sixel_gpu_lookup_preparer_interface_t const *self,
    sixel_lookup_execution_request_t const *request,
    sixel_gpu_device_interface_t *device,
    sixel_gpu_lookup_plan_interface_t **result)
{
    bitlut_preparer_t *preparer;
    bitlut_plan_t *plan;
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
    preparer = (bitlut_preparer_t *)self;
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
        plan->iface.vtbl = &bitlut_plan_vtbl;
        plan->ref = 1U;
        plan->allocator = preparer->allocator;
        sixel_allocator_ref(plan->allocator);
        plan->device = device;
        device->vtbl->ref(device);
        plan->count = request->ncolors;
        plan->query_kind = request->query_kind;
        plan->legacy = preparer->configuration;
        plan->constants[0] = plan->legacy.kind == SIXEL_LUT_POLICY_5BIT
            ? 5 : 6;
        plan->workspace_size = (size_t)1 << (plan->constants[0]*3);
        plan->pass_count = 1;
        plan->passes[0].entry_point = "bitlut_fractional_build";
        plan->passes[0].groups[0] = plan->workspace_size/128;
        plan->passes[0].threads[0] = 128;
        plan->passes[0].groups[1] = plan->passes[0].groups[2] = 1;
        plan->passes[0].threads[1] = plan->passes[0].threads[2] = 1;
        if (plan->query_kind == SIXEL_GPU_LOOKUP_QUERY_BYTE)
            plan->workspace_size = 1;
        memcpy(plan->palette, entries.entries, plan->count * 3U);
        if (preparer->cached != NULL) {
            bitlut_plan_unref(&preparer->cached->iface);
        }
        preparer->cached = plan;
    }
    bitlut_plan_ref(&plan->iface);
    *result = &plan->iface;
    sixel_mutex_unlock(&preparer->gate);
    return SIXEL_OK;
}

static sixel_gpu_lookup_preparer_vtbl_t const bitlut_vtbl = {
    bitlut_ref, bitlut_unref, bitlut_prepare
};

SIXELSTATUS
sixel_gpu_lookup_bitlut_create(sixel_allocator_t *allocator,
    sixel_lookup_policy_configuration_t const *configuration,
    sixel_gpu_lookup_preparer_interface_t **result)
{
    bitlut_preparer_t *preparer;
#if SIXEL_ENABLE_THREADS
    SIXELSTATUS status;
#endif

    if (allocator == NULL || configuration == NULL || result == NULL)
        return SIXEL_BAD_ARGUMENT;
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
    preparer->iface.vtbl = &bitlut_vtbl;
    preparer->ref = 1U;
    preparer->allocator = allocator;
    preparer->configuration = *configuration;
    sixel_allocator_ref(allocator);
    *result = &preparer->iface;
    return SIXEL_OK;
}
