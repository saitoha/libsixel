/* SPDX-License-Identifier: MIT */
#if defined(HAVE_CONFIG_H)
#include "config.h"
#endif
#include <string.h>
#include "lookup-policy-certlut-gpu.h"
#include "threading.h"
#include "sixel_atomic.h"
/* The readable source is lookup-policy-certlut-gpu.metal. */
static char const * const certlut_chunks[] = {
#include <generated-metal/lookup-policy-certlut-gpu-shader.inc.h>
};
#if defined(HAVE_VULKAN)
/* Readable source: lookup-policy-certlut-gpu.glsl. */
static char const * const certlut_vulkan_chunks[] = {
#include <generated-vulkan/lookup-policy-certlut-gpu-shader.inc.h>
};
#endif

/* RGB8 certlut owns a fixed cell grid and its boundary candidates. The
 * immutable table is built on the GPU, independently of the CPU's lazy
 * pair-only certificates. Its full-competitor affine bounds preserve the
 * exhaustive RGB minimum and the first palette index at a tie.
 *
 * This component owns its shader, cache identity, exclusive workspace lease
 * and completion state. No other lookup depends on its private layout. */
enum {
    CERTLUT_PALETTE_WORDS = 256,
    CERTLUT_CELLS = 4096,
    CERTLUT_CELL_COLORS = 64,
    CERTLUT_WORDS = CERTLUT_PALETTE_WORDS
        + CERTLUT_CELLS * (CERTLUT_CELL_COLORS + 1)
};
typedef struct certlut_plan {
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
    int vulkan;
    int busy;
    int valid;
    sixel_gpu_lookup_build_pass_t passes[2];
} certlut_plan_t;

typedef struct certlut_binding {
    sixel_gpu_lookup_binding_interface_t iface;
    sixel_atomic_u32_t ref;
    certlut_plan_t *plan;
    int finished;
    int rebuild;
} certlut_binding_t;

typedef struct certlut_preparer {
    sixel_gpu_lookup_preparer_interface_t iface;
    sixel_atomic_u32_t ref;
    sixel_allocator_t *allocator;
    sixel_mutex_t gate;
    certlut_plan_t *cached;
} certlut_preparer_t;

static void
certlut_plan_ref(sixel_gpu_lookup_plan_interface_t *self)
{
    certlut_plan_t *plan;

    plan = (certlut_plan_t *)self;
    (void)sixel_atomic_fetch_add_u32(&plan->ref, 1);
}

static void
certlut_plan_unref(sixel_gpu_lookup_plan_interface_t *self)
{
    certlut_plan_t *plan;
    sixel_allocator_t *allocator;

    plan = (certlut_plan_t *)self;
    if (sixel_atomic_fetch_sub_u32(&plan->ref, 1) != 1) return;
    allocator = plan->allocator;
    if (plan->workspace != NULL) plan->workspace->vtbl->unref(plan->workspace);
    plan->device->vtbl->unref(plan->device);
    sixel_mutex_destroy(&plan->gate);
    sixel_allocator_free(allocator, plan);
    sixel_allocator_unref(allocator);
}

static SIXELSTATUS
certlut_describe(sixel_gpu_lookup_plan_interface_t const *self,
    sixel_gpu_lookup_program_view_t *view)
{
    certlut_plan_t const *plan;

    if (view == NULL) return SIXEL_BAD_ARGUMENT;
    plan = (certlut_plan_t const *)self;
    memset(view, 0, sizeof(*view));
    view->program_format = "metal.lookup-rgb8.v1";
    view->logical_name = "certlut";
    view->chunks = certlut_chunks;
    view->chunk_count = sizeof(certlut_chunks) /
        sizeof(certlut_chunks[0]);
#if defined(HAVE_VULKAN)
    /* Both programs retain the same table, constants and build passes. */
    if (plan->vulkan) {
        view->program_format = "vulkan.lookup-rgb8.v1";
        view->chunks = certlut_vulkan_chunks;
        view->chunk_count = sizeof(certlut_vulkan_chunks) /
            sizeof(certlut_vulkan_chunks[0]);
    }
#endif
    view->palette = plan->palette;
    view->palette_size = plan->count * 3U;
    view->ncolors = plan->count;
    view->query_kind = plan->query_kind;
    view->constants = &plan->fractional;
    view->constants_size = sizeof(plan->fractional);
    view->workspace_size = CERTLUT_WORDS * sizeof(uint32_t);
    view->build_passes = plan->passes;
    view->build_pass_count = 2;
    return SIXEL_OK;
}

static void
certlut_binding_ref(sixel_gpu_lookup_binding_interface_t *self)
{
    certlut_binding_t *binding;

    binding = (certlut_binding_t *)self;
    (void)sixel_atomic_fetch_add_u32(&binding->ref, 1);
}

static void
certlut_binding_unref(sixel_gpu_lookup_binding_interface_t *self)
{
    certlut_binding_t *binding;
    certlut_plan_t *plan;

    binding = (certlut_binding_t *)self;
    if (sixel_atomic_fetch_sub_u32(&binding->ref, 1) != 1) return;
    plan = binding->plan;
    /* An unfinished lease intentionally remains busy; unref cannot certify
     * GPU completion or allow another invocation to overwrite its storage. */
    sixel_allocator_free(plan->allocator, binding);
    certlut_plan_unref(&plan->iface);
}

static SIXELSTATUS
certlut_binding_view(sixel_gpu_lookup_binding_interface_t const *self,
    sixel_gpu_lookup_binding_view_t *view)
{
    certlut_binding_t const *binding;

    if (view == NULL) return SIXEL_BAD_ARGUMENT;
    binding = (certlut_binding_t const *)self;
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
certlut_finish(sixel_gpu_lookup_binding_interface_t *self,
    SIXELSTATUS outcome)
{
    certlut_binding_t *binding;
    certlut_plan_t *plan;

    binding = (certlut_binding_t *)self;
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

static sixel_gpu_lookup_binding_vtbl_t const certlut_binding_vtbl = {
    certlut_binding_ref, certlut_binding_unref, certlut_binding_view,
    certlut_finish
};

static SIXELSTATUS
certlut_begin(sixel_gpu_lookup_plan_interface_t *self,
    sixel_gpu_lookup_binding_interface_t **result)
{
    certlut_plan_t *plan;
    certlut_binding_t *binding;
    SIXELSTATUS status;

    if (result == NULL) return SIXEL_BAD_ARGUMENT;
    *result = NULL;
    plan = (certlut_plan_t *)self;
    sixel_mutex_lock(&plan->gate);
    if (plan->busy) {
        sixel_mutex_unlock(&plan->gate);
        return SIXEL_LOGIC_ERROR;
    }
    status = SIXEL_OK;
    if (plan->workspace == NULL) {
        status = plan->device->vtbl->create_buffer(plan->device,
            CERTLUT_WORDS * sizeof(uint32_t), &plan->workspace);
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
    binding->iface.vtbl = &certlut_binding_vtbl;
    binding->ref = 1U;
    binding->plan = plan;
    binding->rebuild = !plan->valid;
    certlut_plan_ref(self);
    plan->busy = 1;
    sixel_mutex_unlock(&plan->gate);
    *result = &binding->iface;
    return SIXEL_OK;
}

static sixel_gpu_lookup_plan_vtbl_t const certlut_plan_vtbl = {
    certlut_plan_ref, certlut_plan_unref, certlut_describe,
    certlut_begin
};

static void
certlut_ref(sixel_gpu_lookup_preparer_interface_t *self)
{
    certlut_preparer_t *preparer;

    preparer = (certlut_preparer_t *)self;
    (void)sixel_atomic_fetch_add_u32(&preparer->ref, 1);
}

static void
certlut_unref(sixel_gpu_lookup_preparer_interface_t *self)
{
    certlut_preparer_t *preparer;
    sixel_allocator_t *allocator;

    preparer = (certlut_preparer_t *)self;
    if (sixel_atomic_fetch_sub_u32(&preparer->ref, 1) != 1) return;
    allocator = preparer->allocator;
    if (preparer->cached != NULL)
        certlut_plan_unref(&preparer->cached->iface);
    sixel_mutex_destroy(&preparer->gate);
    sixel_allocator_free(allocator, preparer);
    sixel_allocator_unref(allocator);
}

static SIXELSTATUS
certlut_prepare(sixel_gpu_lookup_preparer_interface_t const *self,
    sixel_lookup_execution_request_t const *request,
    sixel_gpu_device_interface_t *device,
    sixel_gpu_lookup_plan_interface_t **result)
{
    certlut_preparer_t *preparer;
    certlut_plan_t *plan;
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
    if (request->query_kind != SIXEL_GPU_LOOKUP_QUERY_BYTE &&
            request->query_kind != SIXEL_GPU_LOOKUP_QUERY_FRACTIONAL) {
        return SIXEL_FEATURE_ERROR;
    }
    if (request->pixelformat != SIXEL_PIXELFORMAT_RGB888 ||
            request->colorspace != SIXEL_COLORSPACE_GAMMA ||
            (!vulkan && strcmp(format, "metal.lookup-rgb8.v1") != 0)) {
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
    preparer = (certlut_preparer_t *)self;
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
        plan->iface.vtbl = &certlut_plan_vtbl;
        plan->ref = 1U;
        plan->allocator = preparer->allocator;
        sixel_allocator_ref(plan->allocator);
        plan->device = device;
        device->vtbl->ref(device);
        plan->count = request->ncolors;
        plan->query_kind = request->query_kind;
        plan->vulkan = vulkan;
        /* The constant is a uint32 word on Metal and Vulkan alike,
         * independent of C enum storage. */
        plan->fractional = request->query_kind ==
            SIXEL_GPU_LOOKUP_QUERY_FRACTIONAL;
        memcpy(plan->palette, entries.entries, plan->count * 3U);
        plan->passes[0].entry_point = "certlut_pack";
        plan->passes[0].groups[0] = 8;
        plan->passes[0].threads[0] = 32;
        plan->passes[0].groups[1] = plan->passes[0].groups[2] = 1;
        plan->passes[0].threads[1] = plan->passes[0].threads[2] = 1;
        plan->passes[1].entry_point = "certlut_build";
        plan->passes[1].groups[0] = CERTLUT_CELLS / 32;
        plan->passes[1].threads[0] = 32;
        plan->passes[1].groups[1] = plan->passes[1].groups[2] = 1;
        plan->passes[1].threads[1] = plan->passes[1].threads[2] = 1;
        if (preparer->cached != NULL) {
            certlut_plan_unref(&preparer->cached->iface);
        }
        preparer->cached = plan;
    }
    certlut_plan_ref(&plan->iface);
    *result = &plan->iface;
    sixel_mutex_unlock(&preparer->gate);
    return SIXEL_OK;
}

static sixel_gpu_lookup_preparer_vtbl_t const certlut_vtbl = {
    certlut_ref, certlut_unref, certlut_prepare
};

SIXELSTATUS
sixel_gpu_lookup_certlut_create(sixel_allocator_t *allocator,
    sixel_gpu_lookup_preparer_interface_t **result)
{
    certlut_preparer_t *preparer;
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
    preparer->iface.vtbl = &certlut_vtbl;
    preparer->ref = 1U;
    preparer->allocator = allocator;
    sixel_allocator_ref(allocator);
    *result = &preparer->iface;
    return SIXEL_OK;
}
