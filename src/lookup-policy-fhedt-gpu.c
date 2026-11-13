/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <string.h>
#include <stdio.h>
#include "gpu-palette.h"
#include "lookup-policy-fhedt-gpu.h"
#include "threading.h"
#include "gpu-lookup-metal.h"
#include "sixel_atomic.h"
#if defined(HAVE_METAL) || defined(HAVE_VULKAN)
#include <pthread.h>
#endif
/* Readable source: lookup-policy-fhedt-fractional-gpu.metal. */
static char const * const fhedt_chunks[] = {
#include <generated-metal/lookup-policy-fhedt-fractional-gpu-shader.inc.h>
};
#if defined(HAVE_VULKAN)
/* Readable source: lookup-policy-fhedt-gpu.glsl. Its text is fixed so the
 * build-time program matrix covers it; settings travel as constants. */
static char const * const fhedt_vulkan_chunks[] = {
#include <generated-vulkan/lookup-policy-fhedt-gpu-shader.inc.h>
};
#endif

/* The policy owns its representation, cache key and completion protocol.
 * No other lookup can borrow this cache or depend on its table layout. */
typedef struct fhedt_plan {
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
    char specialization[80];
    char const *chunks[1+sizeof(fhedt_chunks)/sizeof(*fhedt_chunks)];
    size_t workspace_size;
    /* Metal reads constants; the Vulkan program reads vulkan_constants:
     * shift, refine, dist2, resolution and the finished table's word
     * offset in the workspace. */
    uint32_t constants[4];
    uint32_t vulkan_constants[5];
    sixel_gpu_lookup_build_pass_t passes[6];
    size_t pass_count;
    int vulkan;
    int busy;
    int valid;
} fhedt_plan_t;

typedef struct fhedt_binding {
    sixel_gpu_lookup_binding_interface_t iface;
    sixel_atomic_u32_t ref;
    fhedt_plan_t *plan;
    int finished;
    int rebuild;
} fhedt_binding_t;

typedef struct fhedt_preparer {
    sixel_gpu_lookup_preparer_interface_t iface;
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    sixel_mutex_t gate;
    fhedt_plan_t *cached;
    sixel_lookup_policy_configuration_t configuration;
} fhedt_preparer_t;

/* This policy always recycles through its process-wide idle cache. CPU
 * shared-instance settings never disable it. Active bindings retain exclusive
 * workspaces; neither another palette nor another in-flight frame can
 * overwrite them. One idle allocation bounds the retained cache capacity. */
#if defined(HAVE_METAL) || defined(HAVE_VULKAN)
static pthread_mutex_t fhedt_idle_gate = PTHREAD_MUTEX_INITIALIZER;
static struct {
    sixel_gpu_device_interface_t *device;
    sixel_gpu_buffer_interface_t *workspace;
    unsigned char palette[768];
    unsigned int count;
    uint32_t constants[4];
    uint32_t vulkan_constants[5];
    int valid;
} fhedt_idle;
#endif

/* Metal byte tables belong to this policy's native implementation. */
static int
fhedt_owns_table(fhedt_plan_t const *plan)
{
    return plan->vulkan || plan->query_kind != SIXEL_GPU_LOOKUP_QUERY_BYTE;
}

static void
fhedt_borrow(fhedt_plan_t *plan)
{
#if defined(HAVE_METAL) || defined(HAVE_VULKAN)
    sixel_gpu_device_interface_t *device;

    if (!fhedt_owns_table(plan)) return;
    device = NULL;
    (void)pthread_mutex_lock(&fhedt_idle_gate);
    if (fhedt_idle.device == plan->device &&
            fhedt_idle.workspace->vtbl->size(fhedt_idle.workspace)
                >= plan->workspace_size) {
        plan->workspace = fhedt_idle.workspace;
        plan->valid = fhedt_idle.valid && fhedt_idle.count == plan->count &&
            memcmp(fhedt_idle.constants, plan->constants,
                sizeof(plan->constants)) == 0 &&
            memcmp(fhedt_idle.vulkan_constants, plan->vulkan_constants,
                sizeof(plan->vulkan_constants)) == 0 &&
            memcmp(fhedt_idle.palette, plan->palette, plan->count*3U) == 0;
        device = fhedt_idle.device;
        fhedt_idle.device = NULL;
        fhedt_idle.workspace = NULL;
    }
    (void)pthread_mutex_unlock(&fhedt_idle_gate);
    if (device != NULL) device->vtbl->unref(device);
#else
    (void)plan;
#endif
}

static void
fhedt_recycle(fhedt_plan_t *plan)
{
#if defined(HAVE_METAL) || defined(HAVE_VULKAN)
    sixel_gpu_device_interface_t *device;
    sixel_gpu_buffer_interface_t *workspace;

    if (!fhedt_owns_table(plan) || plan->workspace == NULL ||
            plan->busy) return;
    plan->device->vtbl->ref(plan->device);
    (void)pthread_mutex_lock(&fhedt_idle_gate);
    device = fhedt_idle.device;
    workspace = fhedt_idle.workspace;
    fhedt_idle.device = plan->device;
    fhedt_idle.workspace = plan->workspace;
    fhedt_idle.count = plan->count;
    fhedt_idle.valid = plan->valid;
    memcpy(fhedt_idle.constants, plan->constants, sizeof(plan->constants));
    memcpy(fhedt_idle.vulkan_constants, plan->vulkan_constants,
        sizeof(plan->vulkan_constants));
    memcpy(fhedt_idle.palette, plan->palette, plan->count*3U);
    plan->workspace = NULL;
    plan->valid = 0;
    (void)pthread_mutex_unlock(&fhedt_idle_gate);
    if (workspace != NULL) workspace->vtbl->unref(workspace);
    if (device != NULL) device->vtbl->unref(device);
#else
    (void)plan;
#endif
}

static void
fhedt_plan_ref(sixel_gpu_lookup_plan_interface_t *self)
{
    fhedt_plan_t *plan;

    plan = (fhedt_plan_t *)self;
    (void)sixel_atomic_fetch_add_u32(&plan->ref, 1);
}

static void
fhedt_plan_unref(sixel_gpu_lookup_plan_interface_t *self)
{
    fhedt_plan_t *plan;
    sixel_allocator_t *allocator;

    plan = (fhedt_plan_t *)self;
    if (sixel_atomic_fetch_sub_u32(&plan->ref, 1) != 1) return;
    allocator = plan->allocator;
    fhedt_recycle(plan);
    if (plan->workspace != NULL) plan->workspace->vtbl->unref(plan->workspace);
    plan->device->vtbl->unref(plan->device);
    sixel_mutex_destroy(&plan->gate);
    sixel_allocator_free(allocator, plan);
    sixel_allocator_unref(allocator);
}

static SIXELSTATUS
fhedt_describe(sixel_gpu_lookup_plan_interface_t const *self,
    sixel_gpu_lookup_program_view_t *view)
{
    fhedt_plan_t const *plan;

    if (view == NULL) return SIXEL_BAD_ARGUMENT;
    plan = (fhedt_plan_t const *)self;
    memset(view, 0, sizeof(*view));
#if defined(HAVE_VULKAN)
    if (plan->vulkan) {
        view->program_format = "vulkan.lookup-rgb8.v1";
        view->logical_name = "fhedt";
        view->query_kind = plan->query_kind;
        view->chunks = fhedt_vulkan_chunks;
        view->chunk_count = sizeof(fhedt_vulkan_chunks) /
            sizeof(fhedt_vulkan_chunks[0]);
        view->palette = plan->palette;
        view->palette_size = plan->count * 3U;
        view->ncolors = plan->count;
        view->workspace_size = plan->workspace_size;
        view->constants = plan->vulkan_constants;
        view->constants_size = sizeof(plan->vulkan_constants);
        view->build_passes = plan->passes;
        view->build_pass_count = plan->pass_count;
        return SIXEL_OK;
    }
#endif
    /* Byte consumers receive this policy's native descriptor. Fractional
     * programs, builders and workspace are owned by this policy alone. */
    if (plan->query_kind == SIXEL_GPU_LOOKUP_QUERY_BYTE) {
        view->program_format = "metal.lookup-native-rgb8.v1";
        view->logical_name = "fhedt";
        view->palette = plan->palette;
        view->palette_size = plan->count * 3U;
        view->ncolors = plan->count;
        view->constants = &plan->metal;
        view->constants_size = sizeof(plan->metal);
        view->workspace_size = 1;
        return SIXEL_OK;
    }
    view->program_format = "metal.lookup-rgb8.v1";
    view->logical_name = "fhedt";
    view->query_kind = plan->query_kind;
    view->chunks = plan->chunks;
    view->chunk_count = sizeof(plan->chunks)/sizeof(*plan->chunks);
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
fhedt_binding_ref(sixel_gpu_lookup_binding_interface_t *self)
{
    fhedt_binding_t *binding;

    binding = (fhedt_binding_t *)self;
    (void)sixel_atomic_fetch_add_u32(&binding->ref, 1);
}

static void
fhedt_binding_unref(sixel_gpu_lookup_binding_interface_t *self)
{
    fhedt_binding_t *binding;
    fhedt_plan_t *plan;

    binding = (fhedt_binding_t *)self;
    if (sixel_atomic_fetch_sub_u32(&binding->ref, 1) != 1) return;
    plan = binding->plan;
    /* An unfinished lease intentionally remains busy; unref cannot certify
     * GPU completion or allow another invocation to overwrite its storage. */
    sixel_allocator_free(plan->allocator, binding);
    fhedt_plan_unref(&plan->iface);
}

static SIXELSTATUS
fhedt_binding_view(sixel_gpu_lookup_binding_interface_t const *self,
    sixel_gpu_lookup_binding_view_t *view)
{
    fhedt_binding_t const *binding;

    if (view == NULL) return SIXEL_BAD_ARGUMENT;
    binding = (fhedt_binding_t const *)self;
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
fhedt_finish(sixel_gpu_lookup_binding_interface_t *self, SIXELSTATUS outcome)
{
    fhedt_binding_t *binding;
    fhedt_plan_t *plan;

    binding = (fhedt_binding_t *)self;
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
    if (outcome == SIXEL_OK) fhedt_recycle(plan);
    sixel_mutex_unlock(&plan->gate);
    return SIXEL_OK;
}

static sixel_gpu_lookup_binding_vtbl_t const fhedt_binding_vtbl = {
    fhedt_binding_ref, fhedt_binding_unref,
    fhedt_binding_view, fhedt_finish
};

static SIXELSTATUS
fhedt_begin(sixel_gpu_lookup_plan_interface_t *self,
    sixel_gpu_lookup_binding_interface_t **result)
{
    fhedt_plan_t *plan;
    fhedt_binding_t *binding;
    SIXELSTATUS status;

    if (result == NULL) return SIXEL_BAD_ARGUMENT;
    *result = NULL;
    plan = (fhedt_plan_t *)self;
    sixel_mutex_lock(&plan->gate);
    if (plan->busy) {
        sixel_mutex_unlock(&plan->gate);
        return SIXEL_LOGIC_ERROR;
    }
    status = SIXEL_OK;
    if (plan->workspace == NULL) fhedt_borrow(plan);
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
    binding->iface.vtbl = &fhedt_binding_vtbl;
    binding->ref = 1U;
    binding->plan = plan;
    binding->rebuild = !plan->valid;
    fhedt_plan_ref(self);
    plan->busy = 1;
    sixel_mutex_unlock(&plan->gate);
    *result = &binding->iface;
    return SIXEL_OK;
}

static sixel_gpu_lookup_plan_vtbl_t const fhedt_plan_vtbl = {
    fhedt_plan_ref, fhedt_plan_unref, fhedt_describe, fhedt_begin
};

static void
fhedt_ref(sixel_gpu_lookup_preparer_interface_t *self)
{
    fhedt_preparer_t *preparer;

    preparer = (fhedt_preparer_t *)self;
    (void)sixel_atomic_fetch_add_u32(&preparer->ref, 1);
}

static void
fhedt_unref(sixel_gpu_lookup_preparer_interface_t *self)
{
    fhedt_preparer_t *preparer;
    sixel_allocator_t *allocator;

    preparer = (fhedt_preparer_t *)self;
    if (sixel_atomic_fetch_sub_u32(&preparer->ref, 1) != 1) return;
    allocator = preparer->allocator;
    if (preparer->cached != NULL) fhedt_plan_unref(&preparer->cached->iface);
    sixel_mutex_destroy(&preparer->gate);
    sixel_allocator_free(allocator, preparer);
    sixel_allocator_unref(allocator);
}

#if defined(HAVE_VULKAN)
/* One 64-wide dispatch covering count invocations, folded into a second
 * group dimension past 65535 groups; the entries flatten both. */
static void
fhedt_vulkan_pass(sixel_gpu_lookup_build_pass_t *pass, char const *entry,
    size_t count)
{
    size_t groups;

    groups = (count + 63U) / 64U;
    pass->entry_point = entry;
    pass->groups[0] = groups < 65535U ? groups : 65535U;
    pass->groups[1] = (groups + pass->groups[0] - 1U) / pass->groups[0];
    pass->groups[2] = 1;
    pass->threads[0] = 64;
    pass->threads[1] = pass->threads[2] = 1;
}

/* The Vulkan program's layout, described in lookup-policy-fhedt-gpu.glsl.
 * Byte queries at R256 are exact lattice points: their stored label is
 * already nearest and corner refinement would visit it first with a strict
 * comparison, so byte R256 skips the boundary table without changing ties.
 * Fractional R256 still needs refinement between lattice points. */
static void
fhedt_vulkan_layout(fhedt_plan_t *plan)
{
    size_t r, cells;
    int refine;

    r = (size_t)plan->configuration.fhedt_resolution;
    cells = r * r * r;
    refine = plan->configuration.fhedt_refine &&
        (plan->query_kind != SIXEL_GPU_LOOKUP_QUERY_BYTE || r != 256);
    plan->vulkan_constants[0] = r == 64 ? 2 : r == 128 ? 1 : 0;
    plan->vulkan_constants[1] = (uint32_t)refine;
    plan->vulkan_constants[2] = (uint32_t)plan->configuration.fhedt_use_dist2;
    plan->vulkan_constants[3] = (uint32_t)r;
    plan->vulkan_constants[4] = (uint32_t)(r * r + 256U +
        (refine ? cells : 0U));
    plan->workspace_size = (r * r + 256U + 2U * cells) * sizeof(uint32_t);
    fhedt_vulkan_pass(&plan->passes[0], "fhedt_clear", r * r);
    fhedt_vulkan_pass(&plan->passes[1], "fhedt_seed", plan->count);
    fhedt_vulkan_pass(&plan->passes[2], "fhedt_x", cells);
    fhedt_vulkan_pass(&plan->passes[3], "fhedt_y", r * r);
    fhedt_vulkan_pass(&plan->passes[4], "fhedt_z", r * r);
    fhedt_vulkan_pass(&plan->passes[5], "fhedt_pack", cells);
    plan->pass_count = refine ? 6 : 5;
}
#endif

static SIXELSTATUS
fhedt_prepare(sixel_gpu_lookup_preparer_interface_t const *self,
    sixel_lookup_execution_request_t const *request,
    sixel_gpu_device_interface_t *device,
    sixel_gpu_lookup_plan_interface_t **result)
{
    fhedt_preparer_t *preparer;
    fhedt_plan_t *plan;
    sixel_palette_entries_view_t entries;
    sixel_gpu_palette_request_t admission;
    size_t r, cells, i;
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
    preparer = (fhedt_preparer_t *)self;
    memset(&admission, 0, sizeof(admission));
    sixel_gpu_palette_set_lookup_configuration(&admission,
        &preparer->configuration);
    if (!sixel_gpu_palette_fhedt_is_supported(&admission))
        return SIXEL_FEATURE_ERROR;
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
        plan->iface.vtbl = &fhedt_plan_vtbl;
        plan->ref = 1U;
        plan->allocator = preparer->allocator;
        sixel_allocator_ref(plan->allocator);
        plan->device = device;
        device->vtbl->ref(device);
        plan->count = request->ncolors;
        plan->query_kind = request->query_kind;
        plan->metal.configuration = &plan->configuration;
        plan->metal.implementation = sixel_lookup_fhedt_gpu_metal();
        plan->configuration = preparer->configuration;
        r = (size_t)plan->configuration.fhedt_resolution;
        cells = r*r*r;
        plan->constants[0] = r == 64 ? 2 : r == 128 ? 1 : 0;
        plan->constants[1] = (uint32_t)plan->configuration.fhedt_refine;
        plan->constants[2] = (uint32_t)plan->configuration.fhedt_use_dist2;
        /* Fractional R256 still needs refinement between lattice points. */
        (void)snprintf(plan->specialization, sizeof(plan->specialization),
            "#define R %u\n#define REFINE %u\n#define PACKED 1\n",
            (unsigned int)r, plan->constants[1]);
        plan->chunks[0] = plan->specialization;
        for (i = 0; i < sizeof(fhedt_chunks)/sizeof(*fhedt_chunks); ++i)
            plan->chunks[i+1] = fhedt_chunks[i];
        plan->workspace_size = (r*r+256+2*cells)*sizeof(uint32_t)
            +cells*(plan->constants[1] ? 2 : 1);
        plan->pass_count = plan->constants[1] ? 6 : 5;
        for (i = 0; i < plan->pass_count; ++i) {
            plan->passes[i].groups[1] = plan->passes[i].groups[2] = 1;
            plan->passes[i].threads[1] = plan->passes[i].threads[2] = 1;
            plan->passes[i].threads[0] = 32;
        }
        plan->passes[0].entry_point = "fhedt_clear";
        plan->passes[0].groups[0] = r*r/32;
        plan->passes[1].entry_point = "fhedt_seed";
        plan->passes[1].groups[0] = (plan->count+31)/32;
        plan->passes[2].entry_point = "fhedt_x";
        plan->passes[2].groups[0] = cells/32;
        plan->passes[3].entry_point = "fhedt_y";
        plan->passes[3].groups[0] = r*r/32;
        plan->passes[4].entry_point = "fhedt_z";
        plan->passes[4].groups[0] = r*r/32;
        plan->passes[5].entry_point = "fhedt_pack";
        plan->passes[5].groups[0] = cells/32;
        if (plan->query_kind == SIXEL_GPU_LOOKUP_QUERY_BYTE)
            plan->workspace_size = 1;
        plan->vulkan = vulkan;
#if defined(HAVE_VULKAN)
        if (vulkan) fhedt_vulkan_layout(plan);
#endif
        memcpy(plan->palette, entries.entries, plan->count * 3U);
        if (preparer->cached != NULL) {
            fhedt_plan_unref(&preparer->cached->iface);
        }
        preparer->cached = plan;
    }
    fhedt_plan_ref(&plan->iface);
    *result = &plan->iface;
    sixel_mutex_unlock(&preparer->gate);
    return SIXEL_OK;
}

static sixel_gpu_lookup_preparer_vtbl_t const fhedt_vtbl = {
    fhedt_ref, fhedt_unref, fhedt_prepare
};

SIXELSTATUS
sixel_gpu_lookup_fhedt_create(sixel_allocator_t *allocator,
    sixel_lookup_policy_configuration_t const *configuration,
    sixel_gpu_lookup_preparer_interface_t **result)
{
    fhedt_preparer_t *preparer;
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
    preparer->iface.vtbl = &fhedt_vtbl;
    preparer->ref = 1U;
    preparer->allocator = allocator;
    preparer->configuration = *configuration;
    sixel_allocator_ref(allocator);
    *result = &preparer->iface;
    return SIXEL_OK;
}
